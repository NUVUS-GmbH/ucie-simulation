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

enum class Fate : std::uint8_t { Completed, NotCompleted, Unknown };

// What the initiator is allowed to read. Ground-truth commit count is never
// an input to these profiles.
enum class ObserverProfile : std::uint8_t {
  A_AxiOnly,
  B_AxiFdi,
  C_AxiFdiUcieStatus,
  D_AxiHypervisorMetadata,
};

// [POLICY] UCIe does not define the AXI completion that follows LinkError.
enum class RecoveryPolicy : std::uint8_t {
  Discard,          // A: no AXI completion
  PreserveLocal,    // B: deliver a response that already reached the local AoU
  ExplicitUnknown,  // C: a distinct UNKNOWN, not OKAY and not a retry permit
  AutoRetry,        // D: reissue the same logical transaction after the link is up
  NoAutoRetry,      // E: same initiator-visible result as Discard in this model
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
      Experiment::RefCrossClient};
  for (Experiment e : all) {
    if (name == to_string(e)) {
      out = e;
      return true;
    }
  }
  return false;
}

}  // namespace ucie
