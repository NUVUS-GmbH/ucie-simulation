#pragma once

#include "enums.hpp"

#include <vector>

namespace ucie {

// Selects one ready client. FIFO expects ready[] already in arrival order
// and returns index 0. Other policies use priority[] and weight[].
class Arbiter {
 public:
  explicit Arbiter(ArbPolicy policy = ArbPolicy::RoundRobin) : policy_(policy) {}

  void set_policy(ArbPolicy policy) { policy_ = policy; }

  // Returns an index into ready[], or -1 if ready is empty.
  int select(const std::vector<int>& ready, const std::vector<int>& priority,
             const std::vector<int>& weight);

 private:
  ArbPolicy policy_;
  int rr_ = 0;
  int wrr_remaining_ = 0;
  int wrr_current_ = -1;
};

}  // namespace ucie
