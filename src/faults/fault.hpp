#pragma once

#include "common/enums.hpp"

#include <cstdint>
#include <string>

namespace ucie {

enum class RecoveryCmd : std::uint8_t {
  None,
  Retrain,
  LinkReset,
  LinkError,
  EmergencyShutdown,
  RemoteUnresponsive,
  EnterL1,
  ExitL1,
};

struct FaultSpec {
  int issue_index = 0;  // -1 matches every transaction
  Trigger trigger = Trigger::Phase;
  Phase phase = Phase::ResponseTransmitting;
  FlitRole role = FlitRole::Response;
  FaultKind kind = FaultKind::RepeatedCorruption;
  double at_ns = 0;
  int retry_at = 3;
  int corrupt_bits = 1;
  bool fail_retrain = false;
  bool persistent = false;
  int storm_naks = 0;
  // -1 corrupts every guest. A non-negative value corrupts only that guest.
  int guest = -1;
  double extra_delay_ns = 0;
  double throttle_factor = 4;
  double bandwidth_factor = 0.25;
  bool fired = false;
};

struct CorruptionArm {
  bool persistent = false;
  int oneshot = 0;
  int bits = 1;
  FlitRole role = FlitRole::Response;
  std::uint64_t logical_id = 0;
  bool any_id = false;
  bool drop_one = false;
  bool duplicate_one = false;
  bool suppress_ack = false;
  double extra_delay_ns = 0;
  int storm_naks = 0;
  int storm_guest = -1;
};

}  // namespace ucie
