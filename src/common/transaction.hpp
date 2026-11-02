#pragma once

// Logical transaction. Retries keep the same logical_id.
//
// Three orthogonal state dimensions (see report §4):
//   A. AXI protocol state (handshake / response code / source)
//   B. Architectural ground truth (commit count / times) — evaluation only
//   C. Observer knowledge about the effect (Fate / EffectKnowledge)
//
// GroundTruth is never an argument of classify(). InitiatorVisible is the
// only state an observer profile may read, and view_for() strips fields the
// selected profile is not allowed to use.

#include "axi/axis_types.hpp"
#include "axi/axi5_types.hpp"
#include "enums.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ucie {

struct GroundTruth {
  int architectural_commit_count = 0;
  bool target_accepted = false;
  bool response_created = false;
  // Commit count latched when an automatic reissue was armed. -1 if never.
  // Read by invariant checks only, not by classify().
  int commit_at_reissue = -1;
  double commit_time_ns = -1;
  // Partial-effect bookkeeping for multi-beat ops (evaluation only).
  int beats_committed = 0;
  std::vector<double> commit_times_ns;
};

// [MODEL ASSUMPTION] Bridge-local identity. Not an AXI or UCIe wire field unless
// a concrete bridge contract exports it. Used for WDT-04 late-response matching.
struct BridgeIdentity {
  std::uint64_t token = 0;       // equals logical_id at issue in this model
  std::uint64_t generation = 0;  // bumped on local close / ID-reuse epoch
  bool local_closed = false;
  bool remote_inflight = false;
  bool late_response_seen = false;
  int late_responses_discarded = 0;
  // When true, remote target may still accept/commit after local AXI close.
  // Safe retry requires this false (or target duplicate suppression).
  // Instant clear is a MODEL ASSUMPTION (shared state), not a wire cancel protocol.
  bool allow_remote_continue = true;
  // If >= 0, remote continuation becomes forbidden at this time (cancel latency).
  double cancel_effective_ns = -1;
  bool cancel_requested = false;
  // Staged local SLVERR awaiting AXI READY handshake (ERR-05 / WDT-03).
  bool pending_local_slverr = false;
  RespSource pending_local_source = RespSource::None;
  // Bridge token of the response accepted into the completion path (assignment proof).
  std::uint64_t completing_token = 0;
  std::uint64_t completing_generation = 0;
  bool completing_token_set = false;
};

struct AxiProtocolRecord {
  AxiProtoState state = AxiProtoState::NotAccepted;
  AxiResp resp = AxiResp::Okay;
  RespSource resp_source = RespSource::None;
  double t_accepted_ns = -1;
  double t_resp_ready_ns = -1;   // BVALID/RVALID asserted (model)
  double t_resp_handshake_ns = -1;  // accepted by manager READY
  int burst_len = 1;             // AXI ARLEN/AWLEN+1 beats modeled
  int beats_accepted = 0;        // address/data acceptance progress
  int beats_responded = 0;       // R beats or write B (1)
  bool bvalid_asserted = false;
  bool bready_accepted = false;
  bool rlast_seen = false;
  // Backpressure: manager held READY low after resp_ready.
  bool resp_backpressured = false;
  // Timed read-beat handshakes under READY delay (not a zero-time fill loop).
  int beat_handshake_count = 0;
  double resp_ready_to_handshake_ns = -1;
};

struct InitiatorVisible {
  bool axi_accepted = false;
  bool axi_completed_ok = false;
  bool axi_completed_err = false;
  AxiResp axi_resp = AxiResp::Okay;
  RespSource axi_resp_source = RespSource::None;
  bool explicit_unknown = false;
  bool stopped = false;
  bool handed_to_ucie = false;
  bool data_attempted = false;
  bool request_ack = false;
  bool data_ack = false;
  bool response_ack = false;
  bool request_nak_only = false;
  bool data_nak_only = false;
  bool response_nak_only = false;
  bool request_timeout = false;
  bool data_timeout = false;
  bool response_timeout = false;
  bool response_at_local_aou = false;
  bool fdi_link_down = false;
  // Standardized status bits. They do not carry an AXI id or a commit bit.
  bool linkerror_status = false;
  bool crc_status = false;
  bool retrain_status = false;
  bool fast_throttle = false;
  bool emergency_shutdown = false;
  bool priority_sideband = false;
  bool status_has_axi_id = false;
  bool status_has_commit_bit = false;
  bool duplicate_completion = false;
  bool deadline_violated = false;
  bool txn_watchdog_fired = false;
  int txn_timeout_count = 0;
  // Optional bridge diagnostic fields (profile C/D may see; classifier may use).
  bool diag_bridge_local_slverr = false;
  bool diag_bridge_token_valid = false;
  // Bridge diagnostic: request never handed to UCIe TX (!handed_to_ucie).
  // Must NOT be inferred from a missing ACK — delivery can precede ACK.
  bool diag_never_handed = false;
  // Observational only: missing request ACK. Never used alone for NOT_COMPLETED.
  bool diag_missing_ack = false;
  std::uint64_t diag_bridge_token = 0;
  bool use_diag_fields = false;  // set per-run: information present vs classifier uses it
  // Classifier comparison: fates recomputed with diagnostics disabled.
  Fate fate_c_without_diag = Fate::Unknown;
  Fate fate_d_without_diag = Fate::Unknown;
  Fate fate_a = Fate::Unknown;
  Fate fate_b = Fate::Unknown;
  Fate fate_c = Fate::Unknown;
  Fate fate_d = Fate::Unknown;
  EffectKnowledge knowledge_a = EffectKnowledge::Indeterminate;
  EffectKnowledge knowledge_b = EffectKnowledge::Indeterminate;
  EffectKnowledge knowledge_c = EffectKnowledge::Indeterminate;
  EffectKnowledge knowledge_d = EffectKnowledge::Indeterminate;
  bool classified = false;
};

struct Transaction {
  std::uint64_t logical_id = 0;
  std::uint32_t axi_id = 0;
  std::uint32_t guest_id = 0;
  int source_chiplet = 0;
  int destination_chiplet = 1;
  OpKind op = OpKind::IncrementCounter;
  std::uint64_t address = 0;
  int size_bytes = 4;
  bool is_write = true;
  bool idempotent = false;
  Criticality criticality = Criticality::BestEffort;
  int issue_index = 0;
  int data = 1;

  double t_created_ns = -1;
  double t_axi_accepted_ns = -1;
  double t_aou_packed_ns = -1;
  double t_ucie_tx_ns = -1;
  double t_remote_rx_ns = -1;
  double t_target_accepted_ns = -1;
  double t_response_created_ns = -1;
  double t_response_tx_ns = -1;
  double t_completion_ns = -1;
  double t_fdi_ns = -1;
  double deadline_ns = -1;
  double t_watchdog_arm_ns = -1;
  double t_watchdog_fire_ns = -1;

  Phase phase = Phase::Created;
  int retry_count = 0;
  int replay_count = 0;
  int software_retry_count = 0;
  int recovery_epoch = 0;
  bool completion_received = false;
  bool pending_reissue = false;

  // Ambiguity window: commit -> observer can name a fate. Negative if no commit.
  // ambiguity_open stays true when the window did not close.
  double ambiguity_window_ns = -1;
  bool ambiguity_open = false;
  bool ambiguity_ended_unknown = false;

  GroundTruth ground;
  InitiatorVisible visible;
  AxiProtocolRecord proto;
  BridgeIdentity bridge;
  // AXI4-Stream record. When axis.enabled, MM response-channel fields are not
  // used as Stream BRESP (there is none). Local abort is bridge lifecycle only.
  AxisRecord axis;

  // AXI5-MM-P1 record (AtomicStore/Poison/Trace). Orthogonal to AXIS; never
  // mixed with Stream state. Trace fields must not enter InitiatorView.
  Axi5MmRecord axi5;

  // Wait attribution for the cross-guest FFI experiment (Guest B focus).
  double wait_arbitration_ns = 0;
  double wait_guest_a_normal_ns = 0;
  double wait_guest_a_replay_ns = 0;
  double wait_own_replay_ns = 0;
  double wait_credit_ns = 0;
  double wait_target_ns = 0;
  double wait_response_ns = 0;
  double wait_recovery_ns = 0;

  // Per-observer projected event history (for observational-equivalence diffs).
  std::vector<std::string> observer_trace_a;
  std::vector<std::string> observer_trace_b;
  std::vector<std::string> observer_trace_c;
  std::vector<std::string> observer_trace_d;

  double latency_ns() const {
    if (t_created_ns < 0 || t_completion_ns < 0) return -1;
    return t_completion_ns - t_created_ns;
  }
};

struct InitiatorView {
  // Locally issued Poison attribute; no target policy or execution state.
  bool poisoned_request = false;
  bool axi_completed_ok = false;
  bool axi_completed_err = false;
  AxiResp axi_resp = AxiResp::Okay;
  RespSource axi_resp_source = RespSource::None;
  bool axi_accepted = false;
  bool explicit_unknown = false;
  bool is_write = true;
  bool use_disposition = false;
  bool handed_to_ucie = false;
  bool data_attempted = false;
  bool request_ack = false;
  bool data_ack = false;
  bool request_nak_only = false;
  bool data_nak_only = false;
  bool request_timeout = false;
  bool data_timeout = false;
  bool fdi_link_down = false;
  bool linkerror_status = false;
  bool use_fdi = false;
  bool use_status = false;
  bool use_diag = false;
  bool diag_bridge_local_slverr = false;
  bool diag_bridge_token_valid = false;
  bool diag_never_handed = false;
  bool diag_missing_ack = false;
  bool status_has_axi_id = false;
  bool status_has_commit_bit = false;
  bool txn_watchdog_fired = false;
};

InitiatorView view_for(ObserverProfile profile, const Transaction& tx);

// Uses only InitiatorView. There is no commit-count parameter.
Fate classify(const InitiatorView& view);
EffectKnowledge classify_effect(const InitiatorView& view);

struct InvariantIssue {
  std::uint64_t logical_id = 0;
  bool false_safe_retry = false;
  bool at_most_once = false;
  bool completion_inconsistent = false;
  bool duplicate_completion = false;
  bool false_sure_no_effect = false;  // NOT_COMPLETED while ground commit > 0
  bool false_sure_effect = false;     // SURE_EFFECT without the required effect
  bool second_local_response = false;
};

std::vector<InvariantIssue> check_transaction_invariants(const std::vector<Transaction>& txs);

}  // namespace ucie
