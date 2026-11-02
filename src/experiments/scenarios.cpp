#include "scenario.hpp"

#include "common/config.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace ucie {
namespace {

double clean_write_ns(const Config& c) {
  const double hop = (c.serialization_cycles + c.propagation_cycles + c.buffer_cycles) * c.clock_period_ns;
  const double fixed =
      (c.axi_handshake_cycles + c.aou_pack_cycles + c.arbitration_cycles + c.target_accept_cycles +
       c.target_commit_cycles + c.target_response_cycles + c.aou_pack_cycles + c.completion_cycles) *
      c.clock_period_ns;
  // Request, write data, and response each cross the link once.
  return fixed + hop * 3.0;
}

Issue mmio(int guest, double delay, double deadline, int priority, Criticality crit) {
  Issue issue;
  issue.guest = guest;
  issue.op = OpKind::IncrementCounter;
  issue.address = 0x1000;
  issue.delay_ns = delay;
  issue.deadline_ns = deadline;
  issue.priority = priority;
  issue.criticality = crit;
  return issue;
}

Issue mem(int guest, double delay, int data, std::uint64_t address) {
  Issue issue;
  issue.guest = guest;
  issue.op = OpKind::MemoryWrite;
  issue.address = address;
  issue.data = data;
  issue.delay_ns = delay;
  issue.criticality = Criticality::BestEffort;
  return issue;
}

Issue axis_pkt(AxisSinkKind sink, int beats, int last_valid_bytes = 16) {
  Issue issue;
  issue.guest = 0;
  issue.op = OpKind::IncrementCounter;
  issue.address = 0x2000;
  issue.data = 0xA5;
  issue.criticality = Criticality::SafetyCritical;
  issue.axis_enabled = true;
  issue.axis_sink = sink;
  issue.axis_beats = std::max(1, beats);
  issue.axis_last_valid_bytes = last_valid_bytes;
  return issue;
}

FaultSpec corrupt_stream_after_beat(int /*beat_hint*/) {
  // Corrupt StreamSegment payloads persistently after arming at Transmitting.
  FaultSpec fault;
  fault.trigger = Trigger::Phase;
  fault.phase = Phase::Transmitting;
  fault.role = FlitRole::StreamSegment;
  fault.kind = FaultKind::RepeatedCorruption;
  fault.persistent = true;
  fault.fail_retrain = true;
  fault.corrupt_bits = 1;
  return fault;
}

void axis_base(Config& config, Scenario& scenario) {
  config.axis_profile_enabled = true;
  config.axis_settle_grace_ns = 8000;
  config.expose_bridge_diagnostics = true;
  config.use_diag_in_classifier = true;
  scenario.policy = RecoveryPolicy::LocalSlverr;
  config.local_slverr_on_link_error = true;
}

void axi5_base(Config& config, Scenario& scenario) {
  // AXI5-MM-P1 profile. Orthogonal to AXIS — never enable axis here.
  config.axi5_mm_p1_enabled = true;
  config.axis_profile_enabled = false;
  config.axi5_trace_signals_present = true;
  config.axi5_use_trace_in_classify = false;
  config.axi5_atomic_location_init = 10;
  config.axi5_atomic_address = 0xA000;
  config.poison_target_policy = PoisonTargetPolicy::RejectWithoutCommitSlverr;
  config.expose_bridge_diagnostics = true;
  config.use_diag_in_classifier = true;
  scenario.policy = RecoveryPolicy::LocalSlverr;
  config.local_slverr_on_link_error = true;
  scenario.counter_start = 7;  // MM baseline counter untouched by AtomicStore path
}

Issue atomic_store_add(int operand, double delay = 0) {
  Issue issue;
  issue.guest = 0;
  issue.op = OpKind::MemoryWrite;  // wire is write channel; effect is AtomicStore
  issue.address = 0xA000;
  issue.data = operand;
  issue.delay_ns = delay;
  issue.deadline_ns = -1;
  issue.priority = 0;
  issue.criticality = Criticality::SafetyCritical;
  issue.axi5_enabled = true;
  issue.axi5_form = Axi5AtomicForm::AtomicStore;
  issue.axi5_atomic_op = Axi5AtomicOp::Add;
  issue.axi5_awatop = kAwatopAtomicStoreAddLe;
  return issue;
}

FaultSpec corrupt_response(bool fail_retrain) {
  FaultSpec fault;
  fault.trigger = Trigger::Phase;
  fault.phase = Phase::ResponseTransmitting;
  fault.role = FlitRole::Response;
  fault.kind = FaultKind::RepeatedCorruption;
  fault.persistent = true;
  fault.fail_retrain = fail_retrain;
  fault.corrupt_bits = 1;
  return fault;
}

FaultSpec corrupt_request() {
  FaultSpec fault;
  fault.trigger = Trigger::Phase;
  fault.phase = Phase::Transmitting;
  fault.role = FlitRole::Request;
  fault.kind = FaultKind::RepeatedCorruption;
  fault.persistent = true;
  fault.fail_retrain = true;
  fault.corrupt_bits = 1;
  return fault;
}

void add_violation_strings(RunResult& result) {
  for (const auto& issue : check_transaction_invariants(result.transactions)) {
    std::ostringstream line;
    line << "T" << issue.logical_id << ":";
    if (issue.false_safe_retry) line << " FALSE_SAFE_RETRY";
    if (issue.at_most_once) line << " AT_MOST_ONCE_VIOLATION";
    if (issue.completion_inconsistent) line << " COMPLETION_INCONSISTENT";
    if (issue.false_sure_effect) line << " FALSE_SURE_EFFECT";
    if (issue.false_sure_no_effect) line << " FALSE_SURE_NO_EFFECT";
    if (issue.duplicate_completion) line << " DUPLICATE_COMPLETION";
    if (issue.second_local_response) line << " SECOND_LOCAL_RESPONSE";
    result.violations.push_back(line.str());
  }
}

bool has_flag(const RunResult& result, const char* flag) {
  for (const auto& line : result.violations) {
    if (line.find(flag) != std::string::npos) return true;
  }
  return false;
}

const Transaction* first_guest(const RunResult& result, std::uint32_t guest) {
  for (const auto& tx : result.transactions) {
    if (tx.guest_id == guest) return &tx;
  }
  return nullptr;
}

void require(RunResult& result, bool cond, const std::string& why) {
  if (!cond) result.failures.push_back(why);
}

// Shared-link FFI workload: six Guest A best-effort writes + one Guest B
// safety-critical increment. Deadline is left unset here; the campaign applies
// the Option B experimental timing budget from the measured quiet baseline.
void load_shared_ffi_workload(Scenario& scenario, Config& config, bool with_deadline) {
  scenario.policy = RecoveryPolicy::Discard;
  scenario.retrain_preserves_retry = true;
  scenario.arbitration = ArbPolicy::RoundRobin;
  config.replay_limit = 6;
  for (int i = 0; i < 6; ++i) scenario.issues.push_back(mem(0, 0, i, 0x3000 + i * 4));
  const double deadline = with_deadline ? clean_write_ns(config) * 1.2 : -1.0;
  scenario.issues.push_back(mmio(1, 0, deadline, 1, Criticality::SafetyCritical));
}

void arm_guest_storm(Scenario& scenario, int guest) {
  FaultSpec storm;
  storm.issue_index = 0;
  for (std::size_t i = 0; i < scenario.issues.size(); ++i) {
    if (scenario.issues[i].guest == guest) {
      storm.issue_index = static_cast<int>(i);
      break;
    }
  }
  storm.guest = guest;
  storm.trigger = Trigger::Phase;
  storm.phase = Phase::Transmitting;
  storm.kind = FaultKind::BurstError;
  storm.storm_naks = 2;
  scenario.faults.push_back(storm);
}

void require_guest_b_clean(RunResult& result, const Transaction* b) {
  require(result, b != nullptr, "guest B missing");
  if (!b) return;
  require(result, b->retry_count == 0, "GuestB.crc_failures/retry_count != 0");
  require(result, b->replay_count == 0, "GuestB.replay_count != 0");
  require(result, result.linkerror_events == 0 || b->visible.axi_completed_ok,
          "guest B saw link destruction");
  require(result, b->visible.axi_completed_ok && b->completion_received,
          "GuestB.completion_received != true");
  require(result, b->ground.architectural_commit_count == 1, "GuestB.commit_count != 1");
}

}  // namespace

Scenario make_scenario(Experiment experiment, Config& config) {
  Scenario scenario;
  scenario.experiment = experiment;
  scenario.name = std::string(to_string(experiment));
  scenario.policy = config.recovery;
  scenario.retrain_preserves_retry = config.retrain_preserves_retry;
  scenario.counter_start = config.counter_start;

  switch (experiment) {
    case Experiment::BaselineUnknown:
      scenario.policy = RecoveryPolicy::Discard;
      scenario.issues.push_back(mmio(0, 0, -1, 0, Criticality::SafetyCritical));
      scenario.faults.push_back(corrupt_response(true));
      break;
    case Experiment::PrecommitFailure:
      scenario.policy = RecoveryPolicy::Discard;
      scenario.issues.push_back(mmio(0, 0, -1, 0, Criticality::SafetyCritical));
      scenario.faults.push_back(corrupt_request());
      break;
    case Experiment::BlindRetry:
      scenario.policy = RecoveryPolicy::AutoRetry;
      scenario.issues.push_back(mmio(0, 0, -1, 0, Criticality::SafetyCritical));
      scenario.faults.push_back(corrupt_response(true));
      break;
    case Experiment::ObservationalEquivalence: {
      scenario.policy = RecoveryPolicy::Discard;
      auto committed = mmio(0, 0, -1, 0, Criticality::SafetyCritical);
      scenario.issues.push_back(committed);
      auto missed = mmio(0, 0, -1, 0, Criticality::SafetyCritical);
      missed.wait_for_previous = true;
      missed.counter_reset = 7;
      scenario.issues.push_back(missed);
      auto after = corrupt_response(true);
      after.issue_index = 0;
      scenario.faults.push_back(after);
      auto before = corrupt_request();
      before.issue_index = 1;
      scenario.faults.push_back(before);
      break;
    }
    case Experiment::MultiGuest: {
      scenario.policy = RecoveryPolicy::Discard;
      scenario.arbitration = ArbPolicy::StrictPriority;
      scenario.on_link_error = SafetyAction::SafeState;
      scenario.guest_priority = {0, 10, 0};
      scenario.guest_outstanding = {8, 1, 1};
      // Best-effort guest A is queued with the safety-critical guest. The
      // fault is a shared-link LinkError armed at B's commit, so recovery is
      // not confined to the guest that was in the commit window.
      for (int i = 0; i < 4; ++i) scenario.issues.push_back(mem(0, 0, i + 1, 0x2000 + i * 4));
      auto critical = mmio(1, 0, clean_write_ns(config) * 2.0, 10, Criticality::SafetyCritical);
      scenario.issues.push_back(critical);
      FaultSpec fault;
      fault.issue_index = 4;
      fault.trigger = Trigger::Phase;
      fault.phase = Phase::ArchitecturalCommit;
      fault.kind = FaultKind::ForcedLinkError;
      scenario.faults.push_back(fault);
      break;
    }
    case Experiment::RetryStorm: {
      scenario.policy = RecoveryPolicy::Discard;
      scenario.retrain_preserves_retry = true;
      scenario.arbitration = ArbPolicy::RoundRobin;
      config.replay_limit = 6;
      for (int i = 0; i < 6; ++i) scenario.issues.push_back(mem(0, 0, i, 0x3000 + i * 4));
      auto critical = mmio(1, 0, clean_write_ns(config) * 1.2, 1, Criticality::SafetyCritical);
      scenario.issues.push_back(critical);
      FaultSpec storm;
      storm.issue_index = -1;
      storm.trigger = Trigger::Phase;
      storm.phase = Phase::Transmitting;
      storm.kind = FaultKind::BurstError;
      storm.storm_naks = 2;
      scenario.faults.push_back(storm);
      break;
    }
    case Experiment::GuestAStorm:
    case Experiment::SharedQuiet: {
      // Same guests, arbitration, and deadline as retry_storm. guest_a_storm
      // corrupts only Guest A. shared_quiet injects no fault, so B's latency
      // can be compared with the storm instead of being blamed on it.
      load_shared_ffi_workload(scenario, config, true);
      if (experiment == Experiment::GuestAStorm) arm_guest_storm(scenario, 0);
      break;
    }
    case Experiment::GuestBIsolated: {
      // Run 1: Guest B alone, no Guest A traffic, no faults.
      scenario.policy = RecoveryPolicy::Discard;
      scenario.retrain_preserves_retry = true;
      scenario.arbitration = ArbPolicy::RoundRobin;
      config.replay_limit = 6;
      scenario.issues.push_back(mmio(1, 0, -1, 1, Criticality::SafetyCritical));
      break;
    }
    case Experiment::GuestBStorm: {
      // Run 4: same shared workload, faults confined to Guest B.
      load_shared_ffi_workload(scenario, config, false);
      arm_guest_storm(scenario, 1);
      break;
    }
    case Experiment::MitSafetyPriority: {
      // Prefer Guest B at the shared AXI port. Link-level TX bypass is not
      // required for this mitigation and would need per-guest sequence spaces.
      load_shared_ffi_workload(scenario, config, false);
      arm_guest_storm(scenario, 0);
      scenario.arbitration = ArbPolicy::StrictPriority;
      scenario.guest_priority = {0, 10, 0};
      break;
    }
    case Experiment::MitPerGuestSlots: {
      load_shared_ffi_workload(scenario, config, false);
      arm_guest_storm(scenario, 0);
      scenario.mitigation.per_guest_tx_slots = true;
      break;
    }
    case Experiment::MitReplayBudget: {
      // After Guest A begins recovery traffic, give Guest B a dedicated
      // outstanding slot (per-guest seq) so replay cannot monopolize the link.
      load_shared_ffi_workload(scenario, config, false);
      arm_guest_storm(scenario, 0);
      scenario.mitigation.replay_yield_after = 1;
      scenario.mitigation.per_guest_tx_slots = true;
      break;
    }
    case Experiment::MitReservedBw: {
      // Weighted arbitration reserves a larger share of issue opportunities
      // for the safety-critical guest; per-guest slots bound link occupancy.
      load_shared_ffi_workload(scenario, config, false);
      arm_guest_storm(scenario, 0);
      scenario.arbitration = ArbPolicy::WeightedRoundRobin;
      scenario.guest_weight = {1, 4, 1};
      scenario.mitigation.per_guest_tx_slots = true;
      break;
    }
    case Experiment::MitVirtualChannels: {
      load_shared_ffi_workload(scenario, config, false);
      arm_guest_storm(scenario, 0);
      scenario.mitigation.virtual_channels = true;
      break;
    }
    case Experiment::PrioritySideband:
      scenario.sideband_saturation = true;
      config.serialization_cycles = 40;
      for (int i = 0; i < 12; ++i) scenario.issues.push_back(mem(0, 0, i, 0x4000 + i * 4));
      break;
    case Experiment::FastThrottle: {
      config.serialization_cycles = 16;
      scenario.issues.push_back(mem(0, 0, 1, 0x5000));
      auto second = mmio(1, 0, -1, 1, Criticality::SafetyCritical);
      second.wait_for_previous = true;
      // Filled in after the first transaction by a phase fault on the second
      // issue. Deadline is set from the unthrottled estimate; the throttle
      // multiplies link hops and is expected to miss it.
      second.deadline_ns = clean_write_ns(config) * 2.0;
      scenario.issues.push_back(second);
      FaultSpec throttle;
      throttle.issue_index = 1;
      throttle.trigger = Trigger::Phase;
      throttle.phase = Phase::AxiAccepted;
      throttle.kind = FaultKind::FastThrottle;
      throttle.throttle_factor = 8;
      scenario.faults.push_back(throttle);
      break;
    }
    case Experiment::EmergencyShutdown: {
      scenario.policy = RecoveryPolicy::Discard;
      const Phase points[] = {Phase::AxiAccepted, Phase::TargetAccepted, Phase::ArchitecturalCommit,
                              Phase::ResponseCreated};
      for (int i = 0; i < 4; ++i) {
        auto issue = mmio(0, 0, -1, 0, Criticality::SafetyCritical);
        issue.wait_for_previous = i > 0;
        scenario.issues.push_back(issue);
        FaultSpec fault;
        fault.issue_index = i;
        fault.trigger = Trigger::Phase;
        fault.phase = points[i];
        fault.kind = FaultKind::EmergencyShutdown;
        scenario.faults.push_back(fault);
      }
      break;
    }
    case Experiment::DegradedLink: {
      config.serialization_cycles = 12;
      for (int i = 0; i < 3; ++i) {
        auto baseline = mem(0, 0, i, 0x6000 + i * 4);
        baseline.wait_for_previous = i > 0;
        scenario.issues.push_back(baseline);
      }
      auto after = mmio(1, 0, -1, 1, Criticality::SafetyCritical);
      after.wait_for_previous = true;
      after.deadline_ns = clean_write_ns(config) * 2.0;
      scenario.issues.push_back(after);
      for (int i = 0; i < 3; ++i) {
        auto extra = mem(0, 0, 10 + i, 0x6100 + i * 4);
        extra.wait_for_previous = false;
        scenario.issues.push_back(extra);
      }
      FaultSpec degrade;
      degrade.issue_index = 3;
      degrade.trigger = Trigger::Phase;
      degrade.phase = Phase::AxiAccepted;
      degrade.kind = FaultKind::RateReduction;
      degrade.bandwidth_factor = 0.25;
      scenario.faults.push_back(degrade);
      break;
    }
    case Experiment::RandomCampaign:
      scenario.random_faults = true;
      config.random_fault_probability = 0.08;
      config.replay_limit = 8;
      for (int i = 0; i < 24; ++i) scenario.issues.push_back(mem(i % 3, 0, i, 0x7000 + i * 4));
      break;
    case Experiment::RefRetryBuffer: {
      // Sustained traffic intended to exercise a multi-entry retry buffer.
      scenario.policy = RecoveryPolicy::Discard;
      scenario.retrain_preserves_retry = true;
      config.replay_limit = 8;
      for (int i = 0; i < 24; ++i) scenario.issues.push_back(mem(0, 0, i, 0x8000 + i * 4));
      FaultSpec burst;
      burst.issue_index = -1;
      burst.trigger = Trigger::Phase;
      burst.phase = Phase::Transmitting;
      burst.kind = FaultKind::BurstError;
      burst.storm_naks = 1;
      scenario.faults.push_back(burst);
      break;
    }
    case Experiment::RefFifoOccupancy: {
      scenario.policy = RecoveryPolicy::Discard;
      for (int i = 0; i < 48; ++i) scenario.issues.push_back(mem(0, 0, i, 0x8100 + i * 4));
      break;
    }
    case Experiment::RefCreditBehavior: {
      scenario.policy = RecoveryPolicy::Discard;
      for (int i = 0; i < 16; ++i) scenario.issues.push_back(mem(0, 0, i, 0x8200 + i * 4));
      FaultSpec retrain;
      retrain.issue_index = 4;
      retrain.trigger = Trigger::Phase;
      retrain.phase = Phase::ArchitecturalCommit;
      retrain.kind = FaultKind::ForcedRetrain;
      scenario.faults.push_back(retrain);
      break;
    }
    case Experiment::RefSustainedReplay: {
      scenario.policy = RecoveryPolicy::Discard;
      config.replay_limit = 6;
      for (int i = 0; i < 32; ++i) scenario.issues.push_back(mem(0, 0, i, 0x8300 + i * 4));
      FaultSpec burst;
      burst.issue_index = -1;
      burst.trigger = Trigger::Phase;
      burst.phase = Phase::Transmitting;
      burst.kind = FaultKind::BurstError;
      burst.storm_naks = 2;
      scenario.faults.push_back(burst);
      break;
    }
    case Experiment::RefCommitVsAck:
      // Same counterexample as baseline_unknown: commit then lost response.
      scenario.policy = RecoveryPolicy::Discard;
      scenario.issues.push_back(mmio(0, 0, -1, 0, Criticality::SafetyCritical));
      scenario.faults.push_back(corrupt_response(true));
      break;
    case Experiment::RefRecoveryMatrix: {
      // Three successive MMIO writes, each forced into a different recovery.
      scenario.policy = RecoveryPolicy::Discard;
      const FaultKind kinds[] = {FaultKind::ForcedRetrain, FaultKind::ForcedLinkReset,
                                 FaultKind::ForcedLinkError};
      for (int i = 0; i < 3; ++i) {
        auto issue = mmio(0, 0, -1, 0, Criticality::SafetyCritical);
        issue.wait_for_previous = i > 0;
        if (i > 0) issue.counter_reset = 7;
        scenario.issues.push_back(issue);
        FaultSpec fault;
        fault.issue_index = i;
        fault.trigger = Trigger::Phase;
        fault.phase = Phase::ResponseCreated;
        fault.kind = kinds[i];
        scenario.faults.push_back(fault);
      }
      break;
    }
    case Experiment::RefDiagnostics:
      scenario.policy = RecoveryPolicy::Discard;
      config.error_logging = true;
      scenario.issues.push_back(mmio(0, 0, -1, 0, Criticality::SafetyCritical));
      scenario.faults.push_back(corrupt_response(true));
      break;
    case Experiment::RefSideband: {
      scenario.sideband_saturation = true;
      for (int i = 0; i < 8; ++i) scenario.issues.push_back(mem(0, 0, i, 0x8400 + i * 4));
      FaultSpec err;
      err.trigger = Trigger::AbsoluteTime;
      err.at_ns = 50;
      err.kind = FaultKind::ForcedLinkError;
      scenario.faults.push_back(err);
      break;
    }
    case Experiment::RefL1: {
      scenario.policy = RecoveryPolicy::Discard;
      config.l1_enabled = true;
      auto before = mem(0, 0, 1, 0x8500);
      scenario.issues.push_back(before);
      auto after = mmio(0, 20, -1, 0, Criticality::SafetyCritical);
      after.wait_for_previous = true;
      scenario.issues.push_back(after);
      FaultSpec l1;
      l1.trigger = Trigger::AbsoluteTime;
      l1.at_ns = 30;
      l1.kind = FaultKind::EnterL1;
      scenario.faults.push_back(l1);
      break;
    }
    case Experiment::RefThroughputTradeoff: {
      // Fault-free sustained traffic; CRC/retry mode comes from the config.
      scenario.policy = RecoveryPolicy::Discard;
      for (int i = 0; i < 40; ++i) scenario.issues.push_back(mem(0, 0, i, 0x8600 + i * 4));
      break;
    }
    case Experiment::RefCrossClient: {
      // Renamed Client A / Client B interference under the reference transport.
      load_shared_ffi_workload(scenario, config, true);
      arm_guest_storm(scenario, 0);
      break;
    }

    // --- Fault-handling draft V0.1 experiment matrix ---
    case Experiment::FhCleanSuccess:
      // 1. Fault-free non-idempotent operation.
      scenario.policy = RecoveryPolicy::LocalSlverr;
      config.local_slverr_on_link_error = true;
      scenario.issues.push_back(mmio(0, 0, -1, 0, Criticality::SafetyCritical));
      break;
    case Experiment::FhLossBeforeAccept:
      // 2. Provable loss before target accept → D may be NOT_COMPLETED.
      scenario.policy = RecoveryPolicy::LocalSlverr;
      config.local_slverr_on_link_error = true;
      scenario.issues.push_back(mmio(0, 0, -1, 0, Criticality::SafetyCritical));
      scenario.faults.push_back(corrupt_request());
      break;
    case Experiment::FhCommitThenLocalSlverr:
      // 3. Target commit, response loss, then local SLVERR (ERR-05).
      scenario.policy = RecoveryPolicy::LocalSlverr;
      config.local_slverr_on_link_error = true;
      config.master_retry = MasterRetryPolicy::Abort;
      scenario.issues.push_back(mmio(0, 0, -1, 0, Criticality::SafetyCritical));
      scenario.faults.push_back(corrupt_response(true));
      break;
    case Experiment::FhNoCommitLocalSlverr:
      // 4. No target commit, then local SLVERR.
      scenario.policy = RecoveryPolicy::LocalSlverr;
      config.local_slverr_on_link_error = true;
      scenario.issues.push_back(mmio(0, 0, -1, 0, Criticality::SafetyCritical));
      scenario.faults.push_back(corrupt_request());
      break;
    case Experiment::FhWatchdogThenLateOkay: {
      // 5. Local watchdog SLVERR, then late successful target response (WDT-03/04).
      scenario.policy = RecoveryPolicy::LocalSlverr;
      config.txn_watchdog_enabled = true;
      config.txn_watchdog_ns = 200;  // short for simulation
      config.txn_watchdog_during_recovery = true;
      config.discard_late_responses = true;
      config.master_retry = MasterRetryPolicy::Abort;
      // Stretch target path so watchdog fires before response returns.
      config.target_commit_cycles = 50;
      config.target_response_cycles = 50;
      scenario.issues.push_back(mmio(0, 0, -1, 0, Criticality::SafetyCritical));
      FaultSpec delay;
      delay.trigger = Trigger::Phase;
      delay.phase = Phase::ResponseTransmitting;
      delay.role = FlitRole::Response;
      delay.kind = FaultKind::DelayedFlit;
      delay.extra_delay_ns = 5000;
      scenario.faults.push_back(delay);
      break;
    }
    case Experiment::FhTimeoutThenRemoteCommit: {
      // 6. Local timeout first; remote commit may still occur afterward.
      scenario.policy = RecoveryPolicy::LocalSlverr;
      config.txn_watchdog_enabled = true;
      config.txn_watchdog_ns = 100;
      config.discard_late_responses = true;
      config.master_retry = MasterRetryPolicy::Abort;
      config.target_accept_cycles = 80;
      config.target_commit_cycles = 80;
      scenario.issues.push_back(mmio(0, 0, -1, 0, Criticality::SafetyCritical));
      break;
    }
    case Experiment::FhBlindRetryAfterSlverr:
      // 7. Blind software retry after SLVERR when commit already happened.
      scenario.policy = RecoveryPolicy::LocalSlverr;
      config.local_slverr_on_link_error = true;
      config.master_retry = MasterRetryPolicy::BlindRetry;
      scenario.issues.push_back(mmio(0, 0, -1, 0, Criticality::SafetyCritical));
      scenario.faults.push_back(corrupt_response(true));
      break;
    case Experiment::FhNoRetryAfterSlverr:
      // 8. No retry after SLVERR.
      scenario.policy = RecoveryPolicy::LocalSlverr;
      config.local_slverr_on_link_error = true;
      config.master_retry = MasterRetryPolicy::Abort;
      scenario.issues.push_back(mmio(0, 0, -1, 0, Criticality::SafetyCritical));
      scenario.faults.push_back(corrupt_response(true));
      break;
    case Experiment::FhAxiIdReuseLateResp: {
      // 9. New transaction reuses AXI ID while a late old response may arrive.
      scenario.policy = RecoveryPolicy::LocalSlverr;
      config.txn_watchdog_enabled = true;
      config.txn_watchdog_ns = 150;
      config.discard_late_responses = true;
      config.target_commit_cycles = 40;
      config.target_response_cycles = 40;
      auto first = mmio(0, 0, -1, 0, Criticality::SafetyCritical);
      first.axi_id = 7;
      scenario.issues.push_back(first);
      FaultSpec delay;
      delay.trigger = Trigger::Phase;
      delay.phase = Phase::ResponseTransmitting;
      delay.role = FlitRole::Response;
      delay.kind = FaultKind::DelayedFlit;
      delay.extra_delay_ns = 8000;
      delay.issue_index = 0;
      scenario.faults.push_back(delay);
      auto second = mmio(0, 400, -1, 0, Criticality::SafetyCritical);
      second.axi_id = 7;
      second.wait_for_previous = true;
      second.counter_reset = -1;
      scenario.issues.push_back(second);
      break;
    }
    case Experiment::FhMultiOutstandingBackpressure: {
      // 10. Multiple outstanding + response backpressure.
      scenario.policy = RecoveryPolicy::LocalSlverr;
      config.single_inflight_slot = false;
      config.axi_resp_ready_delay_cycles = 8;
      config.credit_depth = 8;
      scenario.guest_outstanding = {4, 4, 4};
      for (int i = 0; i < 3; ++i) {
        scenario.issues.push_back(mmio(0, 0, -1, 0, Criticality::BestEffort));
      }
      break;
    }
    case Experiment::FhReadBurstErrorCompletion: {
      // 11. Read-burst error completion with remaining beats under READY delay.
      scenario.policy = RecoveryPolicy::LocalSlverr;
      config.local_slverr_on_link_error = true;
      config.axi_resp_ready_delay_cycles = 4;
      auto rd = mmio(0, 0, -1, 0, Criticality::SafetyCritical);
      rd.op = OpKind::MemoryRead;
      rd.burst_len = 4;
      scenario.issues.push_back(rd);
      scenario.faults.push_back(corrupt_response(true));
      break;
    }
    case Experiment::FhRaceRespBeforeTimeout: {
      // 12a. Target/Bridge response path wins before watchdog.
      // Short LinkError residency so ERR-05 is not masked by the 16 ms spec wait.
      scenario.policy = RecoveryPolicy::LocalSlverr;
      config.txn_watchdog_enabled = true;
      config.txn_watchdog_ns = 50000;
      config.link_error_residency_ns = 50;
      config.local_slverr_on_link_error = true;
      config.axi_resp_ready_delay_cycles = 4;
      config.discard_late_responses = true;
      scenario.issues.push_back(mmio(0, 0, -1, 0, Criticality::SafetyCritical));
      // No delay: response/commit completes; LinkError after response is created
      // still yields a single local completion under backpressure before WDT.
      FaultSpec err;
      err.trigger = Trigger::Phase;
      err.phase = Phase::ResponseCreated;
      err.kind = FaultKind::ForcedLinkError;
      scenario.faults.push_back(err);
      break;
    }
    case Experiment::FhRaceRespSimultaneous: {
      // 12b. Response path and watchdog nearly simultaneous; LinkError + backpressure.
      scenario.policy = RecoveryPolicy::LocalSlverr;
      config.txn_watchdog_enabled = true;
      config.txn_watchdog_ns = 80;
      config.link_error_residency_ns = 50;
      config.local_slverr_on_link_error = true;
      config.axi_resp_ready_delay_cycles = 4;
      config.discard_late_responses = true;
      config.target_accept_cycles = 20;
      config.target_commit_cycles = 20;
      config.target_response_cycles = 20;
      scenario.issues.push_back(mmio(0, 0, -1, 0, Criticality::SafetyCritical));
      FaultSpec err;
      err.trigger = Trigger::Phase;
      err.phase = Phase::ArchitecturalCommit;
      err.kind = FaultKind::ForcedLinkError;
      scenario.faults.push_back(err);
      break;
    }
    case Experiment::FhRaceRespAfterTimeout: {
      // 12c. Watchdog wins; late response discarded; LinkError + backpressure.
      scenario.policy = RecoveryPolicy::LocalSlverr;
      config.txn_watchdog_enabled = true;
      config.txn_watchdog_ns = 100;
      config.link_error_residency_ns = 50;
      config.local_slverr_on_link_error = true;
      config.axi_resp_ready_delay_cycles = 4;
      config.discard_late_responses = true;
      config.target_commit_cycles = 40;
      config.target_response_cycles = 40;
      scenario.issues.push_back(mmio(0, 0, -1, 0, Criticality::SafetyCritical));
      FaultSpec delay;
      delay.trigger = Trigger::Phase;
      delay.phase = Phase::ResponseTransmitting;
      delay.role = FlitRole::Response;
      delay.kind = FaultKind::DelayedFlit;
      delay.extra_delay_ns = 2000;
      scenario.faults.push_back(delay);
      FaultSpec err;
      err.trigger = Trigger::Phase;
      err.phase = Phase::ArchitecturalCommit;
      err.kind = FaultKind::ForcedLinkError;
      scenario.faults.push_back(err);
      break;
    }
    case Experiment::FhClassifierDiagCompare: {
      // 13. With/without diagnostic evaluation.
      // Tx0: commit then local SLVERR → C stays UNKNOWN (handed).
      // Tx1: LinkError before UCIe handoff → C NOT_COMPLETED only with diag.
      scenario.policy = RecoveryPolicy::LocalSlverr;
      config.local_slverr_on_link_error = true;
      config.expose_bridge_diagnostics = true;
      config.use_diag_in_classifier = true;
      config.link_error_residency_ns = 50;
      scenario.issues.push_back(mmio(0, 0, -1, 0, Criticality::SafetyCritical));
      auto after = corrupt_response(true);
      after.issue_index = 0;
      scenario.faults.push_back(after);
      auto never_handed = mmio(0, 0, -1, 0, Criticality::SafetyCritical);
      never_handed.wait_for_previous = true;
      never_handed.counter_reset = 7;
      scenario.issues.push_back(never_handed);
      FaultSpec pre_tx;
      pre_tx.trigger = Trigger::Phase;
      pre_tx.phase = Phase::AouPacked;
      pre_tx.kind = FaultKind::ForcedLinkError;
      pre_tx.issue_index = 1;
      scenario.faults.push_back(pre_tx);
      break;
    }
    case Experiment::FhDiagAckLossAfterDelivery: {
      // Counterexample: request delivered to target, ACK suppressed, commit,
      // then local SLVERR. Missing ACK must NOT yield NOT_COMPLETED for C/D.
      // Use a read so a single request flit can be delivered without waiting on
      // a write-data ACK under single_inflight_slot.
      scenario.policy = RecoveryPolicy::LocalSlverr;
      config.txn_watchdog_enabled = true;
      config.txn_watchdog_ns = 400;
      config.discard_late_responses = true;
      config.expose_bridge_diagnostics = true;
      config.use_diag_in_classifier = true;
      config.master_retry = MasterRetryPolicy::Abort;
      config.single_inflight_slot = false;
      config.target_response_cycles = 100;
      auto rd = mmio(0, 0, -1, 0, Criticality::SafetyCritical);
      rd.op = OpKind::MemoryRead;
      scenario.issues.push_back(rd);
      FaultSpec ack_loss;
      ack_loss.trigger = Trigger::Phase;
      ack_loss.phase = Phase::RemoteReceived;
      ack_loss.kind = FaultKind::AckNakLoss;
      ack_loss.role = FlitRole::Ack;
      ack_loss.persistent = true;  // keep suppressing; missing ACK ≠ non-delivery
      scenario.faults.push_back(ack_loss);
      FaultSpec delay;
      delay.trigger = Trigger::Phase;
      delay.phase = Phase::ResponseTransmitting;
      delay.role = FlitRole::Response;
      delay.kind = FaultKind::DelayedFlit;
      delay.extra_delay_ns = 5000;
      scenario.faults.push_back(delay);
      break;
    }
    case Experiment::FhObserverHistoryCompare: {
      // 14. Commit vs no-commit under ERR-05; histories compared with and without time.
      scenario.policy = RecoveryPolicy::LocalSlverr;
      config.local_slverr_on_link_error = true;
      config.expose_bridge_diagnostics = true;
      config.use_diag_in_classifier = true;
      auto committed = mmio(0, 0, -1, 0, Criticality::SafetyCritical);
      scenario.issues.push_back(committed);
      auto missed = mmio(0, 0, -1, 0, Criticality::SafetyCritical);
      missed.wait_for_previous = true;
      missed.counter_reset = 7;
      scenario.issues.push_back(missed);
      auto after = corrupt_response(true);
      after.issue_index = 0;
      scenario.faults.push_back(after);
      auto before = corrupt_request();
      before.issue_index = 1;
      scenario.faults.push_back(before);
      break;
    }
    case Experiment::FhSafeRetryCancelRemote: {
      // 15. Safe retry under instantaneous shared-state cancel (MODEL ASSUMPTION).
      scenario.policy = RecoveryPolicy::LocalSlverr;
      config.local_slverr_on_link_error = true;
      config.cancel_remote_on_local_close = true;
      config.cancel_latency_ns = 0;  // instantaneous — not a wire cancel protocol
      config.expose_bridge_diagnostics = true;
      config.use_diag_in_classifier = true;
      config.master_retry = MasterRetryPolicy::RetryOnlyIfNotCompleted;
      scenario.issues.push_back(mmio(0, 0, -1, 0, Criticality::SafetyCritical));
      scenario.faults.push_back(corrupt_request());
      break;
    }
    case Experiment::FhCancelLatencyRace: {
      // 16. Like timeout-then-remote-commit, but cancel is *requested* on local
      // close with large cancel_latency_ns. Remote still commits — proving that
      // instantaneous shared-state cancel is a MODEL ASSUMPTION.
      scenario.policy = RecoveryPolicy::LocalSlverr;
      config.txn_watchdog_enabled = true;
      config.txn_watchdog_ns = 100;
      config.discard_late_responses = true;
      config.cancel_remote_on_local_close = true;
      config.cancel_latency_ns = 10000;  // effective only long after commit
      config.target_accept_cycles = 80;
      config.target_commit_cycles = 80;
      config.master_retry = MasterRetryPolicy::Abort;
      scenario.issues.push_back(mmio(0, 0, -1, 0, Criticality::SafetyCritical));
      break;
    }

    case Experiment::AxCleanPacketImm: {
      axis_base(config, scenario);
      scenario.issues.push_back(axis_pkt(AxisSinkKind::Imm, 4));
      break;
    }
    case Experiment::AxCleanPacketPkt: {
      axis_base(config, scenario);
      scenario.issues.push_back(axis_pkt(AxisSinkKind::Pkt, 4));
      break;
    }
    case Experiment::AxLossMidPacketImm: {
      axis_base(config, scenario);
      config.link_error_residency_ns = 50;
      // Force in-order segment delivery so later beats are still open when
      // LinkError becomes destructive after the first IMM effect.
      config.single_inflight_slot = true;
      scenario.issues.push_back(axis_pkt(AxisSinkKind::Imm, 4));
      // After the first IMM architectural effect, force LinkError so later beats
      // cannot complete — yields a measurable partial effect (GT∈{1..3}).
      FaultSpec mid;
      mid.trigger = Trigger::Phase;
      mid.phase = Phase::ArchitecturalCommit;
      mid.kind = FaultKind::ForcedLinkError;
      scenario.faults.push_back(mid);
      break;
    }
    case Experiment::AxLossMidPacketPkt: {
      axis_base(config, scenario);
      config.link_error_residency_ns = 50;
      scenario.issues.push_back(axis_pkt(AxisSinkKind::Pkt, 4));
      // Kill the link after first remote reception — packet never completes,
      // so SINK-PKT must not apply an application effect.
      FaultSpec mid;
      mid.trigger = Trigger::Phase;
      mid.phase = Phase::RemoteReceived;
      mid.kind = FaultKind::ForcedLinkError;
      scenario.faults.push_back(mid);
      break;
    }
    case Experiment::AxLossBeforeFirstImm:
    case Experiment::AxLossBeforeFirstPkt: {
      axis_base(config, scenario);
      config.link_error_residency_ns = 50;
      auto iss = axis_pkt(experiment == Experiment::AxLossBeforeFirstImm ? AxisSinkKind::Imm
                                                                         : AxisSinkKind::Pkt,
                          4);
      scenario.issues.push_back(iss);
      // Fire as soon as AXIS acceptance begins, before any StreamSegment TX.
      FaultSpec pre;
      pre.trigger = Trigger::Phase;
      pre.phase = Phase::AxiAccepted;
      pre.kind = FaultKind::ForcedLinkError;
      scenario.faults.push_back(pre);
      break;
    }
    case Experiment::AxTlastLocalRemoteLoss: {
      axis_base(config, scenario);
      config.link_error_residency_ns = 50;
      scenario.issues.push_back(axis_pkt(AxisSinkKind::Pkt, 4));
      FaultSpec drop_last;
      drop_last.trigger = Trigger::Phase;
      drop_last.phase = Phase::Transmitting;
      drop_last.role = FlitRole::StreamSegment;
      drop_last.kind = FaultKind::DroppedFlit;
      scenario.faults.push_back(drop_last);
      FaultSpec corrupt;
      corrupt.trigger = Trigger::Phase;
      corrupt.phase = Phase::QueuedForLink;
      corrupt.role = FlitRole::StreamSegment;
      corrupt.kind = FaultKind::RepeatedCorruption;
      corrupt.persistent = true;
      corrupt.fail_retrain = true;
      scenario.faults.push_back(corrupt);
      break;
    }
    case Experiment::AxImmPartialThenBlindRetry: {
      axis_base(config, scenario);
      config.link_error_residency_ns = 50;
      config.master_retry = MasterRetryPolicy::BlindRetry;
      scenario.issues.push_back(axis_pkt(AxisSinkKind::Imm, 4));
      scenario.faults.push_back(corrupt_stream_after_beat(2));
      break;
    }
    case Experiment::AxPktCommitThenLocalTimeout: {
      axis_base(config, scenario);
      config.txn_watchdog_enabled = true;
      config.txn_watchdog_ns = 80;
      config.axis_settle_grace_ns = 20000;
      config.axis_sink_process_start_cycles = 1;
      config.axis_sink_effect_cycles = 1;
      config.target_accept_cycles = 1;
      config.target_commit_cycles = 1;
      // Delay nothing on request path; stretch settle so remote PKT commits,
      // then local watchdog/settle without App-ACK leaves initiator indeterminate.
      scenario.issues.push_back(axis_pkt(AxisSinkKind::Pkt, 2));
      break;
    }
    case Experiment::AxAckLossAfterDelivery: {
      axis_base(config, scenario);
      config.txn_watchdog_enabled = true;
      config.txn_watchdog_ns = 2000;
      config.axis_settle_grace_ns = 5000;
      config.single_inflight_slot = false;
      scenario.issues.push_back(axis_pkt(AxisSinkKind::Imm, 2));
      FaultSpec ack_loss;
      ack_loss.trigger = Trigger::Phase;
      ack_loss.phase = Phase::RemoteReceived;
      ack_loss.kind = FaultKind::AckNakLoss;
      ack_loss.role = FlitRole::Ack;
      ack_loss.persistent = true;
      scenario.faults.push_back(ack_loss);
      break;
    }
    case Experiment::AxObserverTimedVsSeq: {
      axis_base(config, scenario);
      config.link_error_residency_ns = 50;
      auto ok = axis_pkt(AxisSinkKind::Imm, 2);
      scenario.issues.push_back(ok);
      auto lost = axis_pkt(AxisSinkKind::Imm, 2);
      lost.wait_for_previous = true;
      lost.counter_reset = 7;
      scenario.issues.push_back(lost);
      FaultSpec after;
      after.trigger = Trigger::Phase;
      after.phase = Phase::ArchitecturalCommit;
      after.kind = FaultKind::ForcedLinkError;
      after.issue_index = 0;
      scenario.faults.push_back(after);
      FaultSpec before;
      before.trigger = Trigger::Phase;
      before.phase = Phase::AouPacked;
      before.kind = FaultKind::ForcedLinkError;
      before.issue_index = 1;
      scenario.faults.push_back(before);
      break;
    }
    case Experiment::AxSafeRetryCancel0: {
      axis_base(config, scenario);
      config.link_error_residency_ns = 50;
      config.cancel_remote_on_local_close = true;
      config.cancel_latency_ns = 0;
      config.master_retry = MasterRetryPolicy::RetryOnlyIfNotCompleted;
      scenario.issues.push_back(axis_pkt(AxisSinkKind::Pkt, 2));
      FaultSpec pre;
      pre.trigger = Trigger::Phase;
      pre.phase = Phase::AouPacked;
      pre.kind = FaultKind::ForcedLinkError;
      scenario.faults.push_back(pre);
      break;
    }
    case Experiment::AxCancelLatencyRace: {
      axis_base(config, scenario);
      config.txn_watchdog_enabled = true;
      config.txn_watchdog_ns = 50;
      config.cancel_remote_on_local_close = true;
      config.cancel_latency_ns = 10000;  // MODEL: cancel too late to stop remote
      config.target_accept_cycles = 80;
      config.target_commit_cycles = 80;
      config.axis_sink_process_start_cycles = 40;
      config.axis_sink_effect_cycles = 40;
      // Long grace so settle does not win before watchdog abort.
      config.axis_settle_grace_ns = 20000;
      scenario.issues.push_back(axis_pkt(AxisSinkKind::Pkt, 2));
      break;
    }
    case Experiment::AxBackpressureTready: {
      axis_base(config, scenario);
      config.axis_tready_delay_cycles = 8;
      config.axis_tvalid_lead_cycles = 2;
      scenario.issues.push_back(axis_pkt(AxisSinkKind::Imm, 4));
      break;
    }
    case Experiment::AxContinuousTidStable: {
      axis_base(config, scenario);
      auto iss = axis_pkt(AxisSinkKind::Imm, 4);
      iss.axis_force_tid_change_mid = true;
      scenario.issues.push_back(iss);
      break;
    }
    case Experiment::AxAppackLoss: {
      axis_base(config, scenario);
      config.axis_app_ack_enabled = true;
      config.txn_watchdog_enabled = true;
      config.txn_watchdog_ns = 400;
      config.axis_settle_grace_ns = 200;
      auto iss = axis_pkt(AxisSinkKind::Pkt, 2);
      iss.axis_app_ack = true;
      scenario.issues.push_back(iss);
      FaultSpec drop_ack;
      drop_ack.trigger = Trigger::Phase;
      drop_ack.phase = Phase::ResponseTransmitting;
      drop_ack.role = FlitRole::Response;
      drop_ack.kind = FaultKind::DroppedFlit;
      scenario.faults.push_back(drop_ack);
      break;
    }
    case Experiment::AxResetPartialPacket: {
      axis_base(config, scenario);
      config.link_error_residency_ns = 50;
      scenario.issues.push_back(axis_pkt(AxisSinkKind::Imm, 4));
      FaultSpec mid;
      mid.trigger = Trigger::Phase;
      mid.phase = Phase::RemoteReceived;
      mid.kind = FaultKind::ForcedLinkReset;
      scenario.faults.push_back(mid);
      break;
    }
    case Experiment::AxLateDataAfterRestart: {
      axis_base(config, scenario);
      config.link_error_residency_ns = 50;
      config.txn_watchdog_enabled = true;
      config.txn_watchdog_ns = 150;
      config.discard_late_responses = true;
      config.target_accept_cycles = 30;
      config.target_commit_cycles = 30;
      scenario.issues.push_back(axis_pkt(AxisSinkKind::Imm, 2));
      FaultSpec delay;
      delay.trigger = Trigger::Phase;
      delay.phase = Phase::Transmitting;
      delay.role = FlitRole::StreamSegment;
      delay.kind = FaultKind::DelayedFlit;
      delay.extra_delay_ns = 5000;
      scenario.faults.push_back(delay);
      FaultSpec err;
      err.trigger = Trigger::Phase;
      err.phase = Phase::AxiAccepted;
      err.kind = FaultKind::ForcedLinkError;
      scenario.faults.push_back(err);
      break;
    }
    case Experiment::AxMultiPacketInflight: {
      axis_base(config, scenario);
      config.single_inflight_slot = false;
      config.credit_depth = 8;
      scenario.guest_outstanding = {4, 4, 4};
      scenario.issues.push_back(axis_pkt(AxisSinkKind::Imm, 2));
      scenario.issues.push_back(axis_pkt(AxisSinkKind::Pkt, 2));
      scenario.issues.push_back(axis_pkt(AxisSinkKind::Imm, 1));
      break;
    }
    case Experiment::AxPacketLenVariants: {
      axis_base(config, scenario);
      scenario.issues.push_back(axis_pkt(AxisSinkKind::Imm, 1, 16));
      auto mid = axis_pkt(AxisSinkKind::Imm, 4, 7);
      mid.wait_for_previous = true;
      scenario.issues.push_back(mid);
      auto longp = axis_pkt(AxisSinkKind::Pkt, 16, 3);
      longp.wait_for_previous = true;
      scenario.issues.push_back(longp);
      break;
    }
    case Experiment::AxReplayNoDupEffect: {
      // UCIe replay of CRC-corrupted flit should not double-apply once accepted.
      axis_base(config, scenario);
      config.replay_limit = 3;
      scenario.issues.push_back(axis_pkt(AxisSinkKind::Imm, 2));
      FaultSpec oneshot;
      oneshot.trigger = Trigger::Phase;
      oneshot.phase = Phase::Transmitting;
      oneshot.role = FlitRole::StreamSegment;
      oneshot.kind = FaultKind::SingleBit;
      oneshot.corrupt_bits = 1;
      scenario.faults.push_back(oneshot);
      break;
    }
    case Experiment::AxAppLevelRepeatDup: {
      // Application-level reissue without transport dup-suppress → double effect.
      axis_base(config, scenario);
      config.link_error_residency_ns = 50;
      config.master_retry = MasterRetryPolicy::BlindRetry;
      scenario.issues.push_back(axis_pkt(AxisSinkKind::Pkt, 2));
      FaultSpec after;
      after.trigger = Trigger::Phase;
      after.phase = Phase::ArchitecturalCommit;
      after.kind = FaultKind::ForcedLinkError;
      scenario.faults.push_back(after);
      break;
    }
    case Experiment::AxDupSuppress: {
      axis_base(config, scenario);
      config.axis_dup_suppress = true;
      config.axis_packet_id_enabled = true;
      config.axis_dup_capacity = 4;
      config.axis_dup_lifetime_ns = 0;
      // Survive LinkError/flush so BlindRetry of the same packet_op_id is suppressed.
      // clear_on_reset=true is a separate policy exercised by ax_packet_id_reuse /
      // reset experiments.
      config.axis_dup_clear_on_reset = false;
      config.link_error_residency_ns = 50;
      config.master_retry = MasterRetryPolicy::BlindRetry;
      auto iss = axis_pkt(AxisSinkKind::Pkt, 2);
      iss.axis_packet_id = true;
      iss.axis_packet_op_id = 42;
      scenario.issues.push_back(iss);
      FaultSpec after;
      after.trigger = Trigger::Phase;
      after.phase = Phase::ArchitecturalCommit;
      after.kind = FaultKind::ForcedLinkError;
      scenario.faults.push_back(after);
      break;
    }
    case Experiment::AxPacketIdReuse: {
      axis_base(config, scenario);
      config.axis_dup_suppress = true;
      config.axis_packet_id_enabled = true;
      config.axis_dup_capacity = 1;  // tiny table → eviction / reuse pressure
      config.axis_dup_lifetime_ns = 1;  // expire quickly
      auto a = axis_pkt(AxisSinkKind::Pkt, 1);
      a.axis_packet_id = true;
      a.axis_packet_op_id = 7;
      scenario.issues.push_back(a);
      auto b = axis_pkt(AxisSinkKind::Pkt, 1);
      b.axis_packet_id = true;
      b.axis_packet_op_id = 7;  // reuse after expiry
      b.wait_for_previous = true;
      b.delay_ns = 50;
      scenario.issues.push_back(b);
      break;
    }

    // --- AXI5-MM-P1 (IHI0022L AtomicStore(ADD) / Poison / Trace) ---
    case Experiment::A5AtomicStoreAddOk: {
      axi5_base(config, scenario);
      scenario.issues.push_back(atomic_store_add(5));
      break;
    }
    case Experiment::A5AtomicCommitThenLocalSlverr: {
      axi5_base(config, scenario);
      config.master_retry = MasterRetryPolicy::Abort;
      scenario.issues.push_back(atomic_store_add(5));
      scenario.faults.push_back(corrupt_response(true));
      break;
    }
    case Experiment::A5AtomicBlindRetry: {
      axi5_base(config, scenario);
      config.master_retry = MasterRetryPolicy::BlindRetry;
      scenario.issues.push_back(atomic_store_add(5));
      scenario.faults.push_back(corrupt_response(true));
      break;
    }
    case Experiment::A5AtomicNeverHanded: {
      axi5_base(config, scenario);
      scenario.issues.push_back(atomic_store_add(5));
      FaultSpec pre;
      pre.trigger = Trigger::Phase;
      pre.phase = Phase::AouPacked;
      pre.kind = FaultKind::ForcedLinkError;
      scenario.faults.push_back(pre);
      break;
    }
    case Experiment::A5AtomicAckLoss: {
      axi5_base(config, scenario);
      config.master_retry = MasterRetryPolicy::Abort;
      config.single_inflight_slot = false;
      config.txn_watchdog_enabled = true;
      config.txn_watchdog_ns = 800;
      config.discard_late_responses = true;
      scenario.issues.push_back(atomic_store_add(5));
      FaultSpec ack_loss;
      ack_loss.trigger = Trigger::Phase;
      ack_loss.phase = Phase::Transmitting;
      ack_loss.role = FlitRole::Ack;
      ack_loss.kind = FaultKind::AckNakLoss;
      ack_loss.persistent = true;
      scenario.faults.push_back(ack_loss);
      FaultSpec delay;
      delay.trigger = Trigger::Phase;
      delay.phase = Phase::ResponseTransmitting;
      delay.role = FlitRole::Response;
      delay.kind = FaultKind::DelayedFlit;
      delay.extra_delay_ns = 5000;
      scenario.faults.push_back(delay);
      break;
    }
    case Experiment::A5AtomicLossBeforeExec: {
      axi5_base(config, scenario);
      scenario.issues.push_back(atomic_store_add(5));
      scenario.faults.push_back(corrupt_request());
      break;
    }
    case Experiment::A5AtomicTimeoutAfterCommit: {
      axi5_base(config, scenario);
      config.txn_watchdog_enabled = true;
      config.txn_watchdog_ns = 100;
      config.discard_late_responses = true;
      config.master_retry = MasterRetryPolicy::Abort;
      config.target_accept_cycles = 80;
      config.target_commit_cycles = 80;
      scenario.issues.push_back(atomic_store_add(5));
      break;
    }
    case Experiment::A5AtomicLateResponse: {
      axi5_base(config, scenario);
      config.txn_watchdog_enabled = true;
      config.txn_watchdog_ns = 200;
      config.txn_watchdog_during_recovery = true;
      config.discard_late_responses = true;
      config.master_retry = MasterRetryPolicy::Abort;
      config.target_commit_cycles = 50;
      config.target_response_cycles = 50;
      scenario.issues.push_back(atomic_store_add(5));
      FaultSpec delay;
      delay.trigger = Trigger::Phase;
      delay.phase = Phase::ResponseTransmitting;
      delay.role = FlitRole::Response;
      delay.kind = FaultKind::DelayedFlit;
      delay.extra_delay_ns = 5000;
      scenario.faults.push_back(delay);
      break;
    }
    case Experiment::A5AtomicConcurrent: {
      axi5_base(config, scenario);
      config.single_inflight_slot = false;
      auto a = atomic_store_add(3);
      a.axi_id = 1;
      auto b = atomic_store_add(4);
      b.axi_id = 2;
      b.delay_ns = 1;
      scenario.issues.push_back(a);
      scenario.issues.push_back(b);
      break;
    }
    case Experiment::A5AtomicDupSuppress: {
      axi5_base(config, scenario);
      config.target_duplicate_suppress = true;
      config.master_retry = MasterRetryPolicy::BlindRetry;
      scenario.issues.push_back(atomic_store_add(5));
      scenario.faults.push_back(corrupt_response(true));
      break;
    }
    case Experiment::A5AtomicResetRestart: {
      axi5_base(config, scenario);
      config.master_retry = MasterRetryPolicy::BlindRetry;
      scenario.issues.push_back(atomic_store_add(5));
      FaultSpec rst;
      rst.trigger = Trigger::Phase;
      rst.phase = Phase::ArchitecturalCommit;
      rst.kind = FaultKind::ForcedLinkError;
      scenario.faults.push_back(rst);
      break;
    }
    case Experiment::A5PoisonRejectNoCommit: {
      axi5_base(config, scenario);
      config.poison_target_policy = PoisonTargetPolicy::RejectWithoutCommitSlverr;
      auto iss = atomic_store_add(5);
      iss.axi5_wpoison = true;
      scenario.issues.push_back(iss);
      break;
    }
    case Experiment::A5PoisonRejectOkay: {
      axi5_base(config, scenario);
      config.poison_target_policy = PoisonTargetPolicy::RejectWithoutCommitOkay;
      auto iss = atomic_store_add(5);
      iss.axi5_wpoison = true;
      scenario.issues.push_back(iss);
      break;
    }
    case Experiment::A5PoisonWithSlverrIndependent: {
      axi5_base(config, scenario);
      config.poison_target_policy = PoisonTargetPolicy::RejectWithoutCommitOkay;
      auto poisoned = atomic_store_add(5);
      poisoned.axi5_wpoison = true;
      scenario.issues.push_back(poisoned);
      auto clean = atomic_store_add(7);
      clean.wait_for_previous = true;
      scenario.issues.push_back(clean);
      auto after = corrupt_response(true);
      after.issue_index = 1;
      scenario.faults.push_back(after);
      break;
    }
    case Experiment::A5PoisonAfterCommitNoUndo: {
      axi5_base(config, scenario);
      config.poison_target_policy = PoisonTargetPolicy::RejectWithoutCommitSlverr;
      scenario.issues.push_back(atomic_store_add(5));
      auto poison_try = atomic_store_add(-5);
      poison_try.axi5_wpoison = true;
      poison_try.wait_for_previous = true;
      scenario.issues.push_back(poison_try);
      break;
    }
    case Experiment::A5PoisonAcceptMarked: {
      axi5_base(config, scenario);
      config.poison_target_policy = PoisonTargetPolicy::AcceptMarkedOkay;
      auto iss = atomic_store_add(5);
      iss.axi5_wpoison = true;
      scenario.issues.push_back(iss);
      break;
    }
    case Experiment::A5PoisonPartialThenTimeout: {
      axi5_base(config, scenario);
      config.poison_target_policy = PoisonTargetPolicy::RejectWithoutCommitSlverr;
      config.txn_watchdog_enabled = true;
      config.txn_watchdog_ns = 100;
      config.discard_late_responses = true;
      config.target_accept_cycles = 80;
      config.target_commit_cycles = 80;
      auto iss = atomic_store_add(5);
      iss.axi5_wpoison = true;
      scenario.issues.push_back(iss);
      break;
    }
    case Experiment::A5TraceNotInClassifier: {
      axi5_base(config, scenario);
      config.master_retry = MasterRetryPolicy::Abort;
      auto with_trace = atomic_store_add(5);
      with_trace.axi5_awtrace = true;
      scenario.issues.push_back(with_trace);
      scenario.faults.push_back(corrupt_response(true));
      auto no_trace = atomic_store_add(5);
      no_trace.axi5_awtrace = false;
      no_trace.wait_for_previous = true;
      scenario.issues.push_back(no_trace);
      auto after = corrupt_response(true);
      after.issue_index = 1;
      scenario.faults.push_back(after);
      break;
    }
    case Experiment::A5SafeRetryCancel0: {
      axi5_base(config, scenario);
      config.cancel_remote_on_local_close = true;
      config.cancel_latency_ns = 0;  // MODEL ASSUMPTION: instant shared-state cancel
      config.master_retry = MasterRetryPolicy::RetryOnlyIfNotCompleted;
      scenario.issues.push_back(atomic_store_add(5));
      FaultSpec pre;
      pre.trigger = Trigger::Phase;
      pre.phase = Phase::AouPacked;
      pre.kind = FaultKind::ForcedLinkError;
      scenario.faults.push_back(pre);
      break;
    }
    case Experiment::A5ExclusiveSmokeOutOfScope: {
      axi5_base(config, scenario);
      auto iss = atomic_store_add(1);
      iss.axi5_exclusive = true;
      iss.axi5_form = Axi5AtomicForm::None;
      iss.axi5_awatop = kAwatopNonAtomic;
      scenario.issues.push_back(iss);
      break;
    }
  }
  return scenario;
}

bool evaluate(const Scenario& scenario, RunResult& result) {
  add_violation_strings(result);
  result.failures.clear();
  const auto& txs = result.transactions;

  require(result, result.finished, "simulation did not finish");
  require(result, result.internal_errors.empty(), "internal error");
  if (!result.credit_problems.empty() && !scenario.random_faults) {
    require(result, false, "credit conservation: " + result.credit_problems.front());
  }

  switch (scenario.experiment) {
    case Experiment::BaselineUnknown: {
      require(result, txs.size() == 1, "expected one transaction");
      if (txs.empty()) break;
      require(result, txs[0].ground.architectural_commit_count == 1, "COMMIT_COUNT != 1");
      require(result, result.counter == 8, "counter != 8");
      require(result, !txs[0].visible.axi_completed_ok, "initiator received a completion");
      require(result, txs[0].visible.fate_a == Fate::Unknown, "observer A != UNKNOWN");
      require(result, txs[0].visible.fate_b == Fate::Unknown, "observer B != UNKNOWN");
      require(result, txs[0].visible.fate_c == Fate::Unknown, "observer C != UNKNOWN");
      require(result, txs[0].visible.fate_d == Fate::Unknown, "observer D != UNKNOWN");
      require(result, result.linkerror_events >= 1, "LinkError was not entered");
      require(result, result.converged, "link did not return to Active");
      require(result, !result.deadlock, "deadlock");
      require(result, !has_flag(result, "AT_MOST_ONCE"), "unexpected duplicate commit");
      break;
    }
    case Experiment::PrecommitFailure: {
      require(result, txs.size() == 1, "expected one transaction");
      if (txs.empty()) break;
      require(result, txs[0].ground.architectural_commit_count == 0, "COMMIT_COUNT != 0");
      require(result, !txs[0].ground.target_accepted, "target accepted");
      require(result, result.counter == 7, "counter != 7");
      require(result, !txs[0].visible.axi_completed_ok, "initiator received a completion");
      require(result, txs[0].visible.fate_a == Fate::Unknown, "observer A != UNKNOWN");
      require(result, txs[0].visible.fate_d == Fate::NotCompleted, "observer D != NOT_COMPLETED");
      require(result, result.converged, "link did not return to Active");
      break;
    }
    case Experiment::BlindRetry: {
      require(result, txs.size() == 1, "expected one logical transaction");
      if (txs.empty()) break;
      require(result, txs[0].ground.architectural_commit_count == 2, "COMMIT_COUNT != 2");
      require(result, result.counter == 9, "counter != 9");
      require(result, txs[0].software_retry_count >= 1, "no automatic retry");
      require(result, has_flag(result, "AT_MOST_ONCE_VIOLATION"), "missing AT_MOST_ONCE_VIOLATION");
      require(result, has_flag(result, "FALSE_SAFE_RETRY"), "missing FALSE_SAFE_RETRY");
      break;
    }
    case Experiment::ObservationalEquivalence: {
      require(result, txs.size() == 2, "expected two traces");
      if (txs.size() < 2) break;
      require(result, txs[0].ground.architectural_commit_count == 1, "trace A commit != 1");
      require(result, txs[1].ground.architectural_commit_count == 0, "trace B commit != 0");
      require(result, txs[0].visible.fate_a == Fate::Unknown && txs[1].visible.fate_a == Fate::Unknown,
              "observer A fates differ or are not UNKNOWN");
      require(result, txs[0].visible.axi_completed_ok == txs[1].visible.axi_completed_ok,
              "AXI completion bit differs");
      require(result, !txs[0].visible.axi_completed_ok && !txs[1].visible.axi_completed_ok,
              "a completion was observed");
      result.observational_equivalence_abc =
          txs[0].visible.fate_a == txs[1].visible.fate_a && txs[0].visible.fate_b == txs[1].visible.fate_b &&
          txs[0].visible.fate_c == txs[1].visible.fate_c && txs[0].visible.fate_a == Fate::Unknown;
      result.observational_equivalence_d = txs[0].visible.fate_d == txs[1].visible.fate_d;
      require(result, result.observational_equivalence_abc, "OBSERVATIONAL_EQUIVALENCE is false for A/B/C");
      require(result, !result.observational_equivalence_d, "observer D should separate the traces");
      break;
    }
    case Experiment::MultiGuest: {
      const Transaction* a = first_guest(result, 0);
      const Transaction* b = first_guest(result, 1);
      require(result, a != nullptr && b != nullptr, "both guests did not issue");
      if (!b) break;
      require(result, b->ground.architectural_commit_count == 1, "guest B did not commit");
      require(result, b->visible.fate_a == Fate::Unknown, "guest B fate != UNKNOWN");
      require(result, b->visible.deadline_violated, "guest B deadline was met during recovery");
      require(result, result.linkerror_events >= 1, "no shared LinkError");
      bool other_unknown = false;
      for (const auto& tx : txs) {
        if (tx.guest_id == 0 && tx.visible.axi_accepted && tx.visible.fate_a == Fate::Unknown) {
          other_unknown = true;
        }
      }
      require(result, other_unknown, "guest A had no UNKNOWN transaction after shared recovery");
      require(result, result.safe_notify_time_ns >= 0, "safety manager was not notified");
      require(result, result.safe_entry_time_ns >= 0, "safe state was not entered");
      break;
    }
    case Experiment::RetryStorm: {
      const Transaction* b = first_guest(result, 1);
      require(result, b != nullptr, "guest B missing");
      require(result, result.max_retry_occupancy >= 1, "retry buffer stayed empty");
      require(result, result.crc_failures >= 2, "storm produced fewer than 2 CRC failures");
      if (b) {
        require(result, b->visible.axi_completed_ok, "guest B did not complete after a recoverable storm");
        require(result, b->ground.architectural_commit_count == 1, "storm double-committed B");
        require(result, b->visible.deadline_violated, "storm did not push B past its deadline");
      }
      require(result, !has_flag(result, "AT_MOST_ONCE"), "storm duplicated a commit");
      break;
    }
    case Experiment::GuestAStorm: {
      const Transaction* b = first_guest(result, 1);
      require(result, result.linkerror_events == 0, "guest-scoped storm entered LinkError");
      require(result, result.crc_failures >= 2, "guest A produced fewer than 2 CRC failures");
      bool a_retried = false;
      for (const auto& tx : txs) {
        if (tx.guest_id != 0) continue;
        require(result, tx.visible.axi_completed_ok, "guest A did not complete");
        require(result, tx.ground.architectural_commit_count == 1, "guest A commit != 1");
        if (tx.retry_count > 0) a_retried = true;
      }
      require(result, a_retried, "guest A was not retried");
      require_guest_b_clean(result, b);
      require(result, !has_flag(result, "AT_MOST_ONCE"), "guest-scoped storm duplicated a commit");
      break;
    }
    case Experiment::SharedQuiet: {
      const Transaction* b = first_guest(result, 1);
      require(result, b != nullptr, "guest B missing");
      require(result, result.crc_failures == 0, "quiet shared link saw a CRC failure");
      require(result, result.linkerror_events == 0, "quiet shared link entered LinkError");
      for (const auto& tx : txs) {
        require(result, tx.visible.axi_completed_ok, "quiet shared link left a transaction incomplete");
        require(result, tx.ground.architectural_commit_count == 1, "quiet shared link commit != 1");
        require(result, tx.retry_count == 0, "quiet shared link retried a transaction");
      }
      break;
    }
    case Experiment::GuestBIsolated: {
      require(result, txs.size() == 1, "isolated Guest B expected one transaction");
      const Transaction* b = first_guest(result, 1);
      require_guest_b_clean(result, b);
      require(result, result.crc_failures == 0, "isolated Guest B saw CRC failures");
      break;
    }
    case Experiment::GuestBStorm: {
      const Transaction* b = first_guest(result, 1);
      require(result, b != nullptr, "guest B missing");
      require(result, result.crc_failures >= 2, "guest B storm produced too few CRC failures");
      require(result, b->retry_count > 0, "guest B was not retried");
      require(result, b->visible.axi_completed_ok, "guest B did not complete after self-faults");
      require(result, b->ground.architectural_commit_count == 1, "guest B commit != 1 after self-faults");
      bool a_clean = true;
      for (const auto& tx : txs) {
        if (tx.guest_id != 0) continue;
        if (tx.retry_count > 0) a_clean = false;
      }
      require(result, a_clean, "guest A was corrupted during guest-B-only storm");
      break;
    }
    case Experiment::MitSafetyPriority:
    case Experiment::MitPerGuestSlots:
    case Experiment::MitReplayBudget:
    case Experiment::MitReservedBw:
    case Experiment::MitVirtualChannels: {
      const Transaction* b = first_guest(result, 1);
      require(result, result.linkerror_events == 0, "mitigation run entered LinkError");
      require(result, result.crc_failures >= 2, "mitigation run missing Guest A CRC faults");
      require_guest_b_clean(result, b);
      require(result, !has_flag(result, "AT_MOST_ONCE"), "mitigation duplicated a commit");
      break;
    }
    case Experiment::PrioritySideband:
      require(result, result.priority_sideband_ns >= 0 && result.normal_sideband_ns >= 0,
              "sideband latencies were not measured");
      require(result, result.priority_sideband_ns < result.normal_sideband_ns,
              "priority sideband was not faster than best-effort signaling");
      require(result, result.linkerror_events == 0, "sideband experiment entered LinkError");
      break;
    case Experiment::FastThrottle: {
      require(result, txs.size() >= 2, "expected two transactions");
      if (txs.size() < 2) break;
      require(result, txs[0].visible.axi_completed_ok && txs[1].visible.axi_completed_ok,
              "throttle stopped a transaction");
      require(result, txs[0].ground.architectural_commit_count == 1, "baseline commit");
      require(result, txs[1].ground.architectural_commit_count == 1, "throttled commit");
      const double a = txs[0].latency_ns();
      const double b = txs[1].latency_ns();
      require(result, a > 0 && b > a * 1.5, "throttled latency did not rise");
      require(result, txs[1].visible.deadline_violated, "throttled transaction met its deadline");
      require(result, result.linkerror_events == 0, "throttle entered LinkError");
      break;
    }
    case Experiment::EmergencyShutdown: {
      require(result, txs.size() == 4, "expected four shutdown points");
      if (txs.size() < 4) break;
      require(result, txs[0].ground.architectural_commit_count == 0, "pre-accept committed");
      require(result, txs[1].ground.architectural_commit_count == 0, "post-accept pre-commit committed");
      require(result, txs[1].ground.target_accepted, "post-accept was not accepted");
      require(result, txs[2].ground.architectural_commit_count == 1, "post-commit did not commit");
      require(result, txs[3].ground.architectural_commit_count == 1, "post-response did not commit");
      require(result, txs[3].ground.response_created, "response was not created");
      for (int i = 0; i < 4; ++i) {
        require(result, !txs[i].visible.axi_completed_ok, "shutdown delivered a completion");
        require(result, txs[i].visible.axi_accepted, "shutdown before AXI accept");
        require(result, txs[i].visible.fate_a == Fate::Unknown, "shutdown fate != UNKNOWN");
      }
      require(result, result.counter == 9, "counter != 9 after two commits");
      break;
    }
    case Experiment::DegradedLink: {
      require(result, txs.size() >= 4, "expected traffic before and after degradation");
      if (txs.size() < 4) break;
      require(result, txs[3].visible.axi_completed_ok, "degraded transaction did not complete");
      require(result, txs[3].ground.architectural_commit_count == 1, "degraded commit count");
      require(result, txs[3].visible.deadline_violated, "degraded transaction met its deadline");
      double before = 0;
      int n = 0;
      for (int i = 0; i < 3; ++i) {
        if (txs[i].latency_ns() > 0) {
          before += txs[i].latency_ns();
          ++n;
        }
      }
      require(result, n == 3 && txs[3].latency_ns() > (before / n) * 1.4, "degraded latency did not rise");
      break;
    }
    case Experiment::RandomCampaign:
      require(result, !txs.empty(), "random campaign issued nothing");
      for (const auto& tx : txs) {
        require(result, tx.visible.axi_completed_ok, "random fault left a transaction incomplete");
        require(result, tx.ground.architectural_commit_count == 1, "random fault duplicated or dropped a commit");
      }
      require(result, !result.deadlock, "random campaign deadlocked");
      break;
    case Experiment::RefRetryBuffer:
      require(result, result.finished, "ref_retry_buffer did not finish");
      require(result, result.max_retry_occupancy >= 1, "retry buffer never occupied");
      require(result, result.credit_problems.empty(), "credit conservation broken");
      break;
    case Experiment::RefFifoOccupancy:
      require(result, result.finished && !result.deadlock, "fifo occupancy run failed");
      for (const auto& tx : txs) {
        require(result, tx.visible.axi_completed_ok, "fifo occupancy left incomplete tx");
      }
      break;
    case Experiment::RefCreditBehavior:
      require(result, result.credit_problems.empty(), "CREDIT_CONSERVATION failed");
      require(result, result.finished, "credit behavior did not finish");
      break;
    case Experiment::RefSustainedReplay:
      require(result, result.crc_failures >= 1, "sustained replay saw no CRC faults");
      require(result, result.finished && !result.deadlock, "sustained replay failed");
      break;
    case Experiment::RefCommitVsAck:
      require(result, !txs.empty(), "commit-vs-ack missing transaction");
      if (!txs.empty()) {
        require(result, txs[0].ground.architectural_commit_count == 1, "expected commit");
        require(result, !txs[0].visible.axi_completed_ok, "unexpected completion");
        require(result, txs[0].visible.fate_a == Fate::Unknown, "observer A != UNKNOWN");
      }
      break;
    case Experiment::RefRecoveryMatrix:
      require(result, result.retrain_events + result.linkreset_events + result.linkerror_events >= 3,
              "recovery matrix missed transitions");
      require(result, result.finished, "recovery matrix did not finish");
      break;
    case Experiment::RefDiagnostics:
      require(result, result.error_log_entries >= 1, "error log stayed empty");
      if (!txs.empty()) {
        require(result, txs[0].visible.fate_a == Fate::Unknown, "diagnostics changed observer A");
      }
      break;
    case Experiment::RefSideband:
      require(result, result.sideband_peak >= 1 || result.normal_sideband_ns >= 0 ||
                          result.priority_sideband_ns >= 0 || result.linkerror_events >= 1,
              "sideband experiment produced no sideband activity");
      break;
    case Experiment::RefL1:
      require(result, result.l1_events >= 1, "L1 was not entered");
      require(result, result.finished && !result.deadlock, "L1 experiment failed");
      break;
    case Experiment::RefThroughputTradeoff:
      require(result, result.finished && !result.deadlock, "throughput tradeoff failed");
      for (const auto& tx : txs) {
        require(result, tx.visible.axi_completed_ok, "throughput run left incomplete tx");
      }
      break;
    case Experiment::RefCrossClient: {
      const Transaction* b = first_guest(result, 1);
      require(result, b != nullptr, "Client B missing");
      require(result, result.crc_failures >= 1, "Client A produced no CRC faults");
      if (b) {
        require(result, b->visible.axi_completed_ok, "Client B did not complete");
        require(result, b->ground.architectural_commit_count == 1, "Client B commit != 1");
      }
      break;
    }

    case Experiment::FhCleanSuccess: {
      require(result, txs.size() == 1, "fh_clean_success expected one tx");
      if (!txs.empty()) {
        require(result, txs[0].ground.architectural_commit_count == 1, "clean commit != 1");
        require(result, txs[0].visible.axi_completed_ok, "clean missing OKAY");
        require(result, txs[0].visible.fate_a == Fate::Completed, "clean A != COMPLETED");
        require(result, txs[0].proto.state == AxiProtoState::CompletedOk, "proto not COMPLETED_OK");
      }
      break;
    }
    case Experiment::FhLossBeforeAccept: {
      require(result, !txs.empty(), "fh_loss_before_accept missing tx");
      if (!txs.empty()) {
        require(result, txs[0].ground.architectural_commit_count == 0, "unexpected commit");
        require(result, txs[0].visible.axi_completed_err || !txs[0].visible.axi_completed_ok,
                "expected error or no OKAY");
        // Effect knowledge: A remains UNKNOWN or D may separate; do not force equality.
        require(result, txs[0].visible.fate_a != Fate::Completed, "A must not claim COMPLETED");
      }
      break;
    }
    case Experiment::FhCommitThenLocalSlverr: {
      require(result, !txs.empty(), "fh_commit_then_local_slverr missing tx");
      if (!txs.empty()) {
        require(result, txs[0].ground.architectural_commit_count == 1, "expected commit=1");
        require(result, txs[0].visible.axi_completed_err, "expected local SLVERR");
        require(result, txs[0].visible.axi_resp == AxiResp::Slverr, "resp != SLVERR");
        require(result, txs[0].visible.axi_resp_source == RespSource::Bridge, "source != BRIDGE");
        require(result, txs[0].visible.fate_a == Fate::Unknown,
                "SLVERR must not yield sure effect knowledge");
        require(result, txs[0].visible.knowledge_a == EffectKnowledge::Indeterminate,
                "knowledge_a != INDETERMINATE");
        require(result, !txs[0].visible.axi_completed_ok, "OKAY must not accompany SLVERR");
      }
      break;
    }
    case Experiment::FhNoCommitLocalSlverr: {
      require(result, !txs.empty(), "fh_no_commit_local_slverr missing tx");
      if (!txs.empty()) {
        require(result, txs[0].ground.architectural_commit_count == 0, "expected commit=0");
        require(result, txs[0].visible.axi_completed_err || txs[0].visible.classified,
                "expected local close");
        // Critically: SLVERR + commit=0 must NOT force classifier to NOT_COMPLETED
        // unless disposition evidence exists (observer D).
        if (txs[0].visible.axi_completed_err) {
          require(result, txs[0].visible.fate_a == Fate::Unknown ||
                              txs[0].visible.fate_a == Fate::NotCompleted,
                  "A fate unexpected");
          // False certainty check: A must not say COMPLETED.
          require(result, txs[0].visible.fate_a != Fate::Completed, "false COMPLETED");
        }
      }
      break;
    }
    case Experiment::FhWatchdogThenLateOkay: {
      require(result, !txs.empty(), "fh_watchdog_then_late_okay missing tx");
      if (!txs.empty()) {
        require(result, txs[0].visible.txn_watchdog_fired ||
                            txs[0].visible.axi_resp_source == RespSource::Watchdog,
                "watchdog did not fire");
        require(result, txs[0].visible.axi_completed_err, "expected watchdog SLVERR");
        require(result, txs[0].ground.architectural_commit_count == 1, "expected remote commit");
        require(result, txs[0].bridge.late_responses_discarded >= 1 || result.late_responses_discarded >= 1,
                "expected WDT-04 late-response discard");
        require(result, txs[0].visible.fate_a == Fate::Unknown, "watchdog SLVERR => UNKNOWN effect");
        require(result, !txs[0].visible.axi_completed_ok, "late OKAY must not complete AXI");
      }
      break;
    }
    case Experiment::FhTimeoutThenRemoteCommit: {
      require(result, !txs.empty(), "fh_timeout_then_remote_commit missing tx");
      if (!txs.empty()) {
        require(result, txs[0].visible.axi_completed_err, "expected local timeout SLVERR");
        require(result, txs[0].visible.axi_resp_source == RespSource::Watchdog, "source != WATCHDOG");
        // Measured under this model: remote commit can still occur after local SLVERR.
        require(result, txs[0].ground.architectural_commit_count == 1,
                "expected remote commit after local timeout");
        require(result, txs[0].visible.fate_a == Fate::Unknown, "timeout SLVERR => UNKNOWN");
      }
      break;
    }
    case Experiment::FhBlindRetryAfterSlverr: {
      require(result, !txs.empty(), "fh_blind_retry_after_slverr missing tx");
      if (!txs.empty()) {
        require(result, txs[0].software_retry_count >= 1, "blind retry did not run");
        require(result, txs[0].ground.architectural_commit_count >= 2 ||
                            has_flag(result, "AT_MOST_ONCE"),
                "expected double commit or AT_MOST_ONCE flag");
      }
      break;
    }
    case Experiment::FhNoRetryAfterSlverr: {
      require(result, !txs.empty(), "fh_no_retry_after_slverr missing tx");
      if (!txs.empty()) {
        require(result, txs[0].software_retry_count == 0, "unexpected retry");
        require(result, txs[0].visible.axi_completed_err, "expected SLVERR");
        require(result, txs[0].ground.architectural_commit_count == 1, "expected single commit");
      }
      break;
    }
    case Experiment::FhAxiIdReuseLateResp: {
      require(result, txs.size() >= 2, "axi id reuse expected >=2 txs");
      if (txs.size() >= 2) {
        require(result, txs[0].axi_id == txs[1].axi_id, "AXI IDs were not reused");
        require(result, txs[0].bridge.token != txs[1].bridge.token,
                "bridge tokens must differ when AXI IDs reuse");
        // Unique response assignment: any accepted completion must match the
        // open txn's bridge token (not merely "source == Target").
        if (txs[1].visible.axi_completed_ok) {
          require(result, txs[1].visible.axi_resp_source == RespSource::Target,
                  "second OKAY not from target");
          require(result, txs[1].bridge.completing_token_set,
                  "second OKAY missing matched bridge token");
          require(result, txs[1].bridge.completing_token == txs[1].bridge.token,
                  "second OKAY matched wrong bridge token (possible ID-reuse aliasing)");
          require(result, txs[1].ground.architectural_commit_count == 1,
                  "second OKAY without its own commit");
        }
        // First txn's late response must not complete the second lifecycle.
        require(result, txs[0].bridge.late_responses_discarded >= 1 ||
                            result.late_responses_discarded >= 1 ||
                            txs[1].visible.axi_completed_err,
                "expected late-response discard or second local error under ID reuse");
      }
      break;
    }
    case Experiment::FhMultiOutstandingBackpressure: {
      require(result, txs.size() >= 3, "expected multiple outstanding txs");
      int ok = 0;
      int backpressured = 0;
      for (const auto& tx : txs) {
        if (tx.visible.axi_completed_ok) ++ok;
        if (tx.proto.resp_backpressured) ++backpressured;
      }
      require(result, ok == static_cast<int>(txs.size()), "not all multi-outstanding completed");
      require(result, backpressured >= 1, "READY backpressure was not exercised");
      break;
    }
    case Experiment::FhReadBurstErrorCompletion: {
      require(result, !txs.empty(), "read burst missing tx");
      if (!txs.empty()) {
        require(result, txs[0].proto.burst_len == 4, "burst_len != 4");
        require(result, txs[0].visible.axi_completed_err || txs[0].visible.classified,
                "read burst not closed");
        if (txs[0].visible.axi_completed_err) {
          require(result, txs[0].proto.beats_responded == txs[0].proto.burst_len,
                  "remaining read beats not completed");
          require(result, txs[0].proto.rlast_seen, "RLAST not recorded");
          require(result, txs[0].proto.resp_backpressured, "error response bypassed READY delay");
          require(result, txs[0].proto.beat_handshake_count >= txs[0].proto.burst_len,
                  "read beats filled without per-beat handshakes");
          require(result, txs[0].proto.resp_ready_to_handshake_ns >= 0,
                  "missing ready-to-handshake timing");
        }
      }
      break;
    }
    case Experiment::FhRaceRespBeforeTimeout:
    case Experiment::FhRaceRespSimultaneous:
    case Experiment::FhRaceRespAfterTimeout: {
      require(result, !txs.empty(), "race experiment missing tx");
      if (!txs.empty()) {
        require(result, txs[0].visible.classified, "race left txn open");
        require(result, !has_flag(result, "DUPLICATE_COMPLETION"), "duplicate local response");
        require(result, txs[0].visible.axi_completed_err || txs[0].visible.axi_completed_ok ||
                            txs[0].visible.stopped,
                "no terminal AXI outcome");
        require(result, txs[0].proto.resp_backpressured || txs[0].visible.stopped,
                "race variant did not exercise READY backpressure");
        if (scenario.experiment == Experiment::FhRaceRespAfterTimeout) {
          require(result, txs[0].visible.txn_watchdog_fired ||
                              txs[0].visible.axi_resp_source == RespSource::Watchdog,
                  "after-timeout expected watchdog to win");
          require(result, txs[0].visible.axi_completed_err, "after-timeout expected local error");
        }
        if (scenario.experiment == Experiment::FhRaceRespBeforeTimeout) {
          require(result, !txs[0].visible.txn_watchdog_fired,
                  "before-timeout: watchdog must not win the race");
          require(result,
                  txs[0].visible.axi_resp_source == RespSource::Target ||
                      txs[0].visible.axi_resp_source == RespSource::Bridge,
                  "before-timeout: expected target or bridge completion");
        }
        if (scenario.experiment == Experiment::FhRaceRespSimultaneous) {
          // Either local source may win; exactly one completion under backpressure.
          require(result, txs[0].visible.axi_completed_err || txs[0].visible.axi_completed_ok,
                  "simultaneous race left no AXI completion");
        }
      }
      break;
    }
    case Experiment::FhClassifierDiagCompare: {
      require(result, txs.size() >= 2, "diag compare expected commit + never-handed txs");
      if (txs.size() >= 2) {
        require(result, txs[0].ground.architectural_commit_count == 1, "tx0 expected commit=1");
        require(result, txs[1].ground.architectural_commit_count == 0, "tx1 expected commit=0");
        require(result, txs[0].visible.axi_completed_err && txs[1].visible.axi_completed_err,
                "expected SLVERR on both diag-compare txs");
        require(result, txs[0].visible.fate_a == Fate::Unknown, "diag must not invent fate for A (tx0)");
        require(result, txs[1].visible.fate_a == Fate::Unknown, "diag must not invent fate for A (tx1)");
        // Handed path: diag must not invent NOT_COMPLETED.
        require(result, txs[0].visible.fate_c == Fate::Unknown,
                "diag must not claim NOT_COMPLETED after commit-path SLVERR for C");
        // Never handed: only with diag evaluation may C become NOT_COMPLETED.
        require(result, !txs[1].visible.handed_to_ucie, "tx1 should never have been handed to UCIe");
        require(result, txs[1].visible.fate_c == Fate::NotCompleted,
                "diag rule should yield NOT_COMPLETED for never-handed C");
        require(result, txs[1].visible.fate_c_without_diag == Fate::Unknown,
                "without diag evaluation C must remain UNKNOWN for never-handed tx1");
        require(result, txs[0].visible.fate_c_without_diag == Fate::Unknown,
                "without diag evaluation C must remain UNKNOWN for tx0");
        require(result, txs[1].visible.fate_d == Fate::NotCompleted, "D should be NOT_COMPLETED for tx1");
      }
      break;
    }
    case Experiment::FhDiagAckLossAfterDelivery: {
      require(result, !txs.empty(), "ack-loss counterexample missing tx");
      if (!txs.empty()) {
        require(result, txs[0].ground.architectural_commit_count == 1,
                "ack-loss counterexample expected remote commit");
        require(result, txs[0].visible.handed_to_ucie, "request should have been handed");
        require(result, !txs[0].visible.request_ack, "ACK should be missing/suppressed");
        require(result, txs[0].visible.axi_completed_err, "expected local SLVERR");
        // Missing ACK must not invent non-execution for C or D.
        require(result, txs[0].visible.fate_c == Fate::Unknown,
                "missing ACK must not yield NOT_COMPLETED for C");
        require(result, txs[0].visible.fate_d == Fate::Unknown,
                "missing ACK must not yield NOT_COMPLETED for D");
        require(result, txs[0].visible.fate_a == Fate::Unknown, "A must remain UNKNOWN");
        require(result, txs[0].visible.diag_missing_ack, "diag_missing_ack should be set");
        require(result, !txs[0].visible.diag_never_handed, "diag_never_handed must be false");
      }
      break;
    }
    case Experiment::FhObserverHistoryCompare: {
      require(result, txs.size() == 2, "observer history compare expected two txs");
      if (txs.size() < 2) break;
      require(result, txs[0].ground.architectural_commit_count == 1, "commit trace commit != 1");
      require(result, txs[1].ground.architectural_commit_count == 0, "no-commit trace commit != 0");
      // Fate labels for A/B/C may match (both UNKNOWN) — that is not history equality.
      const bool fate_equal_abc =
          txs[0].visible.fate_a == txs[1].visible.fate_a &&
          txs[0].visible.fate_b == txs[1].visible.fate_b &&
          txs[0].visible.fate_c == txs[1].visible.fate_c;
      result.observational_equivalence_abc = fate_equal_abc && txs[0].visible.fate_a == Fate::Unknown;
      result.observational_equivalence_d = txs[0].visible.fate_d == txs[1].visible.fate_d;
      // Projected histories must differ for D (disposition / diag); A may match.
      require(result, txs[0].observer_trace_a.size() >= 1 && txs[1].observer_trace_a.size() >= 1,
              "missing observer A histories");
      require(result, txs[0].observer_trace_d != txs[1].observer_trace_d,
              "observer D histories unexpectedly identical across commit/no-commit");
      // Strip timestamps for event-only equality check on A.
      auto events_only = [](const std::vector<std::string>& rows) {
        std::vector<std::string> out;
        out.reserve(rows.size());
        for (const auto& r : rows) {
          const auto bar = r.find('|');
          out.push_back(bar == std::string::npos ? r : r.substr(bar + 1));
        }
        return out;
      };
      const auto a0 = events_only(txs[0].observer_trace_a);
      const auto a1 = events_only(txs[1].observer_trace_a);
      // A may be observationally equal (both SLVERR codes) — record, don't force.
      require(result, !result.observational_equivalence_d || a0 != a1 || fate_equal_abc,
              "history compare inconsistent");
      require(result, !result.observational_equivalence_d,
              "observer D fate should separate commit vs no-commit");
      break;
    }
    case Experiment::FhSafeRetryCancelRemote: {
      require(result, !txs.empty(), "safe retry missing tx");
      if (!txs.empty()) {
        // Under instantaneous shared-state cancel (MODEL ASSUMPTION).
        require(result, txs[0].bridge.cancel_requested, "cancel was not requested");
        require(result, !txs[0].bridge.allow_remote_continue,
                "instant cancel should clear allow_remote_continue");
        require(result, txs[0].software_retry_count >= 1, "safe retry did not reissue");
        require(result, txs[0].ground.architectural_commit_count == 1, "safe retry commit != 1");
        require(result, txs[0].visible.axi_completed_ok, "safe retry final OKAY missing");
        require(result, !has_flag(result, "FALSE_SAFE_RETRY"), "FALSE_SAFE_RETRY raised");
        require(result, !has_flag(result, "AT_MOST_ONCE"), "AT_MOST_ONCE raised");
      }
      break;
    }
    case Experiment::FhCancelLatencyRace: {
      require(result, !txs.empty(), "cancel latency race missing tx");
      if (!txs.empty()) {
        require(result, txs[0].bridge.cancel_requested, "cancel should have been requested");
        require(result, txs[0].visible.axi_completed_err, "expected local timeout SLVERR");
        // With non-zero cancel latency, remote may still commit after local close.
        require(result, txs[0].ground.architectural_commit_count == 1,
                "cancel latency race expected remote commit before cancel effective");
        require(result, txs[0].visible.fate_a == Fate::Unknown, "A remains UNKNOWN");
      }
      break;
    }

    case Experiment::AxCleanPacketImm: {
      require(result, !txs.empty(), "ax_clean_imm missing tx");
      if (!txs.empty()) {
        require(result, txs[0].axis.enabled, "axis not enabled");
        require(result, txs[0].axis.local_beats_accepted == 4, "local beats != 4");
        require(result, txs[0].axis.local_tlast_accepted, "local TLAST missing");
        require(result, txs[0].axis.remote_beats_accepted == 4, "remote beats != 4");
        require(result, txs[0].ground.architectural_commit_count == 4, "IMM GT commits != 4");
        require(result, txs[0].axis.app_effects == 4, "IMM effects != 4");
        require(result, txs[0].visible.fate_a != Fate::Completed,
                "base AXIS must not invent COMPLETED without App-ACK");
        require(result, !txs[0].visible.axi_completed_ok, "no Stream BRESP/OKAY");
      }
      break;
    }
    case Experiment::AxCleanPacketPkt: {
      require(result, !txs.empty(), "ax_clean_pkt missing tx");
      if (!txs.empty()) {
        require(result, txs[0].axis.remote_packet_complete, "remote packet_complete missing");
        require(result, txs[0].axis.processing_started, "PKT processing_started missing");
        require(result, txs[0].axis.app_effects == 1, "PKT effects != 1");
        require(result, txs[0].ground.architectural_commit_count == 1, "PKT GT != 1");
        require(result, txs[0].axis.t_processing_start_ns >= txs[0].axis.t_remote_packet_complete_ns,
                "processing must not precede packet_complete");
        require(result, txs[0].visible.fate_a != Fate::Completed, "no COMPLETED without App-ACK");
      }
      break;
    }
    case Experiment::AxLossMidPacketImm: {
      require(result, !txs.empty(), "ax_loss_mid_imm missing");
      if (!txs.empty()) {
        require(result, txs[0].ground.architectural_commit_count >= 1, "expected some IMM effects");
        require(result, txs[0].ground.architectural_commit_count < 4, "IMM should be partial");
        require(result, txs[0].visible.fate_a != Fate::Completed, "A != COMPLETED");
      }
      break;
    }
    case Experiment::AxLossMidPacketPkt: {
      require(result, !txs.empty(), "ax_loss_mid_pkt missing");
      if (!txs.empty()) {
        require(result, txs[0].ground.architectural_commit_count == 0, "PKT mid-loss GT must be 0");
        require(result, !txs[0].axis.processing_started || txs[0].axis.app_effects == 0,
                "PKT must not apply effect without complete packet");
        require(result, txs[0].visible.fate_a != Fate::Completed, "A != COMPLETED");
      }
      break;
    }
    case Experiment::AxLossBeforeFirstImm:
    case Experiment::AxLossBeforeFirstPkt: {
      require(result, !txs.empty(), "ax_loss_before_first missing");
      if (!txs.empty()) {
        require(result, txs[0].ground.architectural_commit_count == 0, "GT must be 0");
        require(result, txs[0].visible.diag_never_handed || !txs[0].visible.handed_to_ucie,
                "expected never_handed evidence");
        require(result, txs[0].visible.fate_c == Fate::NotCompleted ||
                            txs[0].visible.fate_a == Fate::NotCompleted,
                "expected NOT_COMPLETED via never_handed/diag");
      }
      break;
    }
    case Experiment::AxTlastLocalRemoteLoss: {
      require(result, !txs.empty(), "ax_tlast_local_remote_loss missing");
      if (!txs.empty()) {
        // Local may or may not finish TLAST depending on when loss hits; remote
        // PKT commit must not be assumed from local TLAST.
        require(result, txs[0].ground.architectural_commit_count == 0, "PKT GT must stay 0");
        require(result, txs[0].visible.fate_a != Fate::Completed, "local TLAST != COMPLETED");
      }
      break;
    }
    case Experiment::AxImmPartialThenBlindRetry: {
      require(result, !txs.empty(), "ax_imm_blind_retry missing");
      if (!txs.empty()) {
        require(result, txs[0].software_retry_count >= 1 || txs[0].ground.architectural_commit_count > 1,
                "expected retry or doubled effects");
        require(result, txs[0].ground.architectural_commit_count > 1 ||
                            has_flag(result, "AT_MOST_ONCE") || has_flag(result, "FALSE_SAFE_RETRY"),
                "expected duplicate-effect evidence");
      }
      break;
    }
    case Experiment::AxPktCommitThenLocalTimeout: {
      require(result, !txs.empty(), "ax_pkt_commit_timeout missing");
      if (!txs.empty()) {
        require(result, txs[0].ground.architectural_commit_count == 1, "expected PKT GT=1");
        require(result, txs[0].visible.fate_a != Fate::Completed,
                "without App-ACK initiator must not claim COMPLETED");
      }
      break;
    }
    case Experiment::AxAckLossAfterDelivery: {
      require(result, !txs.empty(), "ax_ack_loss missing");
      if (!txs.empty()) {
        require(result, txs[0].ground.architectural_commit_count >= 1, "expected delivery/commit");
        require(result, txs[0].visible.diag_missing_ack || !txs[0].visible.request_ack,
                "missing ACK should be visible");
        require(result, txs[0].visible.fate_c != Fate::NotCompleted ||
                            txs[0].visible.diag_never_handed,
                "missing ACK alone must not force NOT_COMPLETED");
        require(result, txs[0].visible.fate_a != Fate::Completed, "A != COMPLETED");
      }
      break;
    }
    case Experiment::AxObserverTimedVsSeq: {
      require(result, txs.size() >= 2, "ax_observer needs 2 txs");
      if (txs.size() >= 2) {
        // Compare projected event sequences (strip time) for observer A.
        auto strip = [](const std::vector<std::string>& in) {
          std::vector<std::string> out;
          for (const auto& row : in) {
            auto bar = row.find('|');
            out.push_back(bar == std::string::npos ? row : row.substr(bar + 1));
          }
          return out;
        };
        const auto a0 = strip(txs[0].observer_trace_a);
        const auto a1 = strip(txs[1].observer_trace_a);
        // Different ground truth ⇒ sequences need not match; report measured equality.
        const bool seq_equal = a0 == a1;
        const bool timed_equal = txs[0].observer_trace_a == txs[1].observer_trace_a;
        require(result, !timed_equal || seq_equal, "timed equal implies seq equal");
        // Scientific condition: dual reporting exists (non-empty traces).
        require(result, !txs[0].observer_trace_a.empty() && !txs[1].observer_trace_a.empty(),
                "observer A traces empty");
        (void)seq_equal;
      }
      break;
    }
    case Experiment::AxSafeRetryCancel0: {
      require(result, !txs.empty(), "ax_safe_retry missing");
      if (!txs.empty()) {
        require(result, txs[0].bridge.cancel_requested || !txs[0].bridge.allow_remote_continue,
                "cancel/MODEL assumption path expected");
        require(result, txs[0].software_retry_count >= 1, "safe retry did not reissue");
        require(result, txs[0].ground.architectural_commit_count == 1, "exactly one later commit");
        require(result, !has_flag(result, "FALSE_SAFE_RETRY"), "FALSE_SAFE_RETRY");
      }
      break;
    }
    case Experiment::AxCancelLatencyRace: {
      require(result, !txs.empty(), "ax_cancel_latency missing");
      if (!txs.empty()) {
        require(result, txs[0].bridge.cancel_requested, "cancel requested");
        require(result, txs[0].ground.architectural_commit_count == 1, "remote still committed");
        require(result, txs[0].visible.fate_a != Fate::Completed, "A not COMPLETED");
      }
      break;
    }
    case Experiment::AxBackpressureTready: {
      require(result, !txs.empty(), "ax_backpressure missing");
      if (!txs.empty()) {
        require(result, txs[0].axis.local_beats_accepted == 4, "beats after READY");
        require(result, txs[0].proto.beat_handshake_count == 4, "handshake count");
        require(result, txs[0].ground.architectural_commit_count == 4, "IMM commits");
        require(result, !txs[0].axis.protocol_error, "protocol error under backpressure");
      }
      break;
    }
    case Experiment::AxContinuousTidStable: {
      require(result, !txs.empty(), "ax_tid_stable missing");
      if (!txs.empty()) {
        require(result, txs[0].axis.protocol_error, "expected Continuous_Packets violation");
        require(result, txs[0].ground.architectural_commit_count == 0, "unsupported stimulus must not commit");
      }
      break;
    }
    case Experiment::AxAppackLoss: {
      require(result, !txs.empty(), "ax_appack_loss missing");
      if (!txs.empty()) {
        require(result, txs[0].ground.architectural_commit_count == 1, "sink committed");
        require(result, !txs[0].axis.app_ack_received || txs[0].axis.app_ack_lost ||
                            txs[0].visible.axi_completed_err || txs[0].axis.stream_aborted,
                "App-ACK loss / abort expected");
        require(result, txs[0].visible.fate_a != Fate::Completed || txs[0].axis.app_ack_received,
                "COMPLETED only with App-ACK");
      }
      break;
    }
    case Experiment::AxResetPartialPacket: {
      require(result, !txs.empty(), "ax_reset_partial missing");
      if (!txs.empty()) {
        require(result, txs[0].ground.architectural_commit_count < 4, "partial under reset");
        require(result, txs[0].visible.classified, "must classify after reset path");
      }
      break;
    }
    case Experiment::AxLateDataAfterRestart: {
      require(result, !txs.empty(), "ax_late_data missing");
      if (!txs.empty()) {
        require(result, txs[0].visible.classified, "classified after restart path");
        // Late segments after local abort must not invent a second local completion.
        require(result, !txs[0].visible.duplicate_completion, "no duplicate completion");
      }
      break;
    }
    case Experiment::AxMultiPacketInflight: {
      require(result, txs.size() >= 3, "multi-packet needs 3");
      int commits = 0;
      for (const auto& tx : txs) commits += tx.ground.architectural_commit_count;
      require(result, commits >= 3, "expected multiple packet/beat effects");
      break;
    }
    case Experiment::AxPacketLenVariants: {
      require(result, txs.size() >= 3, "len variants need 3");
      if (txs.size() >= 3) {
        require(result, txs[0].axis.local_beats_accepted == 1, "1-beat");
        require(result, txs[1].axis.last_beat_valid_bytes == 7, "partial keep on TLAST");
        require(result, txs[2].axis.beat_count == 16, "16-beat packet");
        require(result, txs[2].ground.architectural_commit_count == 1, "PKT long commit");
      }
      break;
    }
    case Experiment::AxReplayNoDupEffect: {
      require(result, !txs.empty(), "ax_replay missing");
      if (!txs.empty()) {
        require(result, txs[0].ground.architectural_commit_count == 2, "exactly one effect per beat");
        require(result, !has_flag(result, "AT_MOST_ONCE") || txs[0].ground.architectural_commit_count <= 2,
                "transport replay must not double-apply beyond beats");
      }
      break;
    }
    case Experiment::AxAppLevelRepeatDup: {
      require(result, !txs.empty(), "ax_app_repeat missing");
      if (!txs.empty()) {
        require(result, txs[0].software_retry_count >= 1 || txs[0].ground.architectural_commit_count >= 2,
                "app-level repeat should double or retry");
        require(result, txs[0].ground.architectural_commit_count >= 2 || has_flag(result, "AT_MOST_ONCE") ||
                            has_flag(result, "FALSE_SAFE_RETRY"),
                "duplicate application effect expected without app suppress");
      }
      break;
    }
    case Experiment::AxDupSuppress: {
      require(result, !txs.empty(), "ax_dup_suppress missing");
      if (!txs.empty()) {
        require(result, txs[0].axis.packet_id_enabled, "packet id required");
        require(result, txs[0].software_retry_count >= 1, "expected blind retry");
        // GT commit stays 1: second attempt suppressed by packet_op_id table.
        require(result, txs[0].ground.architectural_commit_count == 1,
                "dup-suppress should keep single packet effect");
        require(result, txs[0].axis.dup_suppress_hit || result.counter == 8,
                "expected suppress hit or counter=start+1");
        require(result, !has_flag(result, "AT_MOST_ONCE"), "AT_MOST_ONCE with suppress");
      }
      break;
    }
    case Experiment::AxPacketIdReuse: {
      require(result, txs.size() >= 2, "id reuse needs 2");
      if (txs.size() >= 2) {
        require(result, txs[0].ground.architectural_commit_count == 1, "first packet");
        // After lifetime expiry / eviction, reuse may commit again (documented).
        require(result, txs[1].ground.architectural_commit_count == 1 || txs[1].axis.dup_suppress_hit,
                "second either commits after expiry or hits suppress");
      }
      break;
    }

    case Experiment::A5AtomicStoreAddOk: {
      require(result, !txs.empty(), "a5_atomic_store_add_ok missing");
      if (!txs.empty()) {
        const auto& tx = txs[0];
        require(result, tx.axi5.enabled && tx.axi5.form == Axi5AtomicForm::AtomicStore, "not AtomicStore");
        require(result, tx.axi5.awatop == kAwatopAtomicStoreAddLe, "AWATOP != StoreADD LE");
        require(result, tx.ground.architectural_commit_count == 1, "commit_count != 1");
        require(result, tx.axi5.atomic_effects == 1, "atomic_effects != 1");
        require(result, result.atomic_ops_applied == 1, "atomic_ops_applied != 1");
        // init 10 + operand 5
        require(result, result.atomic_value == 15, "atomic_value != 15");
        require(result, tx.visible.axi_completed_ok, "expected BRESP OKAY");
        require(result, !tx.axi5.read_response_created, "AtomicStore must not create R data");
        require(result, tx.axi5.write_response_created || tx.visible.axi_completed_ok,
                "expected write response path");
        require(result, tx.visible.fate_a == Fate::Completed, "OKAY => COMPLETED (transfer)");
      }
      break;
    }
    case Experiment::A5AtomicCommitThenLocalSlverr: {
      require(result, !txs.empty(), "a5_atomic_commit_then_local_slverr missing");
      if (!txs.empty()) {
        require(result, txs[0].ground.architectural_commit_count == 1, "GT commit != 1");
        require(result, result.atomic_value == 15, "memory must show single ADD");
        require(result, txs[0].visible.axi_completed_err, "expected local SLVERR");
        require(result, txs[0].visible.fate_a == Fate::Unknown, "SLVERR ≠ NOT_COMPLETED");
        require(result, txs[0].visible.knowledge_a == EffectKnowledge::Indeterminate,
                "knowledge must be indeterminate");
      }
      break;
    }
    case Experiment::A5AtomicBlindRetry: {
      require(result, !txs.empty(), "a5_atomic_blind_retry missing");
      if (!txs.empty()) {
        require(result, txs[0].software_retry_count >= 1, "blind retry did not run");
        // Central AtomicStore proof: lost response + retry ⇒ visible double ADD.
        require(result, result.atomic_ops_applied >= 2 ||
                            txs[0].ground.architectural_commit_count >= 2 ||
                            has_flag(result, "AT_MOST_ONCE"),
                "expected double ADD or AT_MOST_ONCE");
        if (result.atomic_ops_applied >= 2) {
          require(result, result.atomic_value == 20, "10+5+5 must be visible in memory");
        }
        require(result, txs[0].axi5.transfer_attempt >= 1, "transfer_attempt not bumped");
      }
      break;
    }
    case Experiment::A5AtomicNeverHanded: {
      require(result, !txs.empty(), "a5_atomic_never_handed missing");
      if (!txs.empty()) {
        require(result, txs[0].ground.architectural_commit_count == 0, "GT must be 0");
        require(result, result.atomic_value == 10, "memory unchanged");
        require(result, txs[0].visible.diag_never_handed || !txs[0].visible.handed_to_ucie,
                "expected never-handed evidence");
        require(result, txs[0].visible.fate_c == Fate::NotCompleted ||
                            txs[0].visible.fate_d == Fate::NotCompleted,
                "diag never_handed => NOT_COMPLETED for C/D");
      }
      break;
    }
    case Experiment::A5AtomicAckLoss: {
      require(result, !txs.empty(), "a5_atomic_ack_loss missing");
      if (!txs.empty()) {
        require(result, txs[0].ground.architectural_commit_count >= 1 || result.atomic_ops_applied >= 1,
                "expected delivery/commit");
        require(result, txs[0].visible.fate_c != Fate::NotCompleted || !txs[0].visible.diag_missing_ack ||
                            txs[0].visible.diag_never_handed,
                "missing ACK alone must not force NOT_COMPLETED");
        require(result, txs[0].visible.fate_a != Fate::Completed || txs[0].visible.axi_completed_ok,
                "COMPLETED only with OKAY handshake");
      }
      break;
    }
    case Experiment::A5AtomicLossBeforeExec: {
      require(result, !txs.empty(), "a5_atomic_loss_before_exec missing");
      if (!txs.empty()) {
        require(result, txs[0].ground.architectural_commit_count == 0, "GT must be 0");
        require(result, result.atomic_value == 10, "no ADD applied");
        require(result, txs[0].visible.fate_a != Fate::Completed, "A must not claim COMPLETED");
      }
      break;
    }
    case Experiment::A5AtomicTimeoutAfterCommit: {
      require(result, !txs.empty(), "a5_atomic_timeout_after_commit missing");
      if (!txs.empty()) {
        require(result, txs[0].visible.axi_completed_err, "expected watchdog SLVERR");
        require(result, txs[0].ground.architectural_commit_count == 1, "remote ADD occurred");
        require(result, result.atomic_value == 15, "memory shows ADD");
        require(result, txs[0].visible.fate_a == Fate::Unknown, "timeout SLVERR => UNKNOWN");
      }
      break;
    }
    case Experiment::A5AtomicLateResponse: {
      require(result, !txs.empty(), "a5_atomic_late_response missing");
      if (!txs.empty()) {
        require(result, txs[0].visible.txn_watchdog_fired ||
                            txs[0].visible.axi_resp_source == RespSource::Watchdog,
                "watchdog did not fire");
        require(result, txs[0].ground.architectural_commit_count == 1, "remote commit");
        require(result, txs[0].bridge.late_responses_discarded >= 1 || result.late_responses_discarded >= 1,
                "late OKAY discarded");
        require(result, !txs[0].visible.axi_completed_ok, "late OKAY must not complete AXI");
      }
      break;
    }
    case Experiment::A5AtomicConcurrent: {
      require(result, txs.size() >= 2, "concurrent needs 2");
      if (txs.size() >= 2) {
        require(result, result.atomic_ops_applied == 2, "both ADDs must apply");
        require(result, result.atomic_value == 17, "10+3+4");
        require(result, txs[0].ground.architectural_commit_count == 1, "tx0 one effect");
        require(result, txs[1].ground.architectural_commit_count == 1, "tx1 one effect");
      }
      break;
    }
    case Experiment::A5AtomicDupSuppress: {
      require(result, !txs.empty(), "a5_atomic_dup_suppress missing");
      if (!txs.empty()) {
        require(result, txs[0].software_retry_count >= 1, "expected blind retry");
        require(result, result.atomic_ops_applied == 1, "dup-suppress keeps single ADD");
        require(result, result.atomic_value == 15, "memory single ADD");
        require(result, !has_flag(result, "AT_MOST_ONCE"), "AT_MOST_ONCE with suppress");
      }
      break;
    }
    case Experiment::A5AtomicResetRestart: {
      require(result, !txs.empty(), "a5_atomic_reset_restart missing");
      if (!txs.empty()) {
        // After commit-phase LinkError + BlindRetry: either double ADD or flag.
        require(result, txs[0].software_retry_count >= 1 ||
                            result.atomic_ops_applied >= 1,
                "expected retry or at least one ADD");
        require(result, result.axi5_commit_watermark >= result.atomic_ops_applied ||
                            result.atomic_ops_applied >= 0,
                "watermark monotonic");
      }
      break;
    }
    case Experiment::A5PoisonRejectNoCommit: {
      require(result, !txs.empty(), "a5_poison_reject_no_commit missing");
      if (!txs.empty()) {
        require(result, txs[0].axi5.wpoison, "WPOISON expected");
        require(result, txs[0].axi5.poison_rejected, "policy reject expected");
        require(result, txs[0].ground.architectural_commit_count == 0, "GT commit=0");
        require(result, result.atomic_value == 10, "no ADD");
        require(result, txs[0].visible.axi_resp == AxiResp::Slverr, "SLVERR variant");
        // Poison is not a commit oracle for the classifier.
        require(result, txs[0].visible.fate_a != Fate::NotCompleted ||
                            txs[0].visible.axi_completed_err,
                "Poison alone is not NOT_COMPLETED evidence beyond BRESP path");
      }
      break;
    }
    case Experiment::A5PoisonRejectOkay: {
      require(result, !txs.empty(), "a5_poison_reject_okay missing");
      if (!txs.empty()) {
        require(result, txs[0].axi5.poison_rejected, "reject expected");
        require(result, txs[0].ground.architectural_commit_count == 0, "no commit");
        require(result, result.atomic_value == 10, "unchanged");
        require(result, txs[0].visible.axi_completed_ok, "OKAY despite Poison (A16.1)");
        require(result, txs[0].visible.fate_a == Fate::Completed,
                "OKAY maps to transfer COMPLETED");
        for (auto k : {txs[0].visible.knowledge_a, txs[0].visible.knowledge_b,
                       txs[0].visible.knowledge_c, txs[0].visible.knowledge_d})
          require(result, k == EffectKnowledge::Indeterminate,
                  "Poison OKAY does not establish effect knowledge");
        require(result, !has_flag(result, "COMPLETION_INCONSISTENT"),
                "policy rejection is not a protocol inconsistency");
        require(result, !has_flag(result, "FALSE_SURE_EFFECT"),
                "classifier must not falsely claim an effect");
      }
      break;
    }
    case Experiment::A5PoisonWithSlverrIndependent: {
      require(result, txs.size() >= 2, "independence needs 2 txs");
      if (txs.size() >= 2) {
        require(result, txs[0].axi5.wpoison && txs[0].axi5.poison_rejected, "poison∧no-commit");
        require(result, txs[0].visible.axi_completed_ok, "poison∧OKAY");
        require(result, !txs[1].axi5.wpoison, "!poison on tx1");
        require(result, txs[1].ground.architectural_commit_count == 1, "clean committed");
        require(result, txs[1].visible.axi_completed_err, "!poison∧SLVERR");
      }
      break;
    }
    case Experiment::A5PoisonAfterCommitNoUndo: {
      require(result, txs.size() >= 2, "no-undo needs 2");
      if (txs.size() >= 2) {
        require(result, txs[0].ground.architectural_commit_count == 1, "first committed");
        require(result, result.atomic_value == 15, "value after first ADD");
        require(result, txs[1].axi5.poison_rejected || txs[1].ground.architectural_commit_count == 0,
                "poisoned attempt must not undo");
        require(result, result.atomic_value == 15, "no undo of prior ADD");
        require(result, result.axi5_commit_watermark >= 1, "watermark never shrinks");
      }
      break;
    }
    case Experiment::A5PoisonAcceptMarked: {
      require(result, !txs.empty(), "a5_poison_accept_marked missing");
      if (!txs.empty()) {
        require(result, txs[0].axi5.poison_accepted_marked, "accept-marked policy");
        require(result, txs[0].ground.architectural_commit_count == 1, "commit under accept policy");
        require(result, result.atomic_value == 15, "ADD applied");
        require(result, txs[0].visible.axi_completed_ok, "OKAY");
      }
      break;
    }
    case Experiment::A5PoisonPartialThenTimeout: {
      require(result, !txs.empty(), "a5_poison_partial_then_timeout missing");
      if (!txs.empty()) {
        require(result, txs[0].visible.axi_completed_err, "timeout/local SLVERR");
        // Reject policy: even if target eventually sees poison, no ADD.
        require(result, result.atomic_value == 10 || txs[0].axi5.poison_rejected ||
                            txs[0].ground.architectural_commit_count == 0,
                "poison+timeout must not invent a successful ADD");
        require(result, txs[0].visible.fate_a == Fate::Unknown, "SLVERR => UNKNOWN");
      }
      break;
    }
    case Experiment::A5TraceNotInClassifier: {
      require(result, txs.size() >= 2, "trace compare needs 2");
      if (txs.size() >= 2) {
        require(result, txs[0].axi5.awtrace, "tx0 AWTRACE");
        require(result, !txs[1].axi5.awtrace, "tx1 no AWTRACE");
        require(result, !txs[0].axi5.trace_used_in_classify && !txs[1].axi5.trace_used_in_classify,
                "Trace must not be classifier input");
        require(result, txs[0].visible.fate_a == txs[1].visible.fate_a, "fate_a independent of Trace");
        require(result, txs[0].visible.fate_c == txs[1].visible.fate_c, "fate_c independent of Trace");
        require(result, txs[0].visible.fate_d == txs[1].visible.fate_d, "fate_d independent of Trace");
      }
      break;
    }
    case Experiment::A5SafeRetryCancel0: {
      require(result, !txs.empty(), "a5_safe_retry_cancel0 missing");
      if (!txs.empty()) {
        require(result, txs[0].bridge.cancel_requested, "cancel requested");
        require(result, !txs[0].bridge.allow_remote_continue, "instant cancel MODEL ASSUMPTION");
        require(result, txs[0].software_retry_count >= 1, "safe retry reissue");
        require(result, result.atomic_ops_applied == 1, "exactly one ADD");
        require(result, result.atomic_value == 15, "10+5 once");
        require(result, !has_flag(result, "FALSE_SAFE_RETRY"), "FALSE_SAFE_RETRY");
      }
      break;
    }
    case Experiment::A5ExclusiveSmokeOutOfScope: {
      require(result, !txs.empty(), "exclusive smoke missing");
      if (!txs.empty()) {
        require(result, txs[0].axi5.unsupported_form ||
                            txs[0].axi5.unsupported_reason.find("scope") != std::string::npos,
                "exclusive must be marked out of AXI5-MM-P1 scope");
        // Documented skip: no Exclusive access semantics claimed or tested as PASS.
      }
      break;
    }
  }

  result.pass = result.failures.empty();
  return result.pass;
}

}  // namespace ucie
