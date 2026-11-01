#pragma once

#include "common/enums.hpp"
#include "common/transaction.hpp"
#include "faults/fault.hpp"

#include <string>
#include <vector>

namespace ucie {

struct Config;

struct Issue {
  int guest = 0;
  OpKind op = OpKind::IncrementCounter;
  std::uint64_t address = 0x1000;
  int data = 1;
  double delay_ns = 0;
  double deadline_ns = -1;
  Criticality criticality = Criticality::BestEffort;
  int priority = 0;
  bool wait_for_previous = false;
  int counter_reset = -1;
};

// Architectural mitigations for the cross-guest temporal-interference study.
// Only one mitigation should be enabled at a time. The baseline leaves all
// flags false and keeps the single shared in-flight payload slot.
struct Mitigation {
  bool safety_priority_on_replay = false;
  bool per_guest_tx_slots = false;
  int replay_yield_after = -1;  // <0 disables; else yield after N Guest A replays
  bool reserved_safety_bandwidth = false;
  bool virtual_channels = false;
};

struct ResourceStats {
  std::size_t retry_buffer_peak = 0;
  double tx_slot_hold_ns[3] = {0, 0, 0};
  double tx_slot_hold_replay_ns[3] = {0, 0, 0};
  double guest_b_wait_arbitration_ns = 0;
  double guest_b_wait_guest_a_normal_ns = 0;
  double guest_b_wait_guest_a_replay_ns = 0;
  double guest_b_wait_own_replay_ns = 0;
  double guest_b_wait_credit_ns = 0;
  double guest_b_wait_target_ns = 0;
  double guest_b_wait_response_ns = 0;
  double guest_b_wait_recovery_ns = 0;
  int tx_slot_owner = -1;
  bool tx_slot_is_replay = false;
  double tx_slot_acquired_ns = -1;
  int guest_a_replay_attempts = 0;
  int reserved_slots_granted = 0;
  int yield_events = 0;
};

struct Scenario {
  std::string name;
  Experiment experiment = Experiment::BaselineUnknown;
  std::vector<Issue> issues;
  std::vector<FaultSpec> faults;
  RecoveryPolicy policy = RecoveryPolicy::Discard;
  bool retrain_preserves_retry = true;
  bool sideband_saturation = false;
  SafetyAction on_link_error = SafetyAction::Record;
  SafetyAction on_deadline = SafetyAction::Record;
  SafetyAction on_thermal = SafetyAction::Record;
  SafetyAction on_shutdown = SafetyAction::Shutdown;
  int counter_start = 7;
  ArbPolicy arbitration = ArbPolicy::RoundRobin;
  std::vector<int> guest_priority{0, 1, 0};
  std::vector<int> guest_weight{1, 1, 1};
  std::vector<int> guest_outstanding{8, 4, 4};
  bool random_faults = false;
  Mitigation mitigation;
};

struct SidebandRecord {
  bool priority = false;
  std::string reason;
  double t_gen_ns = -1;
  double t_rx_ns = -1;
  double latency_ns = -1;
};

struct RunResult {
  std::string experiment;
  std::vector<Transaction> transactions;
  int counter = 0;
  int fifo_depth = 0;
  int dma_starts = 0;
  bool converged = false;
  bool deadlock = false;
  bool finished = false;
  int crc_failures = 0;
  int retrain_events = 0;
  int linkreset_events = 0;
  int linkerror_events = 0;
  int transparent_retries = 0;
  std::size_t max_retry_occupancy = 0;
  ResourceStats resources;
  double fault_time_ns = -1;
  double detection_time_ns = -1;
  double link_error_time_ns = -1;
  double recovery_time_ns = -1;
  double safe_notify_time_ns = -1;
  double safe_entry_time_ns = -1;
  double priority_sideband_ns = -1;
  double normal_sideband_ns = -1;
  double rate_scale = 1;
  bool observational_equivalence_abc = false;
  bool observational_equivalence_d = false;
  std::vector<std::string> credit_problems;
  std::vector<std::string> violations;
  std::vector<std::string> internal_errors;
  std::vector<SidebandRecord> sideband;
  std::size_t tx_peak = 0;
  std::size_t rx_peak = 0;
  std::size_t sideband_peak = 0;
  double sideband_max_latency_ns = -1;
  int credits_consumed = 0;
  int credits_returned = 0;
  int l1_events = 0;
  int error_log_entries = 0;
  bool pass = false;
  std::vector<std::string> failures;
};

Scenario make_scenario(Experiment experiment, Config& config);
bool evaluate(const Scenario& scenario, RunResult& result);

}  // namespace ucie
