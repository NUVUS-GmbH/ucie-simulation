#pragma once

// Device models. The architectural commit point is the call to commit() /
// axis_commit_*(), which is a different step from AXI / AXIS handshakes.
// [ASSUMPTION] A write commits only after the request and the write data have
// both been delivered. AoU and UCIe do not define device commit.
// AXIS sinks are MODEL PROFILE targets (SINK-IMM / SINK-PKT), not AXI-Stream
// protocol norms. SINK-PKT separates packet reception, processing start, and
// atomic effects — complete reception alone does not imply an effect.

#include "common/transaction.hpp"

#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace ucie {

struct AxisDupEntry {
  std::uint64_t packet_op_id = 0;
  double committed_ns = -1;
};

class TargetModel {
 public:
  int counter = 0;
  int actuator = 0;
  std::vector<int> fifo;
  int dma_starts = 0;
  int accelerator_starts = 0;
  int doorbell_rings = 0;
  int interrupts_acked = 0;
  std::map<std::uint64_t, int> memory;
  // Stream architectural byte stream (evaluation / GT).
  std::vector<std::uint8_t> axis_bytes;
  int axis_packets_committed = 0;

  // AXI5-MM-P1 AtomicStore location (evaluation GT). Separate from `counter`
  // so MM baseline IncrementCounter results remain comparable.
  std::uint64_t atomic_address = 0xA000;
  int atomic_value = 0;
  int atomic_ops_applied = 0;  // successful ADD applications (≠ commit_count alone)
  std::map<std::uint64_t, bool> poisoned_locations;
  PoisonTargetPolicy poison_policy = PoisonTargetPolicy::RejectWithoutCommitSlverr;
  // Monotonic: never decreases (Poison must not undo a prior commit).
  int axi5_commit_watermark = 0;

  // Optional duplicate suppression by bridge token (MODEL ASSUMPTION).
  std::map<std::uint64_t, int> committed_tokens;
  bool duplicate_suppress = false;
  int duplicates_suppressed = 0;

  // Optional AXIS packet_op_id duplicate suppression (ADDITIONAL MECHANISM).
  bool axis_dup_suppress = false;
  int axis_dup_capacity = 16;
  double axis_dup_lifetime_ns = 0;
  bool axis_dup_clear_on_reset = true;
  std::deque<AxisDupEntry> axis_dup_table;
  int axis_duplicates_suppressed = 0;

  std::string commit(Transaction& tx);
  // AtomicStore(ADD) path: updates atomic_value; never returns prior value on R.
  std::string commit_atomic_store(Transaction& tx);

  // SINK-IMM: one atomic effect for a remotely accepted transfer's valid bytes.
  std::string axis_commit_imm_beat(Transaction& tx, int beat_index, double now_ns);
  // SINK-PKT: one atomic packet effect after processing has started.
  std::string axis_commit_pkt_effect(Transaction& tx, double now_ns);

  bool axis_dup_blocks(std::uint64_t packet_op_id, double now_ns);
  void axis_dup_remember(std::uint64_t packet_op_id, double now_ns);
  void axis_dup_on_reset();
};

}  // namespace ucie
