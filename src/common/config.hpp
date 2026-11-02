#pragma once

#include "axi/axi5_types.hpp"
#include "enums.hpp"

#include <cstdint>
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

  // --- Fault-handling draft V0.1 (PROJECT REQUIREMENT / REFERENCE CONFIG) ---
  // Transaction watchdog (WDT-03). Distinct from sim_timeout_ns.
  // Draft suggests ~1 ms start value and escalation after 3 timeouts — both
  // are project configuration, not AXI/UCIe constants.
  bool txn_watchdog_enabled = false;
  double txn_watchdog_ns = 1e6;  // 1 ms REFERENCE CONFIGURATION from draft
  int txn_watchdog_escalate_after = 3;
  bool txn_watchdog_during_recovery = true;  // may fire while link recovers
  // ERR-05 local SLVERR on LinkError/Link-Down for outstanding AXI.
  bool local_slverr_on_link_error = false;
  // WDT-04 discard late responses after local close.
  bool discard_late_responses = true;
  // ERR-07 master retry after error completion.
  MasterRetryPolicy master_retry = MasterRetryPolicy::Abort;
  // When true, classifier profiles C/D may *read* bridge diagnostic fields.
  // Presence of fields ≠ use; see use_diag_in_classifier.
  bool expose_bridge_diagnostics = false;
  bool use_diag_in_classifier = false;
  // Manager response READY delay (cycles) to exercise backpressure.
  int axi_resp_ready_delay_cycles = 0;
  // [MODEL ASSUMPTION] Bridge maps one AXI beat group to one AoU/UCIe message
  // pair (request [+wdata] / response). Not a full AXI4-Stream implementation.
  bool bridge_token_on_wire = true;  // MODEL: flit carries bridge token/generation
  // When true, local SLVERR/close requests remote cancel so the old attempt
  // cannot commit later. Instant effect (cancel_latency_ns==0) is a
  // MODEL ASSUMPTION (shared state), not a confirmed wire cancel protocol.
  bool cancel_remote_on_local_close = false;
  // Delay before cancel becomes effective at the remote. 0 = instantaneous.
  double cancel_latency_ns = 0;
  // Optional target-side duplicate suppression by bridge token (MODEL).
  bool target_duplicate_suppress = false;

  // --- AXIS-P1-Continuous (MODEL PROFILE; see docs/study/phase1_axis_axi5/) ---
  // Bridge-AXIS-P1: one UCIe segment per AXIS transfer. AoU normative mapping
  // remains ungeklärt (AoU full text absent); this is a named bridge policy.
  bool axis_profile_enabled = false;
  int axis_tdata_bytes = 16;
  int axis_tready_delay_cycles = 0;       // receiver backpressure
  int axis_tvalid_lead_cycles = 0;       // transmitter asserts TVALID first
  int axis_sink_process_start_cycles = 2;  // SINK-PKT: gap after packet_complete
  int axis_sink_effect_cycles = 2;         // time to apply one atomic effect
  bool axis_app_ack_enabled = false;
  bool axis_packet_id_enabled = false;
  bool axis_dup_suppress = false;
  int axis_dup_capacity = 16;
  double axis_dup_lifetime_ns = 0;
  bool axis_dup_clear_on_reset = true;
  // After local TLAST without App-ACK: wait for remote progress before settle.
  double axis_settle_grace_ns = 5000;

  // --- AXI5-MM-P1 (IHI0022L subset; see docs/study/phase1_axis_axi5/) ---
  // Bridge packing of AWATOP/WPOISON/Trace onto UCIe flits is MODEL PROFILE
  // only — no AoU conformity claim.
  bool axi5_mm_p1_enabled = false;
  PoisonTargetPolicy poison_target_policy = PoisonTargetPolicy::RejectWithoutCommitSlverr;
  bool axi5_trace_signals_present = true;
  // Hard constraint: classify()/InitiatorView must never read Trace.
  bool axi5_use_trace_in_classify = false;
  // Initial value at the AtomicStore address (evaluation GT).
  int axi5_atomic_location_init = 0;
  // Address used by AtomicStore(ADD) experiments when not overridden by Issue.
  std::uint64_t axi5_atomic_address = 0xA000;

  static Config defaults();
  void overlay_yaml(const std::string& path);
};

}  // namespace ucie
