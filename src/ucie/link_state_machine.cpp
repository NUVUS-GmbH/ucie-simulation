#include "sim/modules.hpp"

namespace ucie {

LinkStateMachine::LinkStateMachine(sc_core::sc_module_name name, World& world) : sc_module(name), w_(world) {
  SC_THREAD(run);
}

void LinkStateMachine::reactivate() {
  // [MODEL] Reactivation delay is configured. It is not a UCIe training time.
  w_.set_state(LinkState::Recovering);
  w_.log(TraceLevel::Summary, "reactivating", "Link", nullptr, "[MODEL] link reactivation");
  sc_core::wait(w_.cfg.reactivation_ns, sc_core::SC_NS);
  if (!w_.link.remote_recovers) {
    w_.link.converged = false;
    w_.log(TraceLevel::Summary, "remote_down", "Link", nullptr, "remote endpoint stays unresponsive");
    return;
  }
  w_.reinit_credits();
  w_.link.aou_a = AouState::Enabled;
  w_.link.aou_b = AouState::Enabled;
  w_.set_state(LinkState::Active);
  w_.link.converged = true;
  w_.link.recovery_set = true;
  w_.link.recovery_time_ns = w_.now_ns();
  for (std::size_t i = 0; i < w_.txns.size(); ++i) {
    auto& tx = w_.txns[i];
    if (!tx.pending_reissue) continue;
    tx.pending_reissue = false;
    ++tx.software_retry_count;
    if (tx.axi5.enabled) {
      ++tx.axi5.transfer_attempt;
      tx.axi5.atomic_effect_applied = false;
      tx.axi5.write_response_created = false;
      tx.axi5.read_response_created = false;
      // value_before/after / atomic_effects accumulate across attempts (GT).
    }
    // ERR-07 / AutoRetry: reopen the logical transaction for a fresh attempt.
    // Prior SLVERR/OKAY fate remains in observer traces; protocol state resets.
    tx.visible.classified = false;
    tx.visible.axi_completed_ok = false;
    tx.visible.axi_completed_err = false;
    tx.visible.axi_accepted = false;
    tx.visible.axi_resp = AxiResp::Okay;
    tx.visible.axi_resp_source = RespSource::None;
    tx.visible.handed_to_ucie = false;
    tx.visible.data_attempted = false;
    tx.visible.request_ack = false;
    tx.visible.data_ack = false;
    tx.visible.response_ack = false;
    tx.visible.request_nak_only = false;
    tx.visible.data_nak_only = false;
    tx.visible.response_nak_only = false;
    tx.visible.request_timeout = false;
    tx.visible.data_timeout = false;
    tx.visible.response_timeout = false;
    tx.visible.response_at_local_aou = false;
    tx.visible.stopped = false;
    tx.visible.txn_watchdog_fired = false;
    tx.completion_received = false;
    tx.bridge.local_closed = false;
    ++tx.bridge.generation;
    const int saved_burst = std::max(1, tx.proto.burst_len);
    tx.proto = AxiProtocolRecord{};
    tx.proto.burst_len = saved_burst;
    tx.t_watchdog_arm_ns = -1;
    tx.t_watchdog_fire_ns = -1;
    tx.phase = Phase::Created;
    tx.recovery_epoch = static_cast<int>(w_.link.epoch);
    if (tx.axis.enabled) {
      // Preserve packet description; clear transfer / reception / effect staging.
      // Architectural ground.commit_count is intentionally NOT cleared.
      const auto offers = tx.axis.offers;
      const auto sink = tx.axis.sink;
      const auto beats = tx.axis.beat_count;
      const auto last_vb = tx.axis.last_beat_valid_bytes;
      const auto tid = tx.axis.tid;
      const auto tdest = tx.axis.tdest;
      const auto tuser = tx.axis.tuser;
      const auto app_ack = tx.axis.app_ack_mode;
      const auto pkt_id = tx.axis.packet_op_id;
      const bool pkt_id_en = tx.axis.packet_id_enabled;
      const int width = tx.axis.data_width_bytes;
      tx.axis = AxisRecord{};
      tx.axis.enabled = true;
      tx.axis.sink = sink;
      tx.axis.beat_count = beats;
      tx.axis.last_beat_valid_bytes = last_vb;
      tx.axis.data_width_bytes = width;
      tx.axis.tid = tid;
      tx.axis.tdest = tdest;
      tx.axis.tuser = tuser;
      tx.axis.offers = offers;
      tx.axis.app_ack_mode = app_ack;
      tx.axis.packet_op_id = pkt_id;
      tx.axis.packet_id_enabled = pkt_id_en;
      tx.axis.continuous_packets = true;
    }
    if (i < w_.partial.size()) w_.partial[i] = Partial{};
    w_.reissue.push_back(static_cast<std::uint32_t>(i));
  }
  w_.log(TraceLevel::Summary, "active", "Link", nullptr, "link is Active again");
  w_.progress.notify(sc_core::SC_ZERO_TIME);
}

void LinkStateMachine::enter_link_error(const std::string& reason) {
  // [SPEC] UCIe 2.0 §10.3.3.7 LinkError. In-flight packets may be dropped.
  // Minimum residency defaults to 16 ms so a partner that missed the sideband
  // message still enters LinkError. §10.3.3.5 / §10.3.3.7 flush pipelines and
  // retry buffers. §10.3.4: the adapter does not guarantee protocol quiescence.
  w_.capture_recovery_snapshot("LinkError", "SPEC");
  w_.record_error("LinkError", nullptr, reason);
  ++w_.link.linkerror_events;
  w_.link.destructive = true;
  w_.in_l1 = false;
  w_.set_state(LinkState::LinkError);
  w_.link.link_error_time_ns = w_.now_ns();
  w_.log(TraceLevel::Summary, "link_error", "Link", nullptr, "LinkError entered");
  w_.log(TraceLevel::Summary, "link_error", "Link", nullptr, reason);
  w_.observe_link_down();
  SidebandRecord msg;
  msg.priority = true;
  msg.reason = "LinkError";
  msg.t_gen_ns = w_.now_ns();
  w_.sb_out.push(msg);
  w_.sideband_peak = std::max(w_.sideband_peak, w_.sb_out.size());
  w_.log(TraceLevel::Summary, "residency", "Link", nullptr,
         "[SPEC] LinkError minimum residency elapses (UCIe 2.0 10.3.3.7)");
  sc_core::wait(w_.cfg.link_error_residency_ns, sc_core::SC_NS);
  w_.link.aou_a = AouState::Disabled;
  w_.link.aou_b = AouState::Disabled;
  w_.log(TraceLevel::Summary, "aou", "AoU", nullptr,
         "[SPEC] AoU activation FSM returns to DISABLED on FDI link-down");
  w_.flush_destructive();
  if (w_.cfg.axis_dup_clear_on_reset) w_.target.axis_dup_on_reset();
  w_.apply_policy();
  // Local SLVERR now traverses the AXI READY/beat path asynchronously. Drain
  // those handshakes (and ERR-07 arming) before reactivation/reissue.
  while (true) {
    bool pending = false;
    for (const auto& tx : w_.txns) {
      if (tx.bridge.pending_local_slverr) pending = true;
    }
    if (!pending) break;
    sc_core::wait(w_.progress);
  }
  if (w_.link.remote_recovers) reactivate();
  else w_.link.converged = false;
}

void LinkStateMachine::enter_link_reset() {
  // [SPEC] UCIe 2.0 §10.3.3.5. Best-effort drain must not block entry.
  // Pipelines and retry buffers are flushed once the state is entered.
  w_.capture_recovery_snapshot("LinkReset", "SPEC");
  w_.record_error("LinkReset", nullptr, "LinkReset entered");
  ++w_.link.linkreset_events;
  w_.link.destructive = true;
  w_.in_l1 = false;
  w_.set_state(LinkState::LinkReset);
  w_.log(TraceLevel::Summary, "link_reset", "Link", nullptr, "LinkReset entered");
  w_.observe_link_down();
  sc_core::wait(w_.cfg.link_reset_ns, sc_core::SC_NS);
  w_.link.aou_a = AouState::Disabled;
  w_.link.aou_b = AouState::Disabled;
  w_.flush_destructive();
  w_.apply_policy();
  while (true) {
    bool pending = false;
    for (const auto& tx : w_.txns) {
      if (tx.bridge.pending_local_slverr) pending = true;
    }
    if (!pending) break;
    sc_core::wait(w_.progress);
  }
  w_.set_state(LinkState::Reset);
  reactivate();
}

void LinkStateMachine::do_retrain() {
  // [SPEC] UCIe 2.0 §10.3.3.4. The CRC threshold that provokes Retrain is
  // implementation specific. Retrain is not the FDI link-down event that
  // returns AoU to DISABLED.
  w_.capture_recovery_snapshot("Retrain", w_.cfg.retrain_preserves_retry ? "POLICY" : "POLICY");
  w_.record_error("Retrain", nullptr, "Retrain entered");
  ++w_.link.retrain_events;
  w_.in_l1 = false;
  w_.set_state(LinkState::Retrain);
  w_.log(TraceLevel::Summary, "retrain", "Link", nullptr, "UCIe enters Retrain");
  w_.log(TraceLevel::Summary, "retrain", "AoU", nullptr,
         "AoU remains enabled across Retrain (FDI link-down is not signaled)");
  sc_core::wait(w_.cfg.retrain_ns, sc_core::SC_NS);
  if (w_.link.fail_retrain) {
    w_.log(TraceLevel::Summary, "retrain_fail", "Link", nullptr, "Recovery fails");
    enter_link_error("Retrain did not restore the link");
    return;
  }
  // [POLICY] Successful Retrain ends the modeled PHY corruption. Survival of
  // retry-buffer payload is retrain_preserves_retry, not a stated flush rule.
  w_.arm.persistent = false;
  w_.arm.oneshot = 0;
  w_.link.streak[0] = 0;
  w_.link.streak[1] = 0;
  w_.log(TraceLevel::Summary, "retrain_done", "Link", nullptr, "Retrain completes and both adapters return to Active");
  if (!w_.link.preserve_retry) {
    std::vector<std::uint64_t> dropped;
    for (int side = 0; side < 2; ++side) {
      for (const auto& flit : w_.retry[side].snapshot()) dropped.push_back(flit.logical_id);
      w_.retry[side].flush();
      w_.replay_q[side].flush();
      w_.wire[side].flush();
      w_.arrive[side].flush();
    }
    ++w_.link.wire_gen;
    w_.log(TraceLevel::Summary, "retrain_drop", "UCIe", nullptr,
           "[POLICY] retry payload discarded on Retrain");
    for (auto id : dropped) {
      if (auto* tx = w_.find(id)) {
        if (!tx->visible.classified && !tx->visible.axi_completed_ok) {
          w_.deadlock = true;
          w_.abandon(*tx, false, false);
        }
      }
    }
  } else {
    w_.link.resend[0] = true;
    w_.link.resend[1] = true;
  }
  w_.set_state(LinkState::Active);
  w_.link.converged = true;
}

void LinkStateMachine::emergency() {
  // [SPEC] Emergency Shutdown is fatal and is left by reset. No outstanding
  // transaction log is defined (UCIe 3.0 §8.4.2.2, as used by the study).
  // [POLICY] This model does not invent a completion or a best-effort drain.
  // [MODEL] After the reset it boots the link again so a later transaction in
  // the same campaign can run. That boot is not a UCIe completion.
  w_.link.emergency = true;
  w_.link.destructive = true;
  w_.log(TraceLevel::Summary, "emergency", "Safety", nullptr,
         "Emergency Shutdown asserted. No standardized outstanding-transaction log is defined");
  w_.set_state(LinkState::Reset);
  w_.observe_link_down();
  SidebandRecord msg;
  msg.priority = true;
  msg.reason = "EmergencyShutdown";
  msg.t_gen_ns = w_.now_ns();
  w_.sb_out.push(msg);
  w_.link.aou_a = AouState::Disabled;
  w_.link.aou_b = AouState::Disabled;
  w_.flush_destructive();
  w_.apply_policy();
  reactivate();
}

void LinkStateMachine::run() {
  w_.set_state(LinkState::Reset);
  w_.link.aou_a = AouState::Disabled;
  w_.link.aou_b = AouState::Disabled;
  sc_core::wait(w_.cfg.reactivation_ns, sc_core::SC_NS);
  w_.reinit_credits();
  w_.link.aou_a = AouState::Enabled;
  w_.link.aou_b = AouState::Enabled;
  w_.set_state(LinkState::Active);
  w_.link.converged = true;
  w_.up_ev.notify(sc_core::SC_ZERO_TIME);
  w_.log(TraceLevel::Summary, "up", "Link", nullptr, "link is Active");

  while (true) {
    if (!w_.link.cmd_pending) sc_core::wait(w_.link.cmd_ev);
    const auto cmd = w_.link.cmd;
    w_.link.cmd_pending = false;
    w_.link.busy = true;
    switch (cmd) {
      case RecoveryCmd::Retrain: do_retrain(); break;
      case RecoveryCmd::LinkReset: enter_link_reset(); break;
      case RecoveryCmd::LinkError: enter_link_error("Forced LinkError"); break;
      case RecoveryCmd::EmergencyShutdown: emergency(); break;
      case RecoveryCmd::RemoteUnresponsive:
        w_.link.remote_recovers = false;
        enter_link_error("[SPEC] no sideband response; local side enters LinkError (UCIe 2.0 10.3.3.8)");
        break;
      case RecoveryCmd::EnterL1: {
        // [REFERENCE-SLIDE] L1 enabled. Entry/exit dwells are SIMULATION CONFIGURATION.
        // [ASSUMPTION] Retry buffer and credits are preserved across L1 (no flush).
        // The modeled cycle is entry → hold → exit in one command so the single
        // recovery-command slot is not overwritten mid-residency.
        if (!w_.cfg.l1_enabled) break;
        ++w_.l1_events;
        w_.record_error("L1_entry", nullptr, "Entering L1");
        w_.log(TraceLevel::Summary, "l1_entry", "Link", nullptr, "[MODEL] L1 entry");
        sc_core::wait(w_.cfg.l1_entry_ns, sc_core::SC_NS);
        w_.in_l1 = true;
        w_.set_state(LinkState::L1);
        sc_core::wait(w_.cfg.l1_hold_ns, sc_core::SC_NS);
        w_.record_error("L1_exit", nullptr, "Exiting L1");
        w_.log(TraceLevel::Summary, "l1_exit", "Link", nullptr, "[MODEL] L1 exit");
        sc_core::wait(w_.cfg.l1_exit_ns, sc_core::SC_NS);
        w_.in_l1 = false;
        w_.set_state(LinkState::Active);
        break;
      }
      case RecoveryCmd::ExitL1: {
        // Explicit exit if a test left the link in L1 without the combined cycle.
        if (!w_.cfg.l1_enabled) break;
        w_.record_error("L1_exit", nullptr, "Exiting L1");
        w_.log(TraceLevel::Summary, "l1_exit", "Link", nullptr, "[MODEL] L1 exit");
        sc_core::wait(w_.cfg.l1_exit_ns, sc_core::SC_NS);
        w_.in_l1 = false;
        w_.set_state(LinkState::Active);
        break;
      }
      case RecoveryCmd::None: break;
    }
    if (w_.link.cmd_pending) continue;
    w_.link.busy = false;
    w_.link.destructive = false;
    w_.link.idle_ev.notify(sc_core::SC_ZERO_TIME);
    w_.progress.notify(sc_core::SC_ZERO_TIME);
  }
}

}  // namespace ucie
