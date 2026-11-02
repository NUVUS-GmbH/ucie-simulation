#include "sim/modules.hpp"

namespace ucie {
namespace {

void arm_err07_after_local(World& w, Transaction& tx) {
  if (tx.software_retry_count >= 1) return;
  if (w.cfg.master_retry == MasterRetryPolicy::BlindRetry) {
    tx.pending_reissue = true;
    tx.ground.commit_at_reissue = tx.ground.architectural_commit_count;
    w.log(TraceLevel::Transactions, "err07_blind_retry", "Master", &tx,
          "[PROJECT ERR-07] BlindRetry armed after SLVERR");
    return;
  }
  if (w.cfg.master_retry != MasterRetryPolicy::RetryOnlyIfNotCompleted) return;

  // Safe retry requires NOT_COMPLETED *and* that the old attempt cannot still
  // execute later (cancel_remote) or that target duplicate suppression exists.
  const bool not_completed =
      tx.visible.fate_d == Fate::NotCompleted || tx.visible.fate_c == Fate::NotCompleted ||
      tx.visible.fate_a == Fate::NotCompleted;
  if (!not_completed) {
    w.log(TraceLevel::Transactions, "err07_safe_retry_blocked", "Master", &tx,
          "[PROJECT ERR-07] RetryOnlyIfNotCompleted: classifier not NOT_COMPLETED");
    return;
  }
  // Instant cancel (latency 0) is a MODEL ASSUMPTION, not a confirmed wire cancel.
  if (w.remote_may_continue(tx) && !w.cfg.target_duplicate_suppress && !w.cfg.axis_dup_suppress) {
    w.log(TraceLevel::Transactions, "err07_safe_retry_blocked", "Master", &tx,
          "[PROJECT ERR-07] RetryOnlyIfNotCompleted: remote may still execute (no cancel/dup-suppress)");
    return;
  }
  tx.pending_reissue = true;
  tx.ground.commit_at_reissue = tx.ground.architectural_commit_count;
  w.log(TraceLevel::Transactions, "err07_safe_retry", "Master", &tx,
        "[PROJECT ERR-07] RetryOnlyIfNotCompleted armed (remote cancelled or dup-suppress)");
}

bool axis_accept_packet(World& w, Transaction& tx, std::uint32_t slot, std::uint64_t epoch) {
  // [SPECIFICATION FACT] IHI0051B §2.2: transfer = TVALID ∧ TREADY.
  // Transmitter must not wait for TREADY before TVALID; once TVALID is high,
  // payload is held stable until the handshake.
  tx.visible.axi_accepted = true;
  tx.proto.state = AxiProtoState::Outstanding;
  tx.proto.t_accepted_ns = w.now_ns();
  w.note(tx, Phase::AxiAccepted, "AXIS", "AXIS manager begins packet T" + std::to_string(tx.logical_id));
  w.record_observer_event(tx, "ACCEPT");
  w.arm_txn_watchdog(tx);
  // Phase faults (e.g. loss-before-first) may arm LinkError synchronously in note().
  if (tx.visible.classified || w.link.destructive || w.link.epoch != epoch) {
    if (!tx.visible.classified) {
      w.stream_local_abort(tx, RespSource::Bridge,
                           "[MODEL] AXIS abort immediately after accept (pre-transfer)");
    }
    return false;
  }

  std::uint8_t packet_tid = tx.axis.tid;
  std::uint8_t packet_tdest = tx.axis.tdest;

  for (int b = 0; b < tx.axis.beat_count; ++b) {
    if (tx.visible.classified || tx.axis.stream_aborted) return false;
    if (w.link.epoch != epoch || w.link.destructive) {
      if (!tx.visible.classified) {
        w.stream_local_abort(tx, RespSource::Bridge,
                             "[MODEL] AXIS local abort on link epoch/destructive during offer");
      }
      return false;
    }
    auto& offer = tx.axis.offers[static_cast<std::size_t>(b)];
    if (tx.axis.continuous_packets) {
      if (b == 0) {
        packet_tid = offer.tid;
        packet_tdest = offer.tdest;
      } else if (offer.tid != packet_tid || offer.tdest != packet_tdest) {
        tx.axis.protocol_error = true;
        tx.axis.protocol_error_reason = "Continuous_Packets TID/TDEST changed mid-packet";
        w.log(TraceLevel::Errors, "axis_protocol_error", "AXIS", &tx, tx.axis.protocol_error_reason);
        w.record_observer_event(tx, "AXI:STREAM_PROTOCOL_ERROR");
        w.stream_local_abort(tx, RespSource::Bridge, tx.axis.protocol_error_reason);
        return false;
      }
      if (!offer.tlast && offer.tkeep != 0xffff && tx.axis.data_width_bytes == 16) {
        tx.axis.protocol_error = true;
        tx.axis.protocol_error_reason = "Continuous_Packets requires TKEEP all-1 when TLAST=0";
        w.stream_local_abort(tx, RespSource::Bridge, tx.axis.protocol_error_reason);
        return false;
      }
    }

    // Assert TVALID with stable payload (may lead TREADY).
    tx.axis.tvalid = true;
    tx.axis.tready = false;
    tx.axis.held = offer;
    if (w.cfg.axis_tvalid_lead_cycles > 0) {
      sc_core::wait(w.cycles(w.cfg.axis_tvalid_lead_cycles));
    }
    // Receiver backpressure: TREADY delayed.
    const int ready_delay = std::max(0, w.cfg.axis_tready_delay_cycles);
    if (ready_delay > 0) {
      sc_core::wait(w.cycles(ready_delay));
    }
    if (tx.visible.classified || tx.axis.stream_aborted) return false;
    if (w.link.epoch != epoch || w.link.destructive) {
      // TVALID still held; no transfer counted without READY handshake.
      w.stream_local_abort(tx, RespSource::Bridge,
                           "[MODEL] AXIS abort while TVALID held (no handshake)");
      return false;
    }
    tx.axis.tready = true;
    // Handshake: count transfer only here.
    sc_core::wait(w.cycles(std::max(1, w.cfg.axi_handshake_cycles)));
    tx.axis.tvalid = false;
    tx.axis.tready = false;
    tx.axis.local_beats_accepted += 1;
    tx.axis.local_bytes_accepted += offer.valid_bytes;
    tx.proto.beats_accepted = tx.axis.local_beats_accepted;
    ++tx.proto.beat_handshake_count;
    if (offer.tlast) {
      tx.axis.local_tlast_accepted = true;
      tx.axis.local_packet_done = true;
      tx.axis.t_local_tlast_ns = w.now_ns();
    }
    w.log(TraceLevel::Transactions, "axis_transfer", "AXIS", &tx,
          "TVALID&TREADY beat=" + std::to_string(b) + " tlast=" + (offer.tlast ? "1" : "0") +
              " bytes=" + std::to_string(offer.valid_bytes));
    w.record_observer_event(tx, "AXI:STREAM_TRANSFER_" + std::to_string(b));
    if (offer.tlast) w.record_observer_event(tx, "AXI:STREAM_TLAST");

    // Bridge-AXIS-P1: one UCIe segment per local transfer. Local handshake is
    // not remote application processing.
    tx.axis.next_offer = b + 1;
    tx.axis.pending_pack_beats.push_back(b);
    w.aou_q.push(slot);
  }
  return true;
}

void axis_wait_settle(World& w, Transaction& tx, std::uint64_t epoch) {
  if (tx.visible.classified || tx.axis.stream_aborted) return;
  if (tx.axis.app_ack_mode != AxisAppAckMode::Off) {
    // Wait for App-ACK via completion_q path, or abort/timeout.
    const double deadline = w.now_ns() + std::max(w.cfg.axis_settle_grace_ns, w.cfg.txn_watchdog_ns);
    while (!tx.visible.classified && !tx.axis.app_ack_received && !tx.axis.stream_aborted) {
      if (w.now_ns() >= deadline) break;
      if (w.link.epoch != epoch && w.link.destructive) break;
      sc_core::wait(w.cycles(1), w.progress | w.link.state_ev | w.completion_q.data_event());
      if (tx.bridge.pending_local_slverr) return;  // abort path owns completion
    }
    if (tx.axis.app_ack_received && !tx.visible.classified) {
      w.stream_settle(tx);
      return;
    }
    if (!tx.visible.classified) {
      w.stream_local_abort(tx, RespSource::Watchdog,
                           "[MODEL] AXIS App-ACK missing; local abort/timeout");
    }
    return;
  }

  // No App-ACK: wait for remote *effects* (not merely reception) or grace.
  // Reception alone must not be treated as application completion.
  const double deadline = w.now_ns() + w.cfg.axis_settle_grace_ns;
  while (!tx.visible.classified && !tx.axis.stream_aborted) {
    const bool remote_done =
        (tx.axis.sink == AxisSinkKind::Imm && tx.axis.app_effects >= tx.axis.beat_count) ||
        (tx.axis.sink == AxisSinkKind::Pkt && tx.axis.app_effects >= 1);
    if (remote_done) {
      // Give phase faults armed at ArchitecturalCommit (LinkError → BlindRetry)
      // a chance to run before a clean settle races ahead of recovery policy.
      sc_core::wait(w.cycles(8), w.progress | w.link.state_ev);
      if (tx.visible.classified || tx.axis.stream_aborted || w.link.destructive) return;
      break;
    }
    if (w.now_ns() >= deadline) break;
    if (w.link.destructive && w.link.epoch != epoch) break;
    sc_core::wait(w.cycles(1), w.progress | w.link.state_ev);
  }
  if (!tx.visible.classified && !tx.axis.stream_aborted) {
    w.stream_settle(tx);
  }
}

}  // namespace

AxiMaster::AxiMaster(sc_core::sc_module_name name, World& world) : sc_module(name), w_(world) {
  SC_THREAD(accept);
  SC_THREAD(complete);
}

void AxiMaster::accept() {
  while (true) {
    std::uint32_t slot = 0;
    if (!w_.axi_q.pop(slot)) continue;
    if (slot >= w_.txns.size()) continue;
    auto& tx = w_.slot(slot);
    const auto epoch = w_.link.epoch;

    if (tx.axis.enabled) {
      if (!axis_accept_packet(w_, tx, slot, epoch)) continue;
      axis_wait_settle(w_, tx, epoch);
      continue;
    }

    // [MODEL] AXI handshake latency. Not defined by UCIe.
    // [SPECIFICATION FACT] AXI4: address (and write data) acceptance is a
    // VALID/READY handshake; acceptance alone is not completion.
    sc_core::wait(w_.cycles(w_.cfg.axi_handshake_cycles));
    if (w_.link.epoch != epoch || tx.visible.classified) continue;
    tx.visible.axi_accepted = true;
    tx.proto.state = AxiProtoState::Outstanding;
    tx.proto.t_accepted_ns = w_.now_ns();
    tx.proto.beats_accepted = tx.is_write ? tx.proto.burst_len : 1;  // addr beat
    w_.note(tx, Phase::AxiAccepted, "AXI", "AXI accepts T" + std::to_string(tx.logical_id));
    w_.record_observer_event(tx, "ACCEPT");
    w_.arm_txn_watchdog(tx);
    if (tx.visible.classified || w_.link.destructive || w_.link.epoch != epoch) continue;
    w_.aou_q.push(slot);
  }
}

void AxiMaster::complete() {
  while (true) {
    std::uint32_t slot = 0;
    if (!w_.completion_q.pop(slot)) continue;
    if (slot >= w_.txns.size()) continue;
    auto& tx = w_.slot(slot);
    const auto epoch = w_.link.epoch;

    if (tx.axis.enabled) {
      // Optional App-ACK return (separate interface). Loss is modeled by never
      // reaching here / suppress. Not an AXI-Stream BRESP.
      if (tx.visible.classified || tx.axis.stream_aborted) continue;
      if (tx.axis.app_ack_mode != AxisAppAckMode::Off) {
        tx.axis.app_ack_received = true;
        tx.axis.t_app_ack_ns = w_.now_ns();
        w_.record_observer_event(tx, "AXI:STREAM_APP_ACK_RX");
        w_.stream_settle(tx);
      }
      continue;
    }

    // Local SLVERR path (ERR-05 / WDT-03): VALID already asserted; apply READY
    // backpressure and per-beat read handshakes before completing.
    if (tx.bridge.pending_local_slverr) {
      if (tx.visible.classified || tx.visible.axi_completed_ok || tx.visible.axi_completed_err ||
          tx.bridge.local_closed) {
        tx.bridge.pending_local_slverr = false;
        continue;
      }
      if (w_.cfg.axi_resp_ready_delay_cycles > 0) {
        tx.proto.resp_backpressured = true;
        sc_core::wait(w_.cycles(w_.cfg.axi_resp_ready_delay_cycles));
      }
      // [SPECIFICATION FACT] AXI4 §A3.4.4: remaining read beats + RLAST required.
      if (!tx.is_write) {
        while (tx.proto.beats_responded < tx.proto.burst_len) {
          sc_core::wait(w_.cycles(std::max(1, w_.cfg.completion_cycles)));
          ++tx.proto.beats_responded;
          ++tx.proto.beat_handshake_count;
          w_.record_observer_event(tx, "RBEAT_" + std::to_string(tx.proto.beats_responded));
        }
        tx.proto.rlast_seen = true;
        w_.record_observer_event(tx, "AXI:RLAST");
      } else {
        sc_core::wait(w_.cycles(w_.cfg.completion_cycles));
        ++tx.proto.beat_handshake_count;
      }
      const RespSource src = tx.bridge.pending_local_source;
      tx.bridge.pending_local_slverr = false;
      w_.complete_resp(tx, AxiResp::Slverr, src);
      arm_err07_after_local(w_, tx);
      continue;
    }

    // Optional manager READY backpressure before accepting a target response.
    if (w_.cfg.axi_resp_ready_delay_cycles > 0) {
      if (tx.proto.t_resp_ready_ns < 0) tx.proto.t_resp_ready_ns = w_.now_ns();
      tx.proto.resp_backpressured = true;
      sc_core::wait(w_.cycles(w_.cfg.axi_resp_ready_delay_cycles));
    }
    sc_core::wait(w_.cycles(w_.cfg.completion_cycles));
    if (tx.visible.classified || tx.visible.axi_completed_ok || tx.visible.axi_completed_err) {
      // Already closed locally — treat as late if a second completion arrives.
      if (w_.cfg.discard_late_responses) {
        tx.bridge.late_response_seen = true;
        ++tx.bridge.late_responses_discarded;
        ++w_.late_responses_discarded;
        w_.log(TraceLevel::Transactions, "late_response_discard", "AXI", &tx,
               "[PROJECT WDT-04] completion_q entry after local close discarded");
        w_.record_observer_event(tx, "LATE_RESP_DISCARD");
      }
      continue;
    }
    if (w_.link.epoch != epoch && w_.scenario.policy != RecoveryPolicy::PreserveLocal &&
        w_.scenario.policy != RecoveryPolicy::LocalSlverr) {
      continue;
    }
    w_.complete(tx);
  }
}

}  // namespace ucie
