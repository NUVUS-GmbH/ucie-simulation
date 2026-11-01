#pragma once

#include "common/enums.hpp"

#include <cstdint>

namespace ucie {

// One UCIe payload or control flit in the timed model.
// [MODEL] This is not a bit-accurate 256B flit. CRC is the detection
// guarantee, not an execution of the specification LFSR.
struct Flit {
  std::uint64_t logical_id = 0;
  std::uint32_t slot = 0;
  std::uint32_t guest_id = 0;
  FlitRole role = FlitRole::Request;
  std::uint8_t seq = 0;
  // When >= 0, sequence checks use the per-guest VC space for this guest.
  int vc = -1;
  bool crc_ok = true;
  int corrupt_bits = 0;
  bool replay = false;
  bool drop = false;
  bool duplicate = false;
  double extra_delay_ns = 0;
  std::uint64_t link_epoch = 0;
  double deadline_ns = -1;
  bool nak_pending = false;
};

}  // namespace ucie
