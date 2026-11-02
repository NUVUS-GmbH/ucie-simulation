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
  std::uint32_t axi_id = 0;
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
  // [MODEL ASSUMPTION] Bridge token/generation for late-response matching.
  // Not claimed as an AXI or UCIe architectural field.
  std::uint64_t bridge_token = 0;
  std::uint64_t bridge_generation = 0;
  AxiResp resp = AxiResp::Okay;
  int beat_index = 0;
  bool rlast = false;
  // AXIS-P1 segment fields (MODEL PROFILE Bridge-AXIS-P1: one segment/transfer).
  // Not used for MM flits. AoU packing of these fields is a bridge policy, not
  // an AoU-spec claim (AoU full text is a documented source gap).
  bool axis_segment = false;
  bool tlast = false;
  std::uint16_t tkeep = 0xffff;
  std::uint8_t tid = 0;
  std::uint8_t tdest = 0;
  std::uint8_t tuser = 0;
  std::uint64_t tdata = 0;
  int valid_bytes = 16;
  std::uint64_t packet_op_id = 0;
  bool app_ack = false;  // optional separate return; not AXI BRESP
  // AXI5-MM-P1 sideband carried on the modeled UCIe payload (MODEL PROFILE
  // bridge packing — not an AoU-conformity claim).
  std::uint8_t awatop = 0;
  bool wpoison = false;
  bool awtrace = false;
  bool wtrace = false;
  bool btrace = false;
  int operand = 0;
};

}  // namespace ucie
