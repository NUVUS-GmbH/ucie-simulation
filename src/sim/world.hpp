#pragma once

// Shared simulation state. SystemC modules wait on the events in this object.
// They do not read GroundTruth when they classify a transaction.

#include "axi/target_model.hpp"
#include "common/arbitration.hpp"
#include "common/config.hpp"
#include "common/queue.hpp"
#include "experiments/scenario.hpp"
#include "faults/fault.hpp"
#include "ucie/credit_manager.hpp"
#include "ucie/flit.hpp"
#include "ucie/retry_buffer.hpp"

#include <systemc>

#include <deque>
#include <map>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ucie {

struct TraceEvent {
  double time_ns = 0;
  std::string event;
  std::string component;
  std::uint64_t transaction_id = 0;
  std::uint32_t guest_id = 0;
  std::string state;
  std::string details;
};

struct QueueSample {
  double time_ns = 0;
  std::string name;
  std::size_t occupancy = 0;
};

struct StateSample {
  double time_ns = 0;
  std::string state;
};

struct Partial {
  bool request = false;
  bool data = false;
  bool queued = false;
};

// [MODEL] Implementation diagnostic/error log. Fields are study abstractions,
// not standardized UCIe register layouts.
struct ErrorLogEntry {
  double time_ns = -1;
  std::string kind;
  int retry_count = 0;
  std::size_t retry_occupancy = 0;
  std::size_t tx_occupancy = 0;
  std::size_t rx_occupancy = 0;
  int credit_available = 0;
  int credit_held = 0;
  std::uint64_t transaction_id = 0;
  std::string details;
};

// Snapshot of buffer/credit/AXI state taken immediately before a recovery
// transition. Used by the reference recovery-matrix experiment.
struct RecoverySnapshot {
  std::string transition;
  double time_ns = -1;
  std::size_t tx_fifo = 0;
  std::size_t rx_fifo = 0;
  std::size_t retry_buffer = 0;
  int credit_available = 0;
  int credit_held = 0;
  int outstanding_axi = 0;
  int outstanding_responses = 0;
  std::string classification;  // SPEC / MODEL / ASSUMPTION / POLICY
};

struct LinkCtrl {
  LinkState state = LinkState::Reset;
  AouState aou_a = AouState::Disabled;
  AouState aou_b = AouState::Disabled;
  bool busy = false;
  // Set for LinkError, LinkReset, and Emergency Shutdown. Retrain leaves this
  // false so a transaction already delivered to the target can still commit.
  bool destructive = false;
  bool cmd_pending = false;
  RecoveryCmd cmd = RecoveryCmd::None;
  bool fail_retrain = false;
  bool preserve_retry = true;
  bool remote_recovers = true;
  std::uint64_t epoch = 1;
  std::uint64_t wire_gen = 1;
  int retrain_events = 0;
  int linkreset_events = 0;
  int linkerror_events = 0;
  int crc_failures = 0;
  int transparent_retries = 0;
  bool fault_time_set = false;
  double fault_time_ns = -1;
  bool detection_set = false;
  double detection_time_ns = -1;
  double link_error_time_ns = -1;
  double recovery_time_ns = -1;
  bool recovery_set = false;
  int streak[2] = {0, 0};
  std::uint8_t next_seq[2] = {0, 0};
  std::uint8_t expected_seq[2] = {0, 0};
  // Per-guest sequence spaces used by virtual-channel / per-guest-slot mitigations.
  // Indexed [direction][guest].
  std::uint8_t next_seq_guest[2][3] = {{0, 0, 0}, {0, 0, 0}};
  std::uint8_t expected_seq_guest[2][3] = {{0, 0, 0}, {0, 0, 0}};
  bool resend[2] = {false, false};
  double rate_scale = 1;
  bool fast_throttle = false;
  bool emergency = false;
  bool converged = false;
  sc_core::sc_event cmd_ev;
  sc_core::sc_event idle_ev;
  sc_core::sc_event state_ev;
  sc_core::sc_event flushed;
};

struct World {
  Config cfg;
  Scenario scenario;
  TargetModel target;
  Arbiter arbiter;
  std::mt19937 rng;
  CorruptionArm arm;
  LinkCtrl link;

  std::vector<Transaction> txns;
  std::vector<Partial> partial;
  std::deque<std::uint32_t> reissue;
  std::set<std::pair<std::uint64_t, int>> accepted[2];
  std::map<std::pair<std::uint64_t, int>, int> storm_hits;

  CreditManager credit[2];
  RetryBuffer retry[2];

  TimedQueue<std::uint32_t> guest_q[3];
  TimedQueue<std::uint32_t> axi_q;
  TimedQueue<std::uint32_t> aou_q;
  TimedQueue<std::uint32_t> target_q;
  TimedQueue<std::uint32_t> completion_q;
  TimedQueue<Flit> send_q[2];
  TimedQueue<Flit> replay_q[2];
  TimedQueue<Flit> ctrl_q[2];
  TimedQueue<Flit> wire[2];
  TimedQueue<Flit> arrive[2];
  TimedQueue<SidebandRecord> sb_out;
  TimedQueue<SidebandRecord> sb_in;

  sc_core::sc_event up_ev;
  sc_core::sc_event progress;
  sc_core::sc_event quiescent;
  sc_core::sc_event watchdog_ev;
  sc_core::sc_event nak_ev[2];
  sc_core::sc_event ack_ev[2];
  sc_core::sc_event credit_ev[2];
  sc_core::sc_event timer_ev[2];

  std::vector<TraceEvent> trace;
  std::vector<QueueSample> occupancy;
  std::vector<StateSample> states;
  std::vector<SidebandRecord> sideband_log;
  std::vector<ErrorLogEntry> error_log;
  std::vector<RecoverySnapshot> recovery_snapshots;
  std::vector<std::string> credit_problems;
  std::vector<std::string> internal_errors;
  ResourceStats resources;

  // Pending early-credit returns awaiting the update threshold.
  int pending_credit_return[2] = {0, 0};
  int credits_issued[2] = {0, 0};
  int credits_consumed[2] = {0, 0};
  int credits_returned[2] = {0, 0};
  std::size_t tx_peak[2] = {0, 0};
  std::size_t rx_peak[2] = {0, 0};
  std::size_t sideband_peak = 0;
  double sideband_max_latency_ns = -1;
  int l1_events = 0;
  bool in_l1 = false;

  bool all_issued = false;
  bool deadlock = false;
  bool finished = false;
  double origin_ns = 0;
  double priority_sideband_ns = -1;
  double normal_sideband_ns = -1;
  double safe_notify_ns = -1;
  double safe_entry_ns = -1;
  bool safe_entered = false;
  std::uint64_t next_logical_id = 1;
  int reserved_countdown = 0;
  // Per-guest outstanding payload count on the initiator TX path (side 0).
  int outstanding_by_guest[3] = {0, 0, 0};

  World(const Config& config, Scenario scenario_in);

  double now_ns() const;
  sc_core::sc_time cycles(int n) const;
  sc_core::sc_time serialization() const;
  sc_core::sc_time propagation() const;
  int max_unacked() const;

  Transaction& slot(std::uint32_t index);
  Transaction* find(std::uint64_t logical_id);

  void log(TraceLevel need, const char* event, const char* component, const Transaction* tx,
           const std::string& details);
  void note(Transaction& tx, Phase phase, const char* component, const std::string& sentence);
  void sample(const std::string& name, std::size_t occ);
  void set_state(LinkState state);

  std::uint32_t allocate(const Issue& issue, int issue_index);
  void on_phase(Transaction& tx, Phase phase);
  void apply_tx(Flit& flit);
  void note_detection();
  void request(RecoveryCmd cmd);
  void classify_now(Transaction& tx);
  void complete(Transaction& tx);
  void abandon(Transaction& tx, bool fdi, bool explicit_unknown);
  void apply_policy();
  void observe_link_down();
  void flush_destructive();
  void audit_credits();
  void reinit_credits();
  bool already_accepted(int receiver, const Flit& flit) const;
  void remember(int receiver, const Flit& flit);
  void arm_replay_deadline(int side, std::uint8_t seq);

  void acquire_tx_slot(int side, const Flit& flit);
  void release_tx_slot(int side, const Flit& flit, bool was_replay);
  bool tx_slot_blocks(int side, const Flit& candidate) const;
  bool safety_guest_waiting(int side) const;
  bool prefer_safety_now() const;
  bool uses_per_guest_seq() const;
  bool allow_slot_bypass(const Flit& candidate) const;
  void attribute_guest_b_wait(WaitCause cause, double start_ns, double end_ns);
  void fold_guest_b_waits();

  void record_error(const std::string& kind, const Transaction* tx, const std::string& details);
  void note_credit_freed(int side);
  int take_credit_update(int side);
  int take_credit_update_force(int side);
  void capture_recovery_snapshot(const std::string& transition, const std::string& classification);
  bool mainband_active() const;
};

}  // namespace ucie
