#pragma once

// Device models. The architectural commit point is the call to commit(),
// which is a different step from AXI completion.
// [ASSUMPTION] A write commits only after the request and the write data have
// both been delivered. AoU and UCIe do not define device commit.

#include "common/transaction.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ucie {

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

  std::string commit(Transaction& tx);
};

}  // namespace ucie
