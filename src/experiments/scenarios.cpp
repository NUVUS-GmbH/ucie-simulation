#include "scenario.hpp"

#include "common/config.hpp"

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
    if (issue.duplicate_completion) line << " DUPLICATE_COMPLETION";
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
  }

  result.pass = result.failures.empty();
  return result.pass;
}

}  // namespace ucie
