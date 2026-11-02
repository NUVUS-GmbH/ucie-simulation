#include "world.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace ucie {
namespace {

bool is_payload(FlitRole role) {
  return role == FlitRole::Request || role == FlitRole::WriteData || role == FlitRole::Response ||
         role == FlitRole::StreamSegment;
}

bool is_control(FlitRole role) {
  return role == FlitRole::Ack || role == FlitRole::Nak || role == FlitRole::CreditReturn;
}

}  // namespace

World::World(const Config& config, Scenario scenario_in)
    : cfg(config), scenario(std::move(scenario_in)), rng(static_cast<std::mt19937::result_type>(config.seed)) {
  txns.reserve(1024);
  partial.reserve(1024);
  arbiter.set_policy(scenario.arbitration);
  link.preserve_retry = scenario.retrain_preserves_retry;
  target.counter = scenario.counter_start;
  target.duplicate_suppress = cfg.target_duplicate_suppress;
  target.axis_dup_suppress = cfg.axis_dup_suppress;
  target.axis_dup_capacity = cfg.axis_dup_capacity;
  target.axis_dup_lifetime_ns = cfg.axis_dup_lifetime_ns;
  target.axis_dup_clear_on_reset = cfg.axis_dup_clear_on_reset;
  target.poison_policy = cfg.poison_target_policy;
  target.atomic_address = cfg.axi5_atomic_address;
  target.atomic_value = cfg.axi5_atomic_location_init;
  target.axi5_commit_watermark = 0;
  const std::size_t guest_cap = static_cast<std::size_t>(cfg.guest_queue_entries);
  const std::size_t tx_cap = static_cast<std::size_t>(cfg.tx_buffer_entries);
  const std::size_t rx_cap = static_cast<std::size_t>(cfg.rx_buffer_entries);
  const std::size_t ctrl_cap = static_cast<std::size_t>(cfg.ctrl_queue_entries);
  for (int i = 0; i < 3; ++i) guest_q[i].configure(guest_cap);
  axi_q.configure(static_cast<std::size_t>(cfg.axi_queue_entries));
  aou_q.configure(static_cast<std::size_t>(cfg.aou_queue_entries));
  target_q.configure(rx_cap);
  completion_q.configure(static_cast<std::size_t>(cfg.response_queue_entries));
  for (int i = 0; i < 2; ++i) {
    credit[i].configure(cfg.credit_depth);
    retry[i].configure(static_cast<std::size_t>(cfg.retry_buffer_entries));
    send_q[i].configure(tx_cap);
    replay_q[i].configure(tx_cap);
    ctrl_q[i].configure(ctrl_cap);
    wire[i].configure(4);
    arrive[i].configure(rx_cap);
    credits_issued[i] = cfg.credit_depth;
  }
  const std::size_t sb_cap = static_cast<std::size_t>(std::max(1, cfg.sideband_fifo_entries));
  sb_out.configure(sb_cap);
  sb_in.configure(sb_cap);
  if (scenario.random_faults) arm.storm_naks = 0;
}

double World::now_ns() const { return sc_core::sc_time_stamp().to_seconds() * 1e9; }

sc_core::sc_time World::cycles(int n) const {
  // [MODEL] Digital handshake and buffer delays are an integer number of
  // clock periods. In cycle mode that is the timing claim. It is not a claim
  // about the UCIe PHY or about FDI bit timing.
  if (n < 0) n = 0;
  return sc_core::sc_time(cfg.clock_period_ns * n, sc_core::SC_NS);
}

sc_core::sc_time World::serialization() const {
  // [MODEL] Flit time is serialization_cycles of the configured clock, scaled
  // by throttle or degradation. The scale is not a UCIe rate table.
  return cycles(cfg.serialization_cycles) * link.rate_scale;
}

sc_core::sc_time World::propagation() const {
  return cycles(cfg.propagation_cycles) * link.rate_scale;
}

int World::max_unacked() const {
  // [SPEC] UCIe 2.0 §3.8: MAX_UNACKNOWLEDGED_FLITS is min(Tx retry buffer, 127).
  const int cap = cfg.max_unacknowledged_cap > 0 ? cfg.max_unacknowledged_cap : 127;
  return std::min(cfg.retry_buffer_entries, cap);
}

Transaction& World::slot(std::uint32_t index) { return txns.at(index); }

Transaction* World::find(std::uint64_t logical_id) {
  for (auto& tx : txns) {
    if (tx.logical_id == logical_id) return &tx;
  }
  return nullptr;
}

void World::sample(const std::string& name, std::size_t occ) {
  if (cfg.trace < TraceLevel::Transactions) return;
  occupancy.push_back(QueueSample{now_ns(), name, occ});
}

void World::log(TraceLevel need, const char* event, const char* component, const Transaction* tx,
                const std::string& details) {
  if (cfg.trace < need) return;
  TraceEvent row;
  row.time_ns = now_ns();
  row.event = event;
  row.component = component;
  row.details = details;
  row.state = std::string(to_string(link.state));
  if (tx) {
    row.transaction_id = tx->logical_id;
    row.guest_id = tx->guest_id;
    row.state = std::string(to_string(tx->phase));
  }
  trace.push_back(std::move(row));
}

void World::set_state(LinkState state) {
  link.state = state;
  states.push_back(StateSample{now_ns(), std::string(to_string(state))});
  log(TraceLevel::Summary, "link_state", "Link", nullptr, std::string(to_string(state)));
  link.state_ev.notify(sc_core::SC_ZERO_TIME);
}

void World::note_detection() {
  if (!link.detection_set) {
    link.detection_set = true;
    link.detection_time_ns = now_ns();
  }
}

void World::request(RecoveryCmd cmd) {
  link.cmd = cmd;
  link.cmd_pending = true;
  link.busy = true;
  if (cmd == RecoveryCmd::LinkError || cmd == RecoveryCmd::LinkReset ||
      cmd == RecoveryCmd::EmergencyShutdown || cmd == RecoveryCmd::RemoteUnresponsive) {
    link.destructive = true;
  }
  link.cmd_ev.notify(sc_core::SC_ZERO_TIME);
}

std::uint32_t World::slot_index(const Transaction& tx) const {
  const auto* base = txns.data();
  const auto idx = static_cast<std::uint32_t>(&tx - base);
  if (idx >= txns.size()) {
    throw std::logic_error("slot_index: transaction not in world pool");
  }
  return idx;
}

bool World::remote_may_continue(const Transaction& tx) const {
  if (tx.bridge.cancel_effective_ns >= 0 && now_ns() >= tx.bridge.cancel_effective_ns) {
    return false;
  }
  return tx.bridge.allow_remote_continue;
}

void World::record_observer_event(Transaction& tx, const std::string& event) {
  // Allowed observations by profile (prefix convention):
  //   AXI:     address/data/resp handshakes and response *codes* (not internal source)
  //   FDI:     link-down / pl_state_sts visibility
  //   STATUS:  standardized UCIe status bits (no AXI id / commit bit)
  //   DIAG:    optional bridge diagnostics (C/D only when exposed)
  //   DISP:    disposition / ACK-NAK / late-discard (D only) — never architectural effect
  //   GT:      ground-truth evaluation only; NEVER projected into A–D or classify()
  // Legacy unprefixed AXI-like names (ACCEPT, RESP_CODE_*, RBEAT_*) map to AXI.
  if (event.rfind("GT:", 0) == 0) {
    // Keep on the global sim trace via callers' note()/log(); do not contaminate
    // observer projections with execution knowledge.
    return;
  }
  const std::string row = std::to_string(now_ns()) + "|" + event;
  const bool axi = event.rfind("AXI:", 0) == 0 || event == "ACCEPT" ||
                   event.rfind("RESP_CODE_", 0) == 0 || event.rfind("RBEAT_", 0) == 0 ||
                   event.rfind("AXI_", 0) == 0 || event.rfind("AXI:STREAM_", 0) == 0;
  const bool fdi = event.rfind("FDI:", 0) == 0 || event.rfind("FDI_", 0) == 0;
  const bool status = event.rfind("STATUS:", 0) == 0;
  const bool diag = event.rfind("DIAG:", 0) == 0;
  const bool disp = event.rfind("DISP:", 0) == 0 || event == "LATE_RESP_DISCARD";

  if (axi) tx.observer_trace_a.push_back(row);
  if (axi || fdi) tx.observer_trace_b.push_back(row);
  if (axi || fdi || status || (diag && cfg.expose_bridge_diagnostics)) {
    tx.observer_trace_c.push_back(row);
  }
  if (axi || fdi || status || (diag && cfg.expose_bridge_diagnostics) || disp) {
    tx.observer_trace_d.push_back(row);
  }
}

void World::classify_now(Transaction& tx) {
  if (tx.visible.status_has_axi_id || tx.visible.status_has_commit_bit) {
    internal_errors.push_back("standardized status carried an AXI id or a commit bit");
  }
  if (cfg.expose_bridge_diagnostics) {
    tx.visible.diag_bridge_local_slverr =
        tx.visible.axi_completed_err &&
        (tx.visible.axi_resp_source == RespSource::Bridge ||
         tx.visible.axi_resp_source == RespSource::Watchdog);
    tx.visible.diag_bridge_token_valid = tx.bridge.token != 0;
    tx.visible.diag_bridge_token = tx.bridge.token;
    // Strong evidence only: never handed to UCIe TX. Missing ACK is recorded
    // separately and must not drive NOT_COMPLETED (delivery can precede ACK).
    tx.visible.diag_never_handed = !tx.visible.handed_to_ucie;
    tx.visible.diag_missing_ack = !tx.visible.request_ack;
    record_observer_event(tx, std::string("DIAG:local_slverr=") +
                                  (tx.visible.diag_bridge_local_slverr ? "1" : "0") +
                                  ",never_handed=" +
                                  (tx.visible.diag_never_handed ? "1" : "0") +
                                  ",missing_ack=" +
                                  (tx.visible.diag_missing_ack ? "1" : "0"));
  }
  // Comparison baseline: C/D with diagnostics present but not used.
  {
    const bool saved = tx.visible.use_diag_fields;
    tx.visible.use_diag_fields = false;
    tx.visible.fate_c_without_diag = classify(view_for(ObserverProfile::C_AxiFdiUcieStatus, tx));
    tx.visible.fate_d_without_diag = classify(view_for(ObserverProfile::D_AxiHypervisorMetadata, tx));
    tx.visible.use_diag_fields = saved;
  }
  tx.visible.use_diag_fields = cfg.expose_bridge_diagnostics && cfg.use_diag_in_classifier;
  tx.visible.fate_a = classify(view_for(ObserverProfile::A_AxiOnly, tx));
  tx.visible.fate_b = classify(view_for(ObserverProfile::B_AxiFdi, tx));
  tx.visible.fate_c = classify(view_for(ObserverProfile::C_AxiFdiUcieStatus, tx));
  tx.visible.fate_d = classify(view_for(ObserverProfile::D_AxiHypervisorMetadata, tx));
  tx.visible.knowledge_a = classify_effect(view_for(ObserverProfile::A_AxiOnly, tx));
  tx.visible.knowledge_b = classify_effect(view_for(ObserverProfile::B_AxiFdi, tx));
  tx.visible.knowledge_c = classify_effect(view_for(ObserverProfile::C_AxiFdiUcieStatus, tx));
  tx.visible.knowledge_d = classify_effect(view_for(ObserverProfile::D_AxiHypervisorMetadata, tx));
  // Classification equality of A/B/C is a measured outcome, not an invariant.
  tx.visible.classified = true;
  tx.recovery_epoch = static_cast<int>(link.epoch);
  record_observer_event(tx, std::string("AXI:CLASSIFY_A=") + std::string(to_string(tx.visible.fate_a)));
  if (cfg.expose_bridge_diagnostics) {
    record_observer_event(tx, std::string("STATUS:fate_c=") + std::string(to_string(tx.visible.fate_c)) +
                                  ",fate_c_no_diag=" + std::string(to_string(tx.visible.fate_c_without_diag)));
    record_observer_event(tx, std::string("DISP:fate_d=") + std::string(to_string(tx.visible.fate_d)) +
                                  ",fate_d_no_diag=" + std::string(to_string(tx.visible.fate_d_without_diag)));
  }
}

void World::complete_resp(Transaction& tx, AxiResp resp, RespSource source) {
  if (tx.visible.axi_completed_ok || tx.visible.axi_completed_err || tx.bridge.local_closed) {
    tx.visible.duplicate_completion = true;
    log(TraceLevel::Errors, "duplicate_completion", "AXI", &tx,
        "second local response refused (WDT-04 / no second completion)");
    return;
  }
  // [SPECIFICATION FACT] AXI4: write completion is the B handshake; read
  // completion requires all beats including RLAST and the final handshake.
  // Caller (AxiMaster) must already have applied READY delay / beat handshakes.
  if (tx.proto.t_resp_ready_ns < 0) tx.proto.t_resp_ready_ns = now_ns();
  tx.proto.bvalid_asserted = true;
  tx.proto.resp = resp;
  tx.proto.resp_source = source;
  tx.proto.t_resp_handshake_ns = now_ns();
  tx.proto.bready_accepted = true;
  if (tx.proto.t_resp_ready_ns >= 0) {
    tx.proto.resp_ready_to_handshake_ns = tx.proto.t_resp_handshake_ns - tx.proto.t_resp_ready_ns;
  }
  if (resp == AxiResp::Okay) {
    tx.visible.axi_completed_ok = true;
    tx.proto.state = AxiProtoState::CompletedOk;
  } else {
    tx.visible.axi_completed_err = true;
    tx.proto.state = AxiProtoState::CompletedError;
  }
  tx.visible.axi_resp = resp;
  tx.visible.axi_resp_source = source;
  tx.completion_received = true;
  tx.visible.stopped = false;
  tx.t_completion_ns = now_ns();
  tx.phase = (resp == AxiResp::Okay) ? Phase::Completed : Phase::Failed;
  tx.bridge.local_closed = true;
  ++tx.bridge.generation;
  if (tx.is_write) {
    tx.proto.beats_responded = 1;
  } else {
    // Beats must already have been handshaked by AxiMaster for local SLVERR;
    // for target OKAY path, fill remaining if not yet recorded.
    if (tx.proto.beats_responded < tx.proto.burst_len) {
      tx.proto.beats_responded = std::max(1, tx.proto.burst_len);
    }
    tx.proto.rlast_seen = true;
  }
  if (cfg.cancel_remote_on_local_close) {
    // MODEL ASSUMPTION: cancel is a local flag (optionally delayed), not a
    // transmitted/confirmed cancel protocol with round-trip evidence.
    tx.bridge.cancel_requested = true;
    if (cfg.cancel_latency_ns <= 0) {
      tx.bridge.allow_remote_continue = false;
      tx.bridge.cancel_effective_ns = now_ns();
    } else {
      tx.bridge.cancel_effective_ns = now_ns() + cfg.cancel_latency_ns;
      // allow_remote_continue stays true until remote_may_continue() expires it.
    }
  }

  if (tx.ground.commit_time_ns >= 0) {
    tx.ambiguity_window_ns = tx.t_completion_ns - tx.ground.commit_time_ns;
    // OKAY with commit_count==1 closes determined; SLVERR never proves effect.
    if (resp == AxiResp::Okay) {
      tx.ambiguity_open = tx.ground.architectural_commit_count != 1;
      tx.ambiguity_ended_unknown = false;
    } else {
      tx.ambiguity_open = false;
      tx.ambiguity_ended_unknown = true;
    }
  }
  if (tx.deadline_ns >= 0 && tx.t_completion_ns > tx.deadline_ns) tx.visible.deadline_violated = true;
  classify_now(tx);
  std::ostringstream text;
  text << "AXI " << to_string(resp) << " from " << to_string(source) << ", proto "
       << to_string(tx.proto.state) << ", observer A " << to_string(tx.visible.fate_a) << " knowledge "
       << to_string(tx.visible.knowledge_a) << ", ready_to_hs_ns "
       << tx.proto.resp_ready_to_handshake_ns;
  log(TraceLevel::Transactions, "complete_resp", "AXI", &tx, text.str());
  // Profile A sees the response code only; source is disposition/internal.
  record_observer_event(tx, std::string("RESP_CODE_") + std::string(to_string(resp)));
  record_observer_event(tx, std::string("DISP:RESP_SRC_") + std::string(to_string(source)));
  progress.notify(sc_core::SC_ZERO_TIME);
}

void World::complete(Transaction& tx) {
  // Target BRESP may be OKAY or SLVERR (Poison reject / accept-marked policies).
  // Flit.resp is latched into proto.resp by handle_incoming_response when Accept.
  AxiResp resp = AxiResp::Okay;
  if (tx.proto.resp_source == RespSource::Target &&
      (tx.proto.resp == AxiResp::Slverr || tx.proto.resp == AxiResp::Decerr ||
       tx.proto.resp == AxiResp::Exokay)) {
    resp = tx.proto.resp;
  } else if (tx.axi5.enabled) {
    resp = tx.axi5.intended_bresp;
  }
  complete_resp(tx, resp, RespSource::Target);
}

void World::local_slverr(Transaction& tx, RespSource source, const std::string& reason) {
  if (tx.visible.classified || tx.visible.axi_completed_ok || tx.visible.axi_completed_err) return;
  if (tx.bridge.pending_local_slverr || tx.bridge.local_closed) return;
  if (tx.axis.enabled) {
    stream_local_abort(tx, source, reason);
    return;
  }
  if (!tx.visible.axi_accepted) {
    // Not yet accepted: no AXI response channel obligation; abandon instead.
    abandon(tx, source == RespSource::Bridge, false);
    return;
  }
  // Stage local SLVERR onto the AXI response path so READY backpressure and
  // per-beat read handshakes are exercised (same path as target completions).
  tx.bridge.pending_local_slverr = true;
  tx.bridge.pending_local_source = source;
  tx.proto.t_resp_ready_ns = now_ns();
  tx.proto.bvalid_asserted = true;
  if (cfg.axi_resp_ready_delay_cycles > 0) tx.proto.resp_backpressured = true;
  log(TraceLevel::Transactions, "local_slverr", "Bridge", &tx, reason);
  record_observer_event(tx, "AXI:LOCAL_SLVERR_VALID");
  completion_q.push(slot_index(tx));
  progress.notify(sc_core::SC_ZERO_TIME);
}

void World::stream_local_abort(Transaction& tx, RespSource source, const std::string& reason) {
  // AXIS has no BRESP. This closes the local stream lifecycle with bridge
  // diagnostics only. axi_completed_err is reused as "lifecycle closed with
  // error evidence" — not a claim that AXI-Stream defined SLVERR.
  if (tx.visible.classified || tx.axis.stream_aborted || tx.bridge.local_closed) return;
  if (!tx.visible.axi_accepted) {
    abandon(tx, source == RespSource::Bridge, false);
    return;
  }
  tx.axis.stream_aborted = true;
  tx.bridge.local_closed = true;
  ++tx.bridge.generation;
  tx.visible.axi_completed_err = true;
  tx.visible.axi_resp = AxiResp::Slverr;
  tx.visible.axi_resp_source = source;
  tx.proto.state = AxiProtoState::CompletedError;
  tx.proto.resp = AxiResp::Slverr;
  tx.proto.resp_source = source;
  tx.t_completion_ns = now_ns();
  tx.phase = Phase::Failed;
  tx.completion_received = true;
  if (cfg.cancel_remote_on_local_close) {
    tx.bridge.cancel_requested = true;
    if (cfg.cancel_latency_ns <= 0) {
      tx.bridge.allow_remote_continue = false;
      tx.bridge.cancel_effective_ns = now_ns();
    } else {
      tx.bridge.cancel_effective_ns = now_ns() + cfg.cancel_latency_ns;
    }
  }
  if (tx.ground.commit_time_ns >= 0) {
    tx.ambiguity_window_ns = tx.t_completion_ns - tx.ground.commit_time_ns;
    tx.ambiguity_open = false;
    tx.ambiguity_ended_unknown = true;
  }
  log(TraceLevel::Transactions, "stream_local_abort", "Bridge", &tx, reason);
  record_observer_event(tx, "AXI:STREAM_LOCAL_ABORT");
  record_observer_event(tx, std::string("DISP:RESP_SRC_") + std::string(to_string(source)));
  // Do not emit RESP_CODE_* — that would invent a Stream BRESP observation.
  classify_now(tx);
  // ERR-07 arming for stream uses the same master_retry policy.
  if (tx.software_retry_count < 1) {
    if (cfg.master_retry == MasterRetryPolicy::BlindRetry) {
      tx.pending_reissue = true;
      tx.ground.commit_at_reissue = tx.ground.architectural_commit_count;
      log(TraceLevel::Transactions, "err07_blind_retry", "Master", &tx,
          "[PROJECT ERR-07] BlindRetry armed after stream local abort");
    } else if (cfg.master_retry == MasterRetryPolicy::RetryOnlyIfNotCompleted) {
      const bool not_completed = tx.visible.fate_d == Fate::NotCompleted ||
                                 tx.visible.fate_c == Fate::NotCompleted ||
                                 tx.visible.fate_a == Fate::NotCompleted;
      if (not_completed &&
          (!remote_may_continue(tx) || cfg.target_duplicate_suppress || cfg.axis_dup_suppress)) {
        tx.pending_reissue = true;
        tx.ground.commit_at_reissue = tx.ground.architectural_commit_count;
        log(TraceLevel::Transactions, "err07_safe_retry", "Master", &tx,
            "[PROJECT ERR-07] RetryOnlyIfNotCompleted armed after stream abort");
      } else {
        log(TraceLevel::Transactions, "err07_safe_retry_blocked", "Master", &tx,
            "[PROJECT ERR-07] stream safe retry blocked");
      }
    }
  }
  progress.notify(sc_core::SC_ZERO_TIME);
}

void World::stream_settle(Transaction& tx) {
  if (tx.visible.classified || tx.axis.stream_aborted) return;
  // Base AXIS interface has no end-to-end OKAY. Settling after local TLAST
  // leaves effect fate UNKNOWN unless an optional App-ACK arrived.
  tx.axis.stream_settled = true;
  tx.bridge.local_closed = true;
  ++tx.bridge.generation;
  tx.t_completion_ns = now_ns();
  if (tx.axis.app_ack_mode != AxisAppAckMode::Off && tx.axis.app_ack_received) {
    tx.visible.axi_completed_ok = true;
    tx.proto.state = AxiProtoState::CompletedOk;
    tx.phase = Phase::Completed;
    tx.completion_received = true;
    record_observer_event(tx, "AXI:STREAM_APP_ACK");
  } else {
    // Protocol locally done; effect knowledge remains indeterminate.
    tx.proto.state = AxiProtoState::Outstanding;
    tx.phase = Phase::Completed;
    tx.completion_received = false;
    record_observer_event(tx, "AXI:STREAM_SETTLE_NO_APP_ACK");
  }
  if (tx.axis.local_packet_done) {
    record_observer_event(tx, "AXI:STREAM_PACKET_DONE_LOCAL");
  }
  if (tx.ground.commit_time_ns >= 0) {
    tx.ambiguity_window_ns = tx.t_completion_ns - tx.ground.commit_time_ns;
    tx.ambiguity_open = !tx.visible.axi_completed_ok;
    tx.ambiguity_ended_unknown = !tx.visible.axi_completed_ok;
  }
  classify_now(tx);
  log(TraceLevel::Transactions, "stream_settle", "Bridge", &tx,
      std::string("AXIS settle fate_a=") + std::string(to_string(tx.visible.fate_a)) +
          " commits=" + std::to_string(tx.ground.architectural_commit_count));
  progress.notify(sc_core::SC_ZERO_TIME);
}

void World::accept_axis_segment(Transaction& tx, const Flit& flit) {
  if (!tx.axis.enabled) return;
  if (tx.visible.classified && !remote_may_continue(tx)) return;
  const int beat = flit.beat_index;
  if (beat < 0 || beat >= tx.axis.beat_count) return;
  // Count remote acceptance separately from application effects.
  if (beat < tx.axis.remote_beats_accepted) {
    // Duplicate delivery of an already-accepted beat index (should be rare
    // after UCIe seq filter); ignore for reception counters.
    return;
  }
  // Require in-order beat acceptance for Continuous_Packets bridge policy.
  if (beat != tx.axis.remote_beats_accepted) return;
  tx.axis.remote_beats_accepted += 1;
  tx.axis.remote_bytes_accepted += flit.valid_bytes;
  tx.axis.pending_remote_beats.push_back(beat);
  if (flit.tlast) {
    tx.axis.remote_packet_complete = true;
    tx.axis.t_remote_packet_complete_ns = now_ns();
    tx.axis.pkt_buffer_sealed = true;
    log(TraceLevel::Transactions, "axis_remote_packet_complete", "Target", &tx,
        "remote TLAST accepted (reception only; not an application effect)");
    record_observer_event(tx, "GT:AXIS_REMOTE_PACKET_COMPLETE");
  }
  target_q.push(slot_index(tx));
  progress.notify(sc_core::SC_ZERO_TIME);
}

World::LateDisposition World::handle_incoming_response(Transaction& tx, const Flit& flit) {
  // WDT-04: after local close, discard late responses for this lifecycle.
  if (cfg.discard_late_responses && tx.bridge.local_closed) {
    tx.bridge.late_response_seen = true;
    ++tx.bridge.late_responses_discarded;
    ++late_responses_discarded;
    log(TraceLevel::Transactions, "late_response_discard", "Bridge", &tx,
        "[PROJECT WDT-04] late response discarded after local close");
    record_observer_event(tx, "LATE_RESP_DISCARD");
    progress.notify(sc_core::SC_ZERO_TIME);
    return LateDisposition::DiscardLate;
  }
  if (cfg.bridge_token_on_wire) {
    if (flit.bridge_token != 0 && flit.bridge_token != tx.bridge.token) {
      ++late_responses_discarded;
      log(TraceLevel::Errors, "wrong_txn_response", "Bridge", &tx,
          "response bridge_token does not match open transaction");
      record_observer_event(tx, "DISP:WRONG_TOKEN_DISCARD");
      return LateDisposition::DiscardWrongTxn;
    }
    // Generation mismatch: response belongs to a prior lifecycle of same token.
    if (flit.bridge_generation != 0 && flit.bridge_generation != tx.bridge.generation &&
        tx.bridge.local_closed) {
      ++late_responses_discarded;
      return LateDisposition::DiscardLate;
    }
  }
  // Latch the matched response identity for assignment proofs (ID-reuse tests).
  tx.bridge.completing_token = flit.bridge_token != 0 ? flit.bridge_token : tx.bridge.token;
  tx.bridge.completing_generation = flit.bridge_generation;
  tx.bridge.completing_token_set = true;
  // Latch target BRESP / BTRACE from the wire flit (before AXI READY handshake).
  tx.proto.resp = flit.resp;
  tx.proto.resp_source = RespSource::Target;
  if (tx.axi5.enabled) {
    tx.axi5.intended_bresp = flit.resp;
    if (flit.btrace) {
      tx.axi5.btrace = true;
      tx.axi5.trace_exported = true;
    }
  }
  // AXI ID alone is not used as the sole match key.
  return LateDisposition::Accept;
}

void World::abandon(Transaction& tx, bool fdi, bool explicit_unknown) {
  if (tx.visible.classified || tx.visible.axi_completed_ok || tx.visible.axi_completed_err) return;
  if (fdi) {
    tx.visible.fdi_link_down = true;
    tx.visible.linkerror_status = true;
    tx.visible.crc_status = link.crc_failures > 0;
    tx.visible.retrain_status = link.retrain_events > 0;
    tx.visible.fast_throttle = link.fast_throttle;
    tx.visible.emergency_shutdown = link.emergency;
    if (tx.t_fdi_ns < 0) tx.t_fdi_ns = now_ns();
    if (tx.ambiguity_open && tx.ground.commit_time_ns >= 0) {
      tx.ambiguity_window_ns = tx.t_fdi_ns - tx.ground.commit_time_ns;
      tx.ambiguity_open = false;
      tx.ambiguity_ended_unknown = true;
    }
    record_observer_event(tx, "FDI:LINK_DOWN");
    record_observer_event(tx, "STATUS:LINKERROR");
  }
  tx.visible.explicit_unknown = explicit_unknown;
  tx.visible.stopped = true;
  if (!tx.visible.axi_accepted) tx.phase = Phase::Failed;
  if (tx.deadline_ns >= 0 && now_ns() > tx.deadline_ns) tx.visible.deadline_violated = true;
  classify_now(tx);
  std::ostringstream text;
  text << "no AXI completion, observer A " << to_string(tx.visible.fate_a) << ", observer D "
       << to_string(tx.visible.fate_d);
  log(TraceLevel::Transactions, explicit_unknown ? "explicit_unknown" : "discard", "Recovery", &tx, text.str());
  progress.notify(sc_core::SC_ZERO_TIME);
}

void World::arm_txn_watchdog(Transaction& tx) {
  if (!cfg.txn_watchdog_enabled) return;
  if (tx.t_watchdog_arm_ns >= 0) return;
  tx.t_watchdog_arm_ns = now_ns();
  log(TraceLevel::Transactions, "txn_watchdog_arm", "WDT", &tx,
      "arm transaction watchdog id=" + std::to_string(tx.axi_id) + " addr=0x" +
          std::to_string(tx.address) + " dir=" + (tx.is_write ? "W" : "R") +
          " timeout_ns=" + std::to_string(cfg.txn_watchdog_ns));
}

void World::observe_link_down() {
  // FDI can be sampled when the link enters the destructive state. The 16 ms
  // residency is how long that state lasts, not how long the initiator must
  // wait before the state is visible.
  const double now = now_ns();
  for (auto& tx : txns) {
    if (tx.visible.classified || tx.visible.axi_completed_ok || tx.visible.axi_completed_err ||
        tx.t_created_ns < 0)
      continue;
    tx.visible.fdi_link_down = true;
    tx.visible.linkerror_status = true;
    tx.visible.crc_status = link.crc_failures > 0;
    tx.visible.retrain_status = link.retrain_events > 0;
    if (tx.t_fdi_ns < 0) tx.t_fdi_ns = now;
    const bool retry_later =
        scenario.policy == RecoveryPolicy::AutoRetry && tx.visible.axi_accepted && tx.software_retry_count < 1;
    if (!retry_later && tx.ambiguity_open && tx.ground.commit_time_ns >= 0) {
      tx.ambiguity_window_ns = now - tx.ground.commit_time_ns;
      tx.ambiguity_open = false;
      tx.ambiguity_ended_unknown = true;
    }
  }
}

void World::apply_policy() {
  // [POLICY] Named recovery policies. None of them is a UCIe-mandated AXI response.
  // LocalSlverr is PROJECT REQUIREMENT ERR-05 from the fault-handling draft.
  const bool err05 = scenario.policy == RecoveryPolicy::LocalSlverr || cfg.local_slverr_on_link_error;
  for (auto& tx : txns) {
    if (tx.visible.classified || tx.visible.axi_completed_ok || tx.visible.axi_completed_err) continue;
    if (tx.t_created_ns < 0) continue;
    if (scenario.policy == RecoveryPolicy::PreserveLocal && tx.visible.response_at_local_aou) {
      log(TraceLevel::Transactions, "preserve_local", "AoU", &tx,
          "[POLICY] response already at the initiating AoU is delivered");
      complete(tx);
      continue;
    }
    if (scenario.policy == RecoveryPolicy::ExplicitUnknown && tx.visible.axi_accepted) {
      log(TraceLevel::Transactions, "policy", "Recovery", &tx,
          "[POLICY] explicit UNKNOWN, not OKAY and not permission to retry");
      abandon(tx, true, true);
      continue;
    }
    if (scenario.policy == RecoveryPolicy::AutoRetry && tx.visible.axi_accepted && tx.software_retry_count < 1) {
      tx.pending_reissue = true;
      tx.ground.commit_at_reissue = tx.ground.architectural_commit_count;
      tx.visible.fdi_link_down = true;
      tx.visible.linkerror_status = true;
      tx.visible.crc_status = link.crc_failures > 0;
      tx.t_fdi_ns = now_ns();
      log(TraceLevel::Transactions, "auto_retry_armed", "Recovery", &tx,
          "[POLICY] same logical transaction will be reissued after the link is up");
      continue;
    }
    if (err05 && tx.visible.axi_accepted) {
      tx.visible.fdi_link_down = true;
      tx.visible.linkerror_status = true;
      if (tx.t_fdi_ns < 0) tx.t_fdi_ns = now_ns();
      local_slverr(tx, RespSource::Bridge,
                   "[PROJECT ERR-05] LinkError/Link-Down: complete outstanding AXI with SLVERR");
      // ERR-07 is decided after the AXI handshake completes (AxiMaster path).
      continue;
    }
    if (tx.ground.response_created && !tx.visible.axi_completed_ok && !tx.visible.axi_completed_err) {
      log(TraceLevel::Transactions, "response_discarded", "Recovery", &tx, "Response is discarded during cleanup");
    }
    abandon(tx, true, false);
  }
}

void World::flush_destructive() {
  ++link.epoch;
  ++link.wire_gen;
  for (int i = 0; i < 2; ++i) {
    retry[i].flush();
    send_q[i].flush();
    replay_q[i].flush();
    ctrl_q[i].flush();
    wire[i].flush();
    arrive[i].flush();
    accepted[i].clear();
    link.next_seq[i] = 0;
    link.expected_seq[i] = 0;
    for (int g = 0; g < 3; ++g) {
      link.next_seq_guest[i][g] = 0;
      link.expected_seq_guest[i][g] = 0;
    }
    link.streak[i] = 0;
    link.resend[i] = false;
  }
  target_q.flush();
  completion_q.flush();
  aou_q.flush();
  for (auto& part : partial) part = Partial{};
  for (int g = 0; g < 3; ++g) outstanding_by_guest[g] = 0;
  resources.tx_slot_owner = -1;
  resources.tx_slot_is_replay = false;
  resources.tx_slot_acquired_ns = -1;
  arm.persistent = false;
  arm.oneshot = 0;
  arm.drop_one = false;
  arm.suppress_ack = false;
  link.flushed.notify(sc_core::SC_ZERO_TIME);
  log(TraceLevel::Summary, "flush", "UCIe", nullptr,
      "[SPEC] pipelines and retry buffers flushed");
}

void World::audit_credits() {
  for (int i = 0; i < 2; ++i) {
    for (const auto& problem : credit[i].audit()) credit_problems.push_back(problem);
  }
}

void World::reinit_credits() {
  audit_credits();
  credit[0].reinitialize();
  credit[1].reinitialize();
  pending_credit_return[0] = 0;
  pending_credit_return[1] = 0;
  credits_issued[0] = cfg.credit_depth;
  credits_issued[1] = cfg.credit_depth;
  credits_consumed[0] = 0;
  credits_consumed[1] = 0;
  credits_returned[0] = 0;
  credits_returned[1] = 0;
}

bool World::already_accepted(int receiver, const Flit& flit) const {
  // AXIS: each transfer is its own UCIe segment; suppress only true replays of
  // the same beat_index. MM Request/WriteData/Response keep beat 0 unless set.
  const int beat = (flit.role == FlitRole::StreamSegment || flit.axis_segment)
                       ? flit.beat_index
                       : flit.beat_index;
  return accepted[receiver].count(
             std::make_tuple(flit.logical_id, static_cast<int>(flit.role), beat)) != 0;
}

void World::remember(int receiver, const Flit& flit) {
  const int beat = flit.beat_index;
  accepted[receiver].insert(
      std::make_tuple(flit.logical_id, static_cast<int>(flit.role), beat));
}

void World::arm_replay_deadline(int side, std::uint8_t seq) {
  // [SPEC] Replay is scheduled when REPLAY_TIMEOUT_FLIT_COUNT >= 375.
  // [MODEL] One flit-time is this configuration's serialization delay, including
  // an active throttle or degradation scale. The count is the spec input.
  const double flit_ns = serialization().to_seconds() * 1e9;
  retry[side].set_deadline(seq, now_ns() + cfg.replay_timeout_flits * flit_ns);
  timer_ev[side].notify(sc_core::SC_ZERO_TIME);
}

void World::acquire_tx_slot(int side, const Flit& flit) {
  if (side != 0 || !is_payload(flit.role)) return;
  const int guest = static_cast<int>(flit.guest_id);
  // Replays retransmit an already-outstanding flit; do not double-count.
  if (!flit.replay && guest >= 0 && guest < 3) ++outstanding_by_guest[guest];
  resources.retry_buffer_peak = std::max(resources.retry_buffer_peak, retry[side].size());
  if (resources.tx_slot_owner < 0 || flit.replay) {
    resources.tx_slot_owner = guest;
    resources.tx_slot_is_replay = flit.replay;
    if (resources.tx_slot_acquired_ns < 0) resources.tx_slot_acquired_ns = now_ns();
  }
}

void World::release_tx_slot(int side, const Flit& flit, bool was_replay) {
  if (side != 0 || !is_payload(flit.role)) return;
  const int guest = static_cast<int>(flit.guest_id);
  if (guest >= 0 && guest < 3 && outstanding_by_guest[guest] > 0) --outstanding_by_guest[guest];
  if (resources.tx_slot_owner == guest && resources.tx_slot_acquired_ns >= 0) {
    const double held = now_ns() - resources.tx_slot_acquired_ns;
    if (guest >= 0 && guest < 3) {
      resources.tx_slot_hold_ns[guest] += held;
      if (was_replay || resources.tx_slot_is_replay) resources.tx_slot_hold_replay_ns[guest] += held;
    }
    resources.tx_slot_owner = -1;
    resources.tx_slot_is_replay = false;
    resources.tx_slot_acquired_ns = -1;
  }
  // If another payload remains outstanding, re-arm the owner from the retry snapshot.
  if (!retry[side].empty() && resources.tx_slot_owner < 0) {
    for (const auto& kept : retry[side].snapshot()) {
      if (!is_payload(kept.role)) continue;
      resources.tx_slot_owner = static_cast<int>(kept.guest_id);
      resources.tx_slot_is_replay = kept.replay;
      resources.tx_slot_acquired_ns = now_ns();
      break;
    }
  }
}

bool World::uses_per_guest_seq() const {
  const auto& mit = scenario.mitigation;
  // Per-guest sequence spaces are required whenever Guest B may transmit while
  // Guest A still holds an unacknowledged flit on the shared link.
  return mit.per_guest_tx_slots || mit.virtual_channels || mit.replay_yield_after >= 0 ||
         mit.reserved_safety_bandwidth;
}

bool World::allow_slot_bypass(const Flit& candidate) const {
  if (candidate.guest_id != 1) return false;
  const auto& mit = scenario.mitigation;
  if (mit.per_guest_tx_slots || mit.virtual_channels) return true;
  if (mit.reserved_safety_bandwidth && reserved_countdown <= 0 && safety_guest_waiting(0)) return true;
  if (mit.replay_yield_after >= 0 && resources.guest_a_replay_attempts >= mit.replay_yield_after &&
      safety_guest_waiting(0)) {
    return true;
  }
  return false;
}

bool World::tx_slot_blocks(int side, const Flit& candidate) const {
  // [MODEL] Optional single-inflight mode (old study default). The Torsten
  // reference architecture disables this so TX FIFO / retry / credits govern
  // outstanding depth instead of a hard one-flit window.
  if (cfg.single_inflight_slot) {
    if (side != 0) return !retry[side].empty();
    const int guest = static_cast<int>(candidate.guest_id);
    if (allow_slot_bypass(candidate) || scenario.mitigation.per_guest_tx_slots ||
        scenario.mitigation.virtual_channels) {
      if (guest >= 0 && guest < 3) return outstanding_by_guest[guest] > 0;
    }
    return !retry[side].empty();
  }
  // Multi-outstanding: block only at the unacknowledged / retry-buffer cap.
  if (side == 0 && (allow_slot_bypass(candidate) || scenario.mitigation.per_guest_tx_slots ||
                    scenario.mitigation.virtual_channels)) {
    const int guest = static_cast<int>(candidate.guest_id);
    if (guest >= 0 && guest < 3) {
      const int per_guest_cap = scenario.guest_outstanding.size() > static_cast<std::size_t>(guest)
                                    ? scenario.guest_outstanding[guest]
                                    : max_unacked();
      return outstanding_by_guest[guest] >= std::max(1, per_guest_cap);
    }
  }
  return static_cast<int>(retry[side].size()) >= max_unacked();
}

bool World::safety_guest_waiting(int side) const {
  if (side != 0) return false;
  for (const auto& tx : txns) {
    if (tx.guest_id != 1) continue;
    if (tx.visible.classified || tx.visible.axi_completed_ok || tx.visible.axi_completed_err) continue;
    if (tx.criticality == Criticality::SafetyCritical) return true;
  }
  return false;
}

bool World::prefer_safety_now() const {
  const auto& mit = scenario.mitigation;
  if (mit.safety_priority_on_replay && safety_guest_waiting(0)) return true;
  if (mit.reserved_safety_bandwidth && reserved_countdown <= 0 && safety_guest_waiting(0)) return true;
  if (mit.replay_yield_after >= 0 && resources.guest_a_replay_attempts >= mit.replay_yield_after &&
      safety_guest_waiting(0)) {
    return true;
  }
  return false;
}

void World::attribute_guest_b_wait(WaitCause cause, double start_ns, double end_ns) {
  if (end_ns <= start_ns) return;
  const double dt = end_ns - start_ns;
  Transaction* b = nullptr;
  for (auto& tx : txns) {
    if (tx.guest_id == 1 && !tx.visible.classified) {
      b = &tx;
      break;
    }
  }
  if (!b) return;
  switch (cause) {
    case WaitCause::Arbitration: b->wait_arbitration_ns += dt; break;
    case WaitCause::GuestANormalTraffic: b->wait_guest_a_normal_ns += dt; break;
    case WaitCause::GuestAReplay: b->wait_guest_a_replay_ns += dt; break;
    case WaitCause::OwnReplay: b->wait_own_replay_ns += dt; break;
    case WaitCause::Credit: b->wait_credit_ns += dt; break;
    case WaitCause::Target: b->wait_target_ns += dt; break;
    case WaitCause::Response: b->wait_response_ns += dt; break;
    case WaitCause::Recovery: b->wait_recovery_ns += dt; break;
    case WaitCause::None: break;
  }
}

void World::fold_guest_b_waits() {
  for (const auto& tx : txns) {
    if (tx.guest_id != 1) continue;
    resources.guest_b_wait_arbitration_ns += tx.wait_arbitration_ns;
    resources.guest_b_wait_guest_a_normal_ns += tx.wait_guest_a_normal_ns;
    resources.guest_b_wait_guest_a_replay_ns += tx.wait_guest_a_replay_ns;
    resources.guest_b_wait_own_replay_ns += tx.wait_own_replay_ns;
    resources.guest_b_wait_credit_ns += tx.wait_credit_ns;
    resources.guest_b_wait_target_ns += tx.wait_target_ns;
    resources.guest_b_wait_response_ns += tx.wait_response_ns;
    resources.guest_b_wait_recovery_ns += tx.wait_recovery_ns;
  }
}

void World::record_error(const std::string& kind, const Transaction* tx, const std::string& details) {
  if (!cfg.error_logging) return;
  ErrorLogEntry entry;
  entry.time_ns = now_ns();
  entry.kind = kind;
  entry.retry_occupancy = std::max(retry[0].size(), retry[1].size());
  entry.tx_occupancy = std::max(send_q[0].size(), send_q[1].size());
  entry.rx_occupancy = std::max(arrive[0].size(), arrive[1].size());
  entry.credit_available = credit[0].available();
  entry.credit_held = credit[0].held();
  if (tx) {
    entry.transaction_id = tx->logical_id;
    entry.retry_count = tx->retry_count;
  }
  entry.details = details;
  error_log.push_back(entry);
  sample("error_log", error_log.size());
}

void World::note_credit_freed(int side) {
  // [REFERENCE-SLIDE] Early credit return: space is freed when the RX path
  // accepts a flit. Credits are batched until credit_update_threshold.
  ++pending_credit_return[side];
}

int World::take_credit_update(int side) {
  const int threshold = std::max(1, cfg.credit_update_threshold);
  if (pending_credit_return[side] < threshold) return 0;
  return take_credit_update_force(side);
}

int World::take_credit_update_force(int side) {
  const int n = pending_credit_return[side];
  pending_credit_return[side] = 0;
  if (n > 0) credits_returned[side] += n;
  return n;
}

void World::capture_recovery_snapshot(const std::string& transition, const std::string& classification) {
  RecoverySnapshot snap;
  snap.transition = transition;
  snap.time_ns = now_ns();
  snap.tx_fifo = send_q[0].size() + send_q[1].size();
  snap.rx_fifo = arrive[0].size() + arrive[1].size();
  snap.retry_buffer = retry[0].size() + retry[1].size();
  snap.credit_available = credit[0].available() + credit[1].available();
  snap.credit_held = credit[0].held() + credit[1].held();
  for (const auto& tx : txns) {
    if (tx.visible.axi_accepted && !tx.visible.classified && !tx.visible.axi_completed_ok &&
        !tx.visible.axi_completed_err) {
      ++snap.outstanding_axi;
    }
    if (tx.ground.response_created && !tx.visible.axi_completed_ok && !tx.visible.axi_completed_err &&
        !tx.visible.classified) {
      ++snap.outstanding_responses;
    }
  }
  snap.classification = classification;
  recovery_snapshots.push_back(snap);
}

bool World::mainband_active() const {
  return link.state == LinkState::Active && !in_l1 && !link.busy;
}

std::uint32_t World::allocate(const Issue& issue, int issue_index) {
  if (txns.size() + 1 > txns.capacity()) {
    throw std::logic_error("transaction pool exceeded the reserved capacity");
  }
  Transaction tx;
  tx.logical_id = next_logical_id++;
  if (issue.axi_id >= 0) {
    tx.axi_id = static_cast<std::uint32_t>(issue.axi_id);
  } else if (issue.reuse_prior_axi_id && !txns.empty()) {
    tx.axi_id = txns.back().axi_id;
  } else {
    tx.axi_id = static_cast<std::uint32_t>(tx.logical_id & 0xffff);
  }
  tx.guest_id = static_cast<std::uint32_t>(issue.guest);
  tx.op = issue.op;
  tx.address = issue.address;
  tx.data = issue.data;
  tx.is_write = !is_read(issue.op);
  tx.idempotent = is_idempotent(issue.op);
  tx.criticality = issue.criticality;
  tx.issue_index = issue_index;
  tx.t_created_ns = now_ns();
  if (issue.deadline_ns >= 0) tx.deadline_ns = tx.t_created_ns + issue.deadline_ns;
  tx.phase = Phase::Created;
  // [MODEL ASSUMPTION] Default burst length 1; read-burst experiments override.
  tx.proto.burst_len = std::max(1, issue.burst_len);
  tx.bridge.token = tx.logical_id;
  tx.bridge.generation = 1;
  const bool want_axi5 = issue.axi5_enabled || issue.axi5_form != Axi5AtomicForm::None ||
                         issue.axi5_wpoison || issue.axi5_awtrace || issue.axi5_wtrace ||
                         issue.axi5_exclusive;
  if (want_axi5 && !issue.axis_enabled) {
    tx.axi5.enabled = true;
    tx.axi5.form = issue.axi5_form;
    tx.axi5.atomic_op = issue.axi5_atomic_op;
    tx.axi5.awatop = issue.axi5_awatop != 0
                         ? issue.axi5_awatop
                         : (issue.axi5_form == Axi5AtomicForm::AtomicStore ? kAwatopAtomicStoreAddLe
                                                                           : kAwatopNonAtomic);
    tx.axi5.wpoison = issue.axi5_wpoison;
    tx.axi5.awtrace = issue.axi5_awtrace && cfg.axi5_trace_signals_present;
    tx.axi5.wtrace = issue.axi5_wtrace && cfg.axi5_trace_signals_present;
    tx.axi5.poison_policy = cfg.poison_target_policy;
    tx.axi5.operand = issue.data;
    tx.axi5.logical_op_id =
        issue.axi5_logical_op_id != 0 ? issue.axi5_logical_op_id : tx.logical_id;
    tx.axi5.transfer_attempt = 0;
    // Hard rule: Trace must not enter classify — flag records the config intent.
    tx.axi5.trace_used_in_classify = false;
    if (cfg.axi5_use_trace_in_classify) {
      // Refuse silent misconfiguration: keep false and log via unsupported.
      tx.axi5.unsupported_reason = "axi5_use_trace_in_classify must remain false in AXI5-MM-P1";
    }
    if (issue.axi5_form == Axi5AtomicForm::AtomicLoad ||
        issue.axi5_form == Axi5AtomicForm::AtomicSwap ||
        issue.axi5_form == Axi5AtomicForm::AtomicCompare || issue.axi5_exclusive) {
      tx.axi5.unsupported_form = true;
      tx.axi5.unsupported_reason = "out of AXI5-MM-P1 scope (AtomicLoad/Swap/Compare/Exclusive)";
    }
    if (issue.axi5_form == Axi5AtomicForm::AtomicStore) {
      tx.is_write = true;
      tx.idempotent = false;
      if (tx.address == 0 || tx.address == 0x1000) {
        tx.address = cfg.axi5_atomic_address;
      }
    }
  }
  if (issue.axis_enabled || cfg.axis_profile_enabled) {
    tx.axis.enabled = true;
    tx.axis.sink = issue.axis_sink;
    tx.axis.continuous_packets = true;
    tx.axis.beat_count = std::max(1, issue.axis_beats);
    tx.axis.data_width_bytes = std::max(1, cfg.axis_tdata_bytes);
    tx.axis.last_beat_valid_bytes =
        std::min(tx.axis.data_width_bytes, std::max(1, issue.axis_last_valid_bytes));
    tx.axis.tid = issue.axis_tid;
    tx.axis.tdest = issue.axis_tdest;
    tx.axis.tuser = issue.axis_tuser;
    tx.axis.app_ack_mode =
        (issue.axis_app_ack || cfg.axis_app_ack_enabled) ? AxisAppAckMode::SeparateReturn
                                                         : AxisAppAckMode::Off;
    tx.axis.packet_id_enabled = issue.axis_packet_id || cfg.axis_packet_id_enabled;
    tx.axis.packet_op_id =
        issue.axis_packet_op_id != 0 ? issue.axis_packet_op_id : tx.logical_id;
    tx.proto.burst_len = tx.axis.beat_count;
    tx.is_write = true;
    tx.axis.offers.reserve(static_cast<std::size_t>(tx.axis.beat_count));
    for (int b = 0; b < tx.axis.beat_count; ++b) {
      AxisBeatOffer offer;
      offer.beat_index = b;
      offer.tdata = static_cast<std::uint64_t>(tx.data) + static_cast<std::uint64_t>(b);
      offer.tkeep = tx.axis.keep_for_beat(b);
      offer.tlast = (b + 1 == tx.axis.beat_count);
      offer.tid = tx.axis.tid;
      offer.tdest = tx.axis.tdest;
      offer.tuser = tx.axis.tuser;
      offer.valid_bytes = tx.axis.valid_bytes_for_beat(b);
      if (issue.axis_force_tid_change_mid && b > 0) {
        offer.tid = static_cast<std::uint8_t>(tx.axis.tid + 1);
      }
      tx.axis.offers.push_back(offer);
    }
  }
  txns.push_back(tx);
  partial.push_back(Partial{});
  auto& stored = txns.back();
  std::ostringstream sentence;
  sentence << "Guest " << static_cast<char>('A' + issue.guest) << " creates T" << stored.logical_id << " "
           << to_string(issue.op);
  if (stored.axis.enabled) {
    sentence << " AXIS beats=" << stored.axis.beat_count << " sink="
             << (stored.axis.sink == AxisSinkKind::Imm ? "IMM" : "PKT");
  }
  log(TraceLevel::Transactions, "create", "Guest", &stored, sentence.str());
  progress.notify(sc_core::SC_ZERO_TIME);
  return static_cast<std::uint32_t>(txns.size() - 1);
}

void World::on_phase(Transaction& tx, Phase phase) {
  if (!link.fault_time_set && phase != Phase::Created) {
    // The first armed fault below overwrites this if it is the real fault.
  }
  for (auto& spec : scenario.faults) {
    if (spec.fired || spec.trigger != Trigger::Phase) continue;
    if (spec.phase != phase) continue;
    if (spec.issue_index >= 0 && spec.issue_index != tx.issue_index) continue;
    spec.fired = true;
    if (!link.fault_time_set) {
      link.fault_time_set = true;
      link.fault_time_ns = now_ns();
    }
    std::ostringstream why;
    why << "fault " << static_cast<int>(spec.kind) << " at " << to_string(phase);
    log(TraceLevel::Transactions, "fault", "Injector", &tx, why.str());

    switch (spec.kind) {
      case FaultKind::RepeatedCorruption:
      case FaultKind::CorruptedResponse:
      case FaultKind::CorruptedRequest:
        arm.persistent = true;
        arm.role = spec.role;
        arm.bits = spec.corrupt_bits;
        arm.logical_id = tx.logical_id;
        arm.any_id = false;
        link.fail_retrain = spec.fail_retrain;
        break;
      case FaultKind::SingleBit:
        arm.oneshot = 1;
        arm.role = spec.role;
        arm.bits = spec.corrupt_bits;
        arm.logical_id = tx.logical_id;
        break;
      case FaultKind::BurstError:
        arm.storm_naks = spec.storm_naks;
        arm.storm_guest = spec.guest;
        break;
      case FaultKind::DroppedFlit:
        arm.drop_one = true;
        arm.role = spec.role;
        arm.logical_id = tx.logical_id;
        break;
      case FaultKind::DuplicatedFlit:
        arm.duplicate_one = true;
        arm.role = spec.role;
        arm.logical_id = tx.logical_id;
        break;
      case FaultKind::DelayedFlit:
      case FaultKind::IncreasedLatency:
      case FaultKind::SidebandDelay:
        arm.extra_delay_ns = spec.extra_delay_ns > 0 ? spec.extra_delay_ns : 1000;
        arm.role = spec.role;
        arm.logical_id = tx.logical_id;
        break;
      case FaultKind::AckNakLoss:
      case FaultKind::RetryTimeout:
        arm.suppress_ack = true;
        arm.persistent = spec.persistent;
        break;
      case FaultKind::ForcedRetrain:
      case FaultKind::RetryExhaustion:
        link.fail_retrain = spec.fail_retrain;
        request(RecoveryCmd::Retrain);
        break;
      case FaultKind::ForcedLinkReset:
        request(RecoveryCmd::LinkReset);
        break;
      case FaultKind::ForcedLinkError:
      case FaultKind::EndpointReset:
        request(RecoveryCmd::LinkError);
        break;
      case FaultKind::RemoteUnresponsive:
        link.remote_recovers = false;
        request(RecoveryCmd::RemoteUnresponsive);
        break;
      case FaultKind::EmergencyShutdown:
        link.emergency = true;
        tx.visible.emergency_shutdown = true;
        request(RecoveryCmd::EmergencyShutdown);
        break;
      case FaultKind::FastThrottle:
        // [SPEC] Fast Throttle does not by itself stop mainband traffic
        // (UCIe 3.0 §8.4.2.1, as used by the transaction-level study).
        // [MODEL] Effective serialization and propagation scale by throttle_factor.
        // The factor is not a specified UCIe rate.
        link.rate_scale *= spec.throttle_factor;
        link.fast_throttle = true;
        tx.visible.fast_throttle = true;
        log(TraceLevel::Summary, "fast_throttle", "Safety", &tx,
            "[MODEL] link rate scale is now " + std::to_string(link.rate_scale));
        break;
      case FaultKind::RateReduction:
        if (spec.bandwidth_factor > 0 && spec.bandwidth_factor < 1) {
          link.rate_scale /= spec.bandwidth_factor;
        }
        log(TraceLevel::Summary, "degraded", "Link", &tx,
            "[MODEL] bandwidth factor applied, rate scale " + std::to_string(link.rate_scale));
        break;
      case FaultKind::CreditLoss:
      case FaultKind::CreditCorruption:
        credit[0].inject_lose();
        break;
      case FaultKind::TargetSlowdown:
        cfg.target_commit_cycles += 20;
        break;
      case FaultKind::EnterL1:
        request(RecoveryCmd::EnterL1);
        break;
      case FaultKind::ExitL1:
        request(RecoveryCmd::ExitL1);
        break;
      default:
        break;
    }
  }

  if (scenario.random_faults && !link.fault_time_set && phase == Phase::Transmitting) {
    link.fault_time_set = true;
    link.fault_time_ns = now_ns();
  }
}

void World::apply_tx(Flit& flit) {
  if (!is_payload(flit.role)) return;
  // [REFERENCE-SLIDE] CRC may be disabled for overhead comparison only.
  if (!cfg.crc_enabled) {
    flit.crc_ok = true;
    flit.corrupt_bits = 0;
    return;
  }
  const bool id_match = arm.any_id || arm.logical_id == 0 || arm.logical_id == flit.logical_id;
  if (arm.persistent && id_match && flit.role == arm.role) {
    flit.crc_ok = false;
    flit.corrupt_bits = arm.bits;
  }
  if (arm.oneshot > 0 && id_match && flit.role == arm.role) {
    flit.crc_ok = false;
    flit.corrupt_bits = arm.bits;
    --arm.oneshot;
  }
  if (arm.storm_naks > 0 &&
      (arm.storm_guest < 0 || flit.guest_id == static_cast<std::uint32_t>(arm.storm_guest))) {
    const auto key = std::make_pair(flit.logical_id, static_cast<int>(flit.role));
    int& hits = storm_hits[key];
    if (hits < arm.storm_naks) {
      ++hits;
      flit.crc_ok = false;
      flit.corrupt_bits = 1;
    }
  }
  if (arm.drop_one && id_match && flit.role == arm.role) {
    flit.drop = true;
    arm.drop_one = false;
  }
  if (arm.duplicate_one && id_match && flit.role == arm.role) {
    flit.duplicate = true;
    arm.duplicate_one = false;
  }
  if (arm.extra_delay_ns > 0 && id_match && flit.role == arm.role) {
    flit.extra_delay_ns = arm.extra_delay_ns;
    arm.extra_delay_ns = 0;
  }
  if (scenario.random_faults) {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    if (dist(rng) < cfg.random_fault_probability) {
      flit.crc_ok = false;
      flit.corrupt_bits = 1;
    }
  }
  (void)is_control;
}

void World::note(Transaction& tx, Phase phase, const char* component, const std::string& sentence) {
  tx.phase = phase;
  const double now = now_ns();
  switch (phase) {
    case Phase::Created: tx.t_created_ns = now; break;
    case Phase::AxiAccepted: tx.t_axi_accepted_ns = now; break;
    case Phase::AouPacked: tx.t_aou_packed_ns = now; break;
    case Phase::Transmitting: tx.t_ucie_tx_ns = now; break;
    case Phase::RemoteReceived: tx.t_remote_rx_ns = now; break;
    case Phase::TargetAccepted:
      tx.t_target_accepted_ns = now;
      tx.ground.target_accepted = true;
      break;
    case Phase::ArchitecturalCommit:
      // Keep the earliest irreversible effect. A later automatic retry must
      // not move the start of the ambiguity window.
      if (tx.ground.commit_time_ns < 0) tx.ground.commit_time_ns = now;
      tx.ambiguity_open = true;
      break;
    case Phase::ResponseCreated:
      tx.t_response_created_ns = now;
      tx.ground.response_created = true;
      break;
    case Phase::ResponseTransmitting: tx.t_response_tx_ns = now; break;
    case Phase::ResponseReceived: tx.t_completion_ns = -1; break;
    case Phase::Completed: tx.t_completion_ns = now; break;
    default: break;
  }
  log(TraceLevel::Transactions, "phase", component, &tx, sentence);
  on_phase(tx, phase);
  progress.notify(sc_core::SC_ZERO_TIME);
}

}  // namespace ucie
