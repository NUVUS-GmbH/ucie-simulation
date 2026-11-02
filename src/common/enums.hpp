#pragma once

// Shared enumerations for the timed UCIe simulator.
//
// Tags used throughout this tree:
//   [SPEC]        behavior taken from a cited UCIe or AoU rule
//   [MODEL]       timing or structure that is a simulation abstraction
//   [ASSUMPTION]  a reading the specification text available here does not require
//   [POLICY]      an implementation choice, configurable, not "the" UCIe answer

#include <cstdint>
#include <string_view>

namespace ucie {

enum class Phase : std::uint16_t {
  Created,
  AxiAccepted,
  AouPacked,
  WaitingForCredit,
  QueuedForLink,
  Transmitting,
  RemoteReceived,
  TargetAccepted,
  ArchitecturalCommit,
  ResponseCreated,
  ResponseQueued,
  ResponseTransmitting,
  ResponseReceived,
  Completed,
  Failed,
  Unknown,
};

enum class OpKind : std::uint8_t {
  MemoryWrite,
  MemoryRead,
  IncrementCounter,
  PushFifo,
  TriggerDma,
  StartAccelerator,
  RingDoorbell,
  AckInterrupt,
  IncrementActuator,
  PopFifo,
};

enum class Criticality : std::uint8_t { BestEffort, SafetyCritical };

enum class LinkState : std::uint8_t {
  Reset,
  Disabled,
  Active,
  Retrain,
  LinkReset,
  LinkError,
  Recovering,
  // [MODEL] L1 low-power residency. Enter/exit dwells are configuration.
  L1,
};

enum class AouState : std::uint8_t { Disabled, Activate, Enabled, Deactivate };

// Classifier label. For OKAY handshakes this is a *transfer-class* COMPLETED
// (handshake observed), mapped to EffectKnowledge::SureEffect. Ground-truth
// architectural effect is checked separately; COMPLETION_INCONSISTENT /
// FALSE_SURE_EFFECT fire when commit_count != 1. SLVERR implies neither
// COMPLETED nor NOT_COMPLETED by itself (default UNKNOWN).
enum class Fate : std::uint8_t { Completed, NotCompleted, Unknown };

// A. AXI protocol completion state (orthogonal to architectural commit).
enum class AxiProtoState : std::uint8_t {
  NotAccepted,
  Outstanding,
  CompletedOk,
  CompletedError,
};

// BRESP/RRESP encoding used by the model (AXI4 §A3.4.4 / Table A3-5).
enum class AxiResp : std::uint8_t { Okay, Exokay, Slverr, Decerr };

// Who produced the AXI response observed by the manager.
enum class RespSource : std::uint8_t {
  None,
  Target,
  Bridge,      // ERR-05 local completion on LinkError/Link-Down
  Watchdog,    // WDT-03 transaction timeout
  Recovery,    // other recovery-policy response
};

// C. Observer knowledge about the architectural effect (labels only; see Fate).
enum class EffectKnowledge : std::uint8_t {
  SureEffect,
  SureNoEffect,
  Indeterminate,
};

// What the initiator is allowed to read. Ground-truth commit count is never
// an input to these profiles.
enum class ObserverProfile : std::uint8_t {
  A_AxiOnly,
  B_AxiFdi,
  C_AxiFdiUcieStatus,
  D_AxiHypervisorMetadata,
};

// [POLICY] UCIe does not define the AXI completion that follows LinkError.
// LocalSlverr is PROJECT REQUIREMENT ERR-05 from the AXI-over-UCIe-S draft.
enum class RecoveryPolicy : std::uint8_t {
  Discard,          // A: no AXI completion
  PreserveLocal,    // B: deliver a response that already reached the local AoU
  ExplicitUnknown,  // C: a distinct UNKNOWN, not OKAY and not a retry permit
  AutoRetry,        // D: reissue the same logical transaction after the link is up
  NoAutoRetry,      // E: same initiator-visible result as Discard in this model
  LocalSlverr,      // ERR-05: complete outstanding AXI with SLVERR on LinkError
};

// [PROJECT REQUIREMENT] ERR-07 master policy after a local/error completion.
enum class MasterRetryPolicy : std::uint8_t {
  Abort,                 // no software retry
  BlindRetry,            // reissue after SLVERR without commit knowledge
  RetryOnlyIfNotCompleted,  // reissue only when classifier returns NOT_COMPLETED
};

enum class SafetyAction : std::uint8_t {
  Record,
  Continue,
  Retry,
  ResetEndpoint,
  ResetDevice,
  IsolateGuest,
  Degraded,
  SafeState,
  Shutdown,
};

enum class ArbPolicy : std::uint8_t { Fifo, RoundRobin, StrictPriority, WeightedRoundRobin };

enum class FaultKind : std::uint8_t {
  None,
  SingleBit,
  RepeatedCorruption,
  BurstError,
  DroppedFlit,
  DuplicatedFlit,
  DelayedFlit,
  CorruptedResponse,
  CorruptedRequest,
  AckNakLoss,
  RetryTimeout,
  RetryExhaustion,
  ForcedRetrain,
  ForcedLinkReset,
  ForcedLinkError,
  EndpointReset,
  RemoteUnresponsive,
  IncreasedLatency,
  TargetSlowdown,
  QueueCongestion,
  ArbitrationDelay,
  SidebandDelay,
  FastThrottle,
  EmergencyShutdown,
  RateReduction,
  CreditLoss,
  CreditCorruption,
  BufferUnavailable,
  GuestCreditHog,
  EnterL1,
  ExitL1,
};

enum class Trigger : std::uint8_t { AbsoluteTime, Phase, Condition, RandomCampaign };

enum class FlitRole : std::uint8_t {
  Request,
  WriteData,
  Response,
  Ack,
  Nak,
  CreditReturn,
  // AXIS-P1: one UCIe payload message per stream transfer (MODEL PROFILE).
  StreamSegment,
};

enum class TraceLevel : std::uint8_t { None, Errors, Summary, Transactions, Full };

enum class Experiment : std::uint8_t {
  BaselineUnknown,
  PrecommitFailure,
  BlindRetry,
  ObservationalEquivalence,
  MultiGuest,
  RetryStorm,
  GuestAStorm,
  SharedQuiet,
  GuestBIsolated,
  GuestBStorm,
  MitSafetyPriority,
  MitPerGuestSlots,
  MitReplayBudget,
  MitReservedBw,
  MitVirtualChannels,
  PrioritySideband,
  FastThrottle,
  EmergencyShutdown,
  DegradedLink,
  RandomCampaign,
  // Reference-architecture validation suite (Torsten / Swissbit-style).
  RefRetryBuffer,
  RefFifoOccupancy,
  RefCreditBehavior,
  RefSustainedReplay,
  RefCommitVsAck,
  RefRecoveryMatrix,
  RefDiagnostics,
  RefSideband,
  RefL1,
  RefThroughputTradeoff,
  RefCrossClient,
  // Fault-handling draft V0.1 experiment matrix (ERR-05 / WDT-03 / WDT-04 / ERR-07).
  FhCleanSuccess,
  FhLossBeforeAccept,
  FhCommitThenLocalSlverr,
  FhNoCommitLocalSlverr,
  FhWatchdogThenLateOkay,
  FhTimeoutThenRemoteCommit,
  FhBlindRetryAfterSlverr,
  FhNoRetryAfterSlverr,
  FhAxiIdReuseLateResp,
  FhMultiOutstandingBackpressure,
  FhReadBurstErrorCompletion,
  FhRaceRespBeforeTimeout,
  FhRaceRespSimultaneous,
  FhRaceRespAfterTimeout,
  FhClassifierDiagCompare,
  FhDiagAckLossAfterDelivery,
  FhObserverHistoryCompare,
  FhSafeRetryCancelRemote,
  FhCancelLatencyRace,
  // AXIS-P1-Continuous experiment matrix (ax_*).
  AxCleanPacketImm,
  AxCleanPacketPkt,
  AxLossMidPacketImm,
  AxLossMidPacketPkt,
  AxLossBeforeFirstImm,
  AxLossBeforeFirstPkt,
  AxTlastLocalRemoteLoss,
  AxImmPartialThenBlindRetry,
  AxPktCommitThenLocalTimeout,
  AxAckLossAfterDelivery,
  AxObserverTimedVsSeq,
  AxSafeRetryCancel0,
  AxCancelLatencyRace,
  AxBackpressureTready,
  AxContinuousTidStable,
  AxAppackLoss,
  AxResetPartialPacket,
  AxLateDataAfterRestart,
  AxMultiPacketInflight,
  AxPacketLenVariants,
  AxReplayNoDupEffect,
  AxAppLevelRepeatDup,
  AxDupSuppress,
  AxPacketIdReuse,
  // AXI5-MM-P1 experiment matrix (a5_*). Orthogonal to AXIS; MM path only.
  A5AtomicStoreAddOk,
  A5AtomicCommitThenLocalSlverr,
  A5AtomicBlindRetry,
  A5AtomicNeverHanded,
  A5AtomicAckLoss,
  A5AtomicLossBeforeExec,
  A5AtomicTimeoutAfterCommit,
  A5AtomicLateResponse,
  A5AtomicConcurrent,
  A5AtomicDupSuppress,
  A5AtomicResetRestart,
  A5PoisonRejectNoCommit,
  A5PoisonRejectOkay,
  A5PoisonWithSlverrIndependent,
  A5PoisonAfterCommitNoUndo,
  A5PoisonAcceptMarked,
  A5PoisonPartialThenTimeout,
  A5TraceNotInClassifier,
  A5SafeRetryCancel0,
  A5ExclusiveSmokeOutOfScope,
};

// Cause codes for Guest B wait attribution in the cross-guest FFI study.
enum class WaitCause : std::uint8_t {
  None,
  Arbitration,
  GuestANormalTraffic,
  GuestAReplay,
  OwnReplay,
  Credit,
  Target,
  Response,
  Recovery,
};

inline constexpr bool is_idempotent(OpKind op) {
  return op == OpKind::MemoryWrite || op == OpKind::MemoryRead;
}

inline constexpr bool is_read(OpKind op) {
  return op == OpKind::MemoryRead || op == OpKind::PopFifo;
}

inline std::string_view to_string(Phase p) {
  switch (p) {
    case Phase::Created: return "CREATED";
    case Phase::AxiAccepted: return "AXI_ACCEPTED";
    case Phase::AouPacked: return "AOU_PACKED";
    case Phase::WaitingForCredit: return "WAITING_FOR_CREDIT";
    case Phase::QueuedForLink: return "QUEUED_FOR_LINK";
    case Phase::Transmitting: return "TRANSMITTING";
    case Phase::RemoteReceived: return "REMOTE_RECEIVED";
    case Phase::TargetAccepted: return "TARGET_ACCEPTED";
    case Phase::ArchitecturalCommit: return "ARCHITECTURAL_COMMIT";
    case Phase::ResponseCreated: return "RESPONSE_CREATED";
    case Phase::ResponseQueued: return "RESPONSE_QUEUED";
    case Phase::ResponseTransmitting: return "RESPONSE_TRANSMITTING";
    case Phase::ResponseReceived: return "RESPONSE_RECEIVED";
    case Phase::Completed: return "COMPLETED";
    case Phase::Failed: return "FAILED";
    case Phase::Unknown: return "UNKNOWN";
  }
  return "?";
}

inline std::string_view to_string(Fate f) {
  switch (f) {
    case Fate::Completed: return "COMPLETED";
    case Fate::NotCompleted: return "NOT_COMPLETED";
    case Fate::Unknown: return "UNKNOWN";
  }
  return "?";
}

inline std::string_view to_string(AxiProtoState s) {
  switch (s) {
    case AxiProtoState::NotAccepted: return "NOT_ACCEPTED";
    case AxiProtoState::Outstanding: return "OUTSTANDING";
    case AxiProtoState::CompletedOk: return "COMPLETED_OK";
    case AxiProtoState::CompletedError: return "COMPLETED_ERROR";
  }
  return "?";
}

inline std::string_view to_string(AxiResp r) {
  switch (r) {
    case AxiResp::Okay: return "OKAY";
    case AxiResp::Exokay: return "EXOKAY";
    case AxiResp::Slverr: return "SLVERR";
    case AxiResp::Decerr: return "DECERR";
  }
  return "?";
}

inline std::string_view to_string(RespSource s) {
  switch (s) {
    case RespSource::None: return "NONE";
    case RespSource::Target: return "TARGET";
    case RespSource::Bridge: return "BRIDGE";
    case RespSource::Watchdog: return "WATCHDOG";
    case RespSource::Recovery: return "RECOVERY";
  }
  return "?";
}

inline std::string_view to_string(EffectKnowledge k) {
  switch (k) {
    case EffectKnowledge::SureEffect: return "SURE_EFFECT";
    case EffectKnowledge::SureNoEffect: return "SURE_NO_EFFECT";
    case EffectKnowledge::Indeterminate: return "INDETERMINATE";
  }
  return "?";
}

inline std::string_view to_string(LinkState s) {
  switch (s) {
    case LinkState::Reset: return "RESET";
    case LinkState::Disabled: return "DISABLED";
    case LinkState::Active: return "ACTIVE";
    case LinkState::Retrain: return "RETRAIN";
    case LinkState::LinkReset: return "LINK_RESET";
    case LinkState::LinkError: return "LINK_ERROR";
    case LinkState::Recovering: return "RECOVERING";
    case LinkState::L1: return "L1";
  }
  return "?";
}

inline std::string_view to_string(OpKind op) {
  switch (op) {
    case OpKind::MemoryWrite: return "MEMORY_WRITE";
    case OpKind::MemoryRead: return "MEMORY_READ";
    case OpKind::IncrementCounter: return "INCREMENT_COUNTER";
    case OpKind::PushFifo: return "PUSH_FIFO";
    case OpKind::TriggerDma: return "TRIGGER_DMA";
    case OpKind::StartAccelerator: return "START_ACCELERATOR";
    case OpKind::RingDoorbell: return "RING_DOORBELL";
    case OpKind::AckInterrupt: return "ACK_INTERRUPT";
    case OpKind::IncrementActuator: return "INCREMENT_ACTUATOR";
    case OpKind::PopFifo: return "POP_FIFO";
  }
  return "?";
}

inline std::string_view to_string(FlitRole r) {
  switch (r) {
    case FlitRole::Request: return "request";
    case FlitRole::WriteData: return "wdata";
    case FlitRole::Response: return "response";
    case FlitRole::Ack: return "ack";
    case FlitRole::Nak: return "nak";
    case FlitRole::CreditReturn: return "credit";
    case FlitRole::StreamSegment: return "stream_segment";
  }
  return "?";
}

inline std::string_view to_string(Experiment e) {
  switch (e) {
    case Experiment::BaselineUnknown: return "baseline_unknown";
    case Experiment::PrecommitFailure: return "precommit";
    case Experiment::BlindRetry: return "blind_retry";
    case Experiment::ObservationalEquivalence: return "observational_equivalence";
    case Experiment::MultiGuest: return "multi_guest";
    case Experiment::RetryStorm: return "retry_storm";
    case Experiment::GuestAStorm: return "guest_a_storm";
    case Experiment::SharedQuiet: return "shared_quiet";
    case Experiment::GuestBIsolated: return "guest_b_isolated";
    case Experiment::GuestBStorm: return "guest_b_storm";
    case Experiment::MitSafetyPriority: return "mit_safety_priority";
    case Experiment::MitPerGuestSlots: return "mit_per_guest_slots";
    case Experiment::MitReplayBudget: return "mit_replay_budget";
    case Experiment::MitReservedBw: return "mit_reserved_bw";
    case Experiment::MitVirtualChannels: return "mit_virtual_channels";
    case Experiment::PrioritySideband: return "priority_sideband";
    case Experiment::FastThrottle: return "fast_throttle";
    case Experiment::EmergencyShutdown: return "emergency_shutdown";
    case Experiment::DegradedLink: return "degraded_link";
    case Experiment::RandomCampaign: return "random_campaign";
    case Experiment::RefRetryBuffer: return "ref_retry_buffer";
    case Experiment::RefFifoOccupancy: return "ref_fifo_occupancy";
    case Experiment::RefCreditBehavior: return "ref_credit_behavior";
    case Experiment::RefSustainedReplay: return "ref_sustained_replay";
    case Experiment::RefCommitVsAck: return "ref_commit_vs_ack";
    case Experiment::RefRecoveryMatrix: return "ref_recovery_matrix";
    case Experiment::RefDiagnostics: return "ref_diagnostics";
    case Experiment::RefSideband: return "ref_sideband";
    case Experiment::RefL1: return "ref_l1";
    case Experiment::RefThroughputTradeoff: return "ref_throughput_tradeoff";
    case Experiment::RefCrossClient: return "ref_cross_client";
    case Experiment::FhCleanSuccess: return "fh_clean_success";
    case Experiment::FhLossBeforeAccept: return "fh_loss_before_accept";
    case Experiment::FhCommitThenLocalSlverr: return "fh_commit_then_local_slverr";
    case Experiment::FhNoCommitLocalSlverr: return "fh_no_commit_local_slverr";
    case Experiment::FhWatchdogThenLateOkay: return "fh_watchdog_then_late_okay";
    case Experiment::FhTimeoutThenRemoteCommit: return "fh_timeout_then_remote_commit";
    case Experiment::FhBlindRetryAfterSlverr: return "fh_blind_retry_after_slverr";
    case Experiment::FhNoRetryAfterSlverr: return "fh_no_retry_after_slverr";
    case Experiment::FhAxiIdReuseLateResp: return "fh_axi_id_reuse_late_resp";
    case Experiment::FhMultiOutstandingBackpressure: return "fh_multi_outstanding_backpressure";
    case Experiment::FhReadBurstErrorCompletion: return "fh_read_burst_error_completion";
    case Experiment::FhRaceRespBeforeTimeout: return "fh_race_resp_before_timeout";
    case Experiment::FhRaceRespSimultaneous: return "fh_race_resp_simultaneous";
    case Experiment::FhRaceRespAfterTimeout: return "fh_race_resp_after_timeout";
    case Experiment::FhClassifierDiagCompare: return "fh_classifier_diag_compare";
    case Experiment::FhDiagAckLossAfterDelivery: return "fh_diag_ack_loss_after_delivery";
    case Experiment::FhObserverHistoryCompare: return "fh_observer_history_compare";
    case Experiment::FhSafeRetryCancelRemote: return "fh_safe_retry_cancel_remote";
    case Experiment::FhCancelLatencyRace: return "fh_cancel_latency_race";
    case Experiment::AxCleanPacketImm: return "ax_clean_packet_imm";
    case Experiment::AxCleanPacketPkt: return "ax_clean_packet_pkt";
    case Experiment::AxLossMidPacketImm: return "ax_loss_mid_packet_imm";
    case Experiment::AxLossMidPacketPkt: return "ax_loss_mid_packet_pkt";
    case Experiment::AxLossBeforeFirstImm: return "ax_loss_before_first_imm";
    case Experiment::AxLossBeforeFirstPkt: return "ax_loss_before_first_pkt";
    case Experiment::AxTlastLocalRemoteLoss: return "ax_tlast_local_remote_loss";
    case Experiment::AxImmPartialThenBlindRetry: return "ax_imm_partial_then_blind_retry";
    case Experiment::AxPktCommitThenLocalTimeout: return "ax_pkt_commit_then_local_timeout";
    case Experiment::AxAckLossAfterDelivery: return "ax_ack_loss_after_delivery";
    case Experiment::AxObserverTimedVsSeq: return "ax_observer_timed_vs_seq";
    case Experiment::AxSafeRetryCancel0: return "ax_safe_retry_cancel0";
    case Experiment::AxCancelLatencyRace: return "ax_cancel_latency_race";
    case Experiment::AxBackpressureTready: return "ax_backpressure_tready";
    case Experiment::AxContinuousTidStable: return "ax_continuous_tid_stable";
    case Experiment::AxAppackLoss: return "ax_appack_loss";
    case Experiment::AxResetPartialPacket: return "ax_reset_partial_packet";
    case Experiment::AxLateDataAfterRestart: return "ax_late_data_after_restart";
    case Experiment::AxMultiPacketInflight: return "ax_multi_packet_inflight";
    case Experiment::AxPacketLenVariants: return "ax_packet_len_variants";
    case Experiment::AxReplayNoDupEffect: return "ax_replay_no_dup_effect";
    case Experiment::AxAppLevelRepeatDup: return "ax_app_level_repeat_dup";
    case Experiment::AxDupSuppress: return "ax_dup_suppress";
    case Experiment::AxPacketIdReuse: return "ax_packet_id_reuse";
    case Experiment::A5AtomicStoreAddOk: return "a5_atomic_store_add_ok";
    case Experiment::A5AtomicCommitThenLocalSlverr: return "a5_atomic_commit_then_local_slverr";
    case Experiment::A5AtomicBlindRetry: return "a5_atomic_blind_retry";
    case Experiment::A5AtomicNeverHanded: return "a5_atomic_never_handed";
    case Experiment::A5AtomicAckLoss: return "a5_atomic_ack_loss";
    case Experiment::A5AtomicLossBeforeExec: return "a5_atomic_loss_before_exec";
    case Experiment::A5AtomicTimeoutAfterCommit: return "a5_atomic_timeout_after_commit";
    case Experiment::A5AtomicLateResponse: return "a5_atomic_late_response";
    case Experiment::A5AtomicConcurrent: return "a5_atomic_concurrent";
    case Experiment::A5AtomicDupSuppress: return "a5_atomic_dup_suppress";
    case Experiment::A5AtomicResetRestart: return "a5_atomic_reset_restart";
    case Experiment::A5PoisonRejectNoCommit: return "a5_poison_reject_no_commit";
    case Experiment::A5PoisonRejectOkay: return "a5_poison_reject_okay";
    case Experiment::A5PoisonWithSlverrIndependent: return "a5_poison_with_slverr_independent";
    case Experiment::A5PoisonAfterCommitNoUndo: return "a5_poison_after_commit_no_undo";
    case Experiment::A5PoisonAcceptMarked: return "a5_poison_accept_marked";
    case Experiment::A5PoisonPartialThenTimeout: return "a5_poison_partial_then_timeout";
    case Experiment::A5TraceNotInClassifier: return "a5_trace_not_in_classifier";
    case Experiment::A5SafeRetryCancel0: return "a5_safe_retry_cancel0";
    case Experiment::A5ExclusiveSmokeOutOfScope: return "a5_exclusive_smoke_out_of_scope";
  }
  return "?";
}

inline std::string_view to_string(WaitCause c) {
  switch (c) {
    case WaitCause::None: return "NONE";
    case WaitCause::Arbitration: return "WAIT_ARBITRATION";
    case WaitCause::GuestANormalTraffic: return "WAIT_GUEST_A_NORMAL_TRAFFIC";
    case WaitCause::GuestAReplay: return "WAIT_GUEST_A_REPLAY";
    case WaitCause::OwnReplay: return "WAIT_OWN_REPLAY";
    case WaitCause::Credit: return "WAIT_CREDIT";
    case WaitCause::Target: return "WAIT_TARGET";
    case WaitCause::Response: return "WAIT_RESPONSE";
    case WaitCause::Recovery: return "WAIT_RECOVERY";
  }
  return "?";
}

inline bool experiment_from_name(std::string_view name, Experiment& out) {
  const Experiment all[] = {
      Experiment::BaselineUnknown, Experiment::PrecommitFailure, Experiment::BlindRetry,
      Experiment::ObservationalEquivalence, Experiment::MultiGuest, Experiment::RetryStorm,
      Experiment::GuestAStorm, Experiment::SharedQuiet, Experiment::GuestBIsolated,
      Experiment::GuestBStorm, Experiment::MitSafetyPriority, Experiment::MitPerGuestSlots,
      Experiment::MitReplayBudget, Experiment::MitReservedBw, Experiment::MitVirtualChannels,
      Experiment::PrioritySideband, Experiment::FastThrottle, Experiment::EmergencyShutdown,
      Experiment::DegradedLink, Experiment::RandomCampaign, Experiment::RefRetryBuffer,
      Experiment::RefFifoOccupancy, Experiment::RefCreditBehavior, Experiment::RefSustainedReplay,
      Experiment::RefCommitVsAck, Experiment::RefRecoveryMatrix, Experiment::RefDiagnostics,
      Experiment::RefSideband, Experiment::RefL1, Experiment::RefThroughputTradeoff,
      Experiment::RefCrossClient, Experiment::FhCleanSuccess, Experiment::FhLossBeforeAccept,
      Experiment::FhCommitThenLocalSlverr, Experiment::FhNoCommitLocalSlverr,
      Experiment::FhWatchdogThenLateOkay, Experiment::FhTimeoutThenRemoteCommit,
      Experiment::FhBlindRetryAfterSlverr, Experiment::FhNoRetryAfterSlverr,
      Experiment::FhAxiIdReuseLateResp, Experiment::FhMultiOutstandingBackpressure,
      Experiment::FhReadBurstErrorCompletion, Experiment::FhRaceRespBeforeTimeout,
      Experiment::FhRaceRespSimultaneous, Experiment::FhRaceRespAfterTimeout,
      Experiment::FhClassifierDiagCompare, Experiment::FhDiagAckLossAfterDelivery,
      Experiment::FhObserverHistoryCompare, Experiment::FhSafeRetryCancelRemote,
      Experiment::FhCancelLatencyRace, Experiment::AxCleanPacketImm,
      Experiment::AxCleanPacketPkt, Experiment::AxLossMidPacketImm,
      Experiment::AxLossMidPacketPkt, Experiment::AxLossBeforeFirstImm,
      Experiment::AxLossBeforeFirstPkt, Experiment::AxTlastLocalRemoteLoss,
      Experiment::AxImmPartialThenBlindRetry, Experiment::AxPktCommitThenLocalTimeout,
      Experiment::AxAckLossAfterDelivery, Experiment::AxObserverTimedVsSeq,
      Experiment::AxSafeRetryCancel0, Experiment::AxCancelLatencyRace,
      Experiment::AxBackpressureTready, Experiment::AxContinuousTidStable,
      Experiment::AxAppackLoss, Experiment::AxResetPartialPacket,
      Experiment::AxLateDataAfterRestart, Experiment::AxMultiPacketInflight,
      Experiment::AxPacketLenVariants, Experiment::AxReplayNoDupEffect,
      Experiment::AxAppLevelRepeatDup, Experiment::AxDupSuppress,
      Experiment::AxPacketIdReuse, Experiment::A5AtomicStoreAddOk,
      Experiment::A5AtomicCommitThenLocalSlverr, Experiment::A5AtomicBlindRetry,
      Experiment::A5AtomicNeverHanded, Experiment::A5AtomicAckLoss,
      Experiment::A5AtomicLossBeforeExec, Experiment::A5AtomicTimeoutAfterCommit,
      Experiment::A5AtomicLateResponse, Experiment::A5AtomicConcurrent,
      Experiment::A5AtomicDupSuppress, Experiment::A5AtomicResetRestart,
      Experiment::A5PoisonRejectNoCommit, Experiment::A5PoisonRejectOkay,
      Experiment::A5PoisonWithSlverrIndependent, Experiment::A5PoisonAfterCommitNoUndo,
      Experiment::A5PoisonAcceptMarked, Experiment::A5PoisonPartialThenTimeout,
      Experiment::A5TraceNotInClassifier, Experiment::A5SafeRetryCancel0,
      Experiment::A5ExclusiveSmokeOutOfScope};
  for (Experiment e : all) {
    if (name == to_string(e)) {
      out = e;
      return true;
    }
  }
  // Legacy alias kept for older scripts/docs.
  if (name == "fh_race_resp_timeout_linkerror") {
    out = Experiment::FhRaceRespAfterTimeout;
    return true;
  }
  // Matrix aliases from Phase-1 docs (sink chosen via experiment id).
  if (name == "ax_clean_packet") {
    out = Experiment::AxCleanPacketImm;
    return true;
  }
  if (name == "ax_loss_mid_packet") {
    out = Experiment::AxLossMidPacketImm;
    return true;
  }
  if (name == "ax_loss_before_first") {
    out = Experiment::AxLossBeforeFirstImm;
    return true;
  }
  return false;
}

}  // namespace ucie
