#include "world.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace ucie {
namespace {

bool is_payload(FlitRole role) {
  return role == FlitRole::Request || role == FlitRole::WriteData || role == FlitRole::Response;
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

void World::classify_now(Transaction& tx) {
  if (tx.visible.status_has_axi_id || tx.visible.status_has_commit_bit) {
    internal_errors.push_back("standardized status carried an AXI id or a commit bit");
  }
  tx.visible.fate_a = classify(view_for(ObserverProfile::A_AxiOnly, tx));
  tx.visible.fate_b = classify(view_for(ObserverProfile::B_AxiFdi, tx));
  tx.visible.fate_c = classify(view_for(ObserverProfile::C_AxiFdiUcieStatus, tx));
  tx.visible.fate_d = classify(view_for(ObserverProfile::D_AxiHypervisorMetadata, tx));
  // B and C observe extra signals, but those signals are not allowed to
  // change the label. A difference here means the view builder leaked.
  if (tx.visible.fate_b != tx.visible.fate_a || tx.visible.fate_c != tx.visible.fate_a) {
    internal_errors.push_back("observer B or C diverged from observer A");
  }
  tx.visible.classified = true;
  tx.recovery_epoch = static_cast<int>(link.epoch);
}

void World::complete(Transaction& tx) {
  if (tx.visible.axi_completed_ok) {
    tx.visible.duplicate_completion = true;
    log(TraceLevel::Errors, "duplicate_completion", "AXI", &tx, "second completion refused");
    return;
  }
  tx.visible.axi_completed_ok = true;
  tx.completion_received = true;
  tx.visible.stopped = false;
  tx.t_completion_ns = now_ns();
  tx.phase = Phase::Completed;
  if (tx.ground.commit_time_ns >= 0) {
    tx.ambiguity_window_ns = tx.t_completion_ns - tx.ground.commit_time_ns;
    // A single commit closed by one OKAY is determined. A repeated commit is
    // not: the OKAY does not say which attempt took effect.
    tx.ambiguity_open = tx.ground.architectural_commit_count != 1;
    tx.ambiguity_ended_unknown = false;
  }
  if (tx.deadline_ns >= 0 && tx.t_completion_ns > tx.deadline_ns) tx.visible.deadline_violated = true;
  classify_now(tx);
  log(TraceLevel::Transactions, "complete", "AXI", &tx,
      "initiator completion, observer A " + std::string(to_string(tx.visible.fate_a)));
  progress.notify(sc_core::SC_ZERO_TIME);
}

void World::abandon(Transaction& tx, bool fdi, bool explicit_unknown) {
  if (tx.visible.classified || tx.visible.axi_completed_ok) return;
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

void World::observe_link_down() {
  // FDI can be sampled when the link enters the destructive state. The 16 ms
  // residency is how long that state lasts, not how long the initiator must
  // wait before the state is visible.
  const double now = now_ns();
  for (auto& tx : txns) {
    if (tx.visible.classified || tx.visible.axi_completed_ok || tx.t_created_ns < 0) continue;
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
  for (auto& tx : txns) {
    if (tx.visible.classified || tx.visible.axi_completed_ok) continue;
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
    if (tx.ground.response_created && !tx.visible.axi_completed_ok) {
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
  return accepted[receiver].count({flit.logical_id, static_cast<int>(flit.role)}) != 0;
}

void World::remember(int receiver, const Flit& flit) {
  accepted[receiver].insert({flit.logical_id, static_cast<int>(flit.role)});
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
    if (tx.visible.classified || tx.visible.axi_completed_ok) continue;
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
    if (tx.visible.axi_accepted && !tx.visible.classified && !tx.visible.axi_completed_ok) {
      ++snap.outstanding_axi;
    }
    if (tx.ground.response_created && !tx.visible.axi_completed_ok && !tx.visible.classified) {
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
  tx.axi_id = static_cast<std::uint32_t>(tx.logical_id & 0xffff);
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
  txns.push_back(tx);
  partial.push_back(Partial{});
  auto& stored = txns.back();
  std::ostringstream sentence;
  sentence << "Guest " << static_cast<char>('A' + issue.guest) << " creates T" << stored.logical_id << " "
           << to_string(issue.op);
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
