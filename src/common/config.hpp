#pragma once

#include "enums.hpp"

#include <string>

namespace ucie {

// Numbers loaded from YAML are timing abstractions unless a field is marked
// as a specification constant in config.cpp. They are not normative UCIe
// electrical or cycle values just because they appear in a config file.
struct Config {
  double clock_period_ns = 1.0;
  // [MODEL] cycle mode quantizes the digital handshake and buffer waits onto
  // clock_period_ns. It does not make the PHY, the FDI encoding, or the
  // millisecond-scale recovery timers cycle-accurate.
  bool cycle_mode = false;

  int axi_handshake_cycles = 1;
  int aou_pack_cycles = 2;
  int serialization_cycles = 4;
  int propagation_cycles = 2;
  int buffer_cycles = 1;
  int arbitration_cycles = 1;
  int target_accept_cycles = 2;
  int target_commit_cycles = 4;
  int target_response_cycles = 2;
  int completion_cycles = 1;

  // [MODEL] Not a single number stated as a universal UCIe retrain length.
  double retrain_ns = 5000;
  double link_reset_ns = 10000;
  double reactivation_ns = 2000;
  double sideband_message_ns = 50;
  double priority_sideband_ns = 20;

  // [SPEC] UCIe 2.0 §10.3.3.7 minimum LinkError residency is 16 ms.
  // [SPEC] UCIe 2.0 §10.3.3.8 sideband handshake timeout is 8 ms.
  // The timed model waits these durations. They are not shortened unless the
  // config file overrides them, and an override is no longer the spec value.
  double link_error_residency_ns = 16e6;
  double sideband_timeout_ns = 8e6;

  // [SPEC] UCIe 2.0 §3.8 REPLAY_TIMEOUT_FLIT_COUNT >= 375.
  int replay_timeout_flits = 375;
  // [SPEC] UCIe 2.0 §3.8 points at the PCIe replay rules and does not restate
  // the integer. 3 is the parameter this study uses, not a UCIe literal.
  int replay_limit = 3;
  // [SPEC] UCIe 2.0 §3.7 3-bit detection guarantee. Four-or-more-bit faults
  // are outside the set this model claims to detect.
  int crc_detection_bits = 3;
  // [SPEC] UCIe 2.0 §3.8 MAX_UNACKNOWLEDGED_FLITS cap.
  int max_unacknowledged_cap = 127;

  // Mainband FIFOs and retry buffer. Reference-slide values override these in
  // configs/torsten_reference_*.yaml. Defaults below are the older study sizes.
  int tx_buffer_entries = 16;
  int retry_buffer_entries = 32;
  int rx_buffer_entries = 16;
  int axi_queue_entries = 8;
  int aou_queue_entries = 8;
  int response_queue_entries = 8;
  int guest_queue_entries = 8;
  int credit_depth = 4;
  int ctrl_queue_entries = 64;
  // [REFERENCE-SLIDE] Sideband FIFO depth when set by the Torsten configs.
  int sideband_fifo_entries = 32;

  // [MODEL] When true, only one unacknowledged payload flit may occupy the TX
  // path per direction. The old cross-guest study relied on this. The Torsten
  // reference architecture sets this false so TX/RX/retry depths can matter.
  bool single_inflight_slot = true;

  // [REFERENCE-SLIDE] Early credit return with an update threshold (flits).
  // Threshold 1 = return every freed credit immediately.
  bool early_credit_return = true;
  int credit_update_threshold = 1;

  // [REFERENCE-SLIDE] Streaming / Format-6 metadata. Timing is not inferred.
  int link_width = 16;
  int fdi_width_bits = 512;
  int flit_format = 6;
  bool crc_enabled = true;
  bool retry_enabled = true;
  bool l1_enabled = false;
  bool error_logging = false;
  // "ucie2x" or "ucie3". UCIe 3 mode only enables modeled 3.0 mechanisms.
  std::string ucie_revision = "ucie2x";
  bool priority_sideband = false;

  bool retrain_preserves_retry = true;
  RecoveryPolicy recovery = RecoveryPolicy::Discard;
  TraceLevel trace = TraceLevel::Transactions;
  double sim_timeout_ns = 200e6;
  std::uint64_t seed = 1;
  double random_fault_probability = 0.0;
  int counter_start = 7;
  bool partition_credits = false;

  // [MODEL] L1 entry/exit dwell when L1 is enabled. Not a Swissbit timing claim.
  double l1_entry_ns = 100;
  double l1_exit_ns = 100;
  double l1_hold_ns = 500;

  static Config defaults();
  void overlay_yaml(const std::string& path);
};

}  // namespace ucie
