#include "arbitration.hpp"

namespace ucie {

int Arbiter::select(const std::vector<int>& ready, const std::vector<int>& priority,
                    const std::vector<int>& weight) {
  if (ready.empty()) return -1;
  auto prio = [&](int guest) {
    return (guest >= 0 && guest < static_cast<int>(priority.size())) ? priority[guest] : 0;
  };
  auto w = [&](int guest) {
    int value = (guest >= 0 && guest < static_cast<int>(weight.size())) ? weight[guest] : 1;
    return value < 1 ? 1 : value;
  };

  if (policy_ == ArbPolicy::Fifo) return 0;

  if (policy_ == ArbPolicy::StrictPriority) {
    int best = 0;
    for (int i = 1; i < static_cast<int>(ready.size()); ++i) {
      if (prio(ready[i]) > prio(ready[best])) best = i;
    }
    return best;
  }

  if (policy_ == ArbPolicy::WeightedRoundRobin) {
    for (int guard = 0; guard < static_cast<int>(ready.size()) + 1; ++guard) {
      if (wrr_remaining_ <= 0 || wrr_current_ < 0) {
        wrr_current_ = ready[static_cast<std::size_t>(rr_ % static_cast<int>(ready.size()))];
        rr_ = (rr_ + 1) % static_cast<int>(ready.size());
        wrr_remaining_ = w(wrr_current_);
      }
      for (int i = 0; i < static_cast<int>(ready.size()); ++i) {
        if (ready[i] == wrr_current_ && wrr_remaining_ > 0) {
          --wrr_remaining_;
          return i;
        }
      }
      wrr_remaining_ = 0;
    }
    return 0;
  }

  // Round-robin over the ready set, remembered across calls.
  int start = rr_;
  int pick = start % static_cast<int>(ready.size());
  rr_ = pick + 1;
  return pick;
}

}  // namespace ucie
