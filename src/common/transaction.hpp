#pragma once

// Logical transaction. Retries keep the same logical_id.
//
// GroundTruth is never an argument of classify(). InitiatorVisible is the
// only state an observer profile may read, and view_for() strips fields the
// selected profile is not allowed to use.

#include "enums.hpp"

#include <cstdint>
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
};

struct InitiatorVisible {
  bool axi_accepted = false;
  bool axi_completed_ok = false;
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
  Fate fate_a = Fate::Unknown;
  Fate fate_b = Fate::Unknown;
  Fate fate_c = Fate::Unknown;
  Fate fate_d = Fate::Unknown;
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

  // Wait attribution for the cross-guest FFI experiment (Guest B focus).
  double wait_arbitration_ns = 0;
  double wait_guest_a_normal_ns = 0;
  double wait_guest_a_replay_ns = 0;
  double wait_own_replay_ns = 0;
  double wait_credit_ns = 0;
  double wait_target_ns = 0;
  double wait_response_ns = 0;
  double wait_recovery_ns = 0;

  double latency_ns() const {
    if (t_created_ns < 0 || t_completion_ns < 0) return -1;
    return t_completion_ns - t_created_ns;
  }
};

struct InitiatorView {
  bool axi_completed_ok = false;
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
  bool status_has_axi_id = false;
  bool status_has_commit_bit = false;
};

InitiatorView view_for(ObserverProfile profile, const Transaction& tx);

// Uses only InitiatorView. There is no commit-count parameter.
Fate classify(const InitiatorView& view);

struct InvariantIssue {
  std::uint64_t logical_id = 0;
  bool false_safe_retry = false;
  bool at_most_once = false;
  bool completion_inconsistent = false;
  bool duplicate_completion = false;
};

std::vector<InvariantIssue> check_transaction_invariants(const std::vector<Transaction>& txs);

}  // namespace ucie
