#pragma once

// [ASSUMPTION] One credit stands for one AoU message (request, write data,
// or response), not for a granule. The AoU 0.8 text is not in this workspace.
// The public §8.3.3 citation used by the transaction-level study is about
// activation, not credit granularity. Conservation is checked on this pool.
//
// Reinitialization at the next activation is an epoch boundary. It is not
// treated as a credit leak. An injected credit fault is recorded separately.

#include <string>
#include <vector>

namespace ucie {

class CreditManager {
 public:
  CreditManager() = default;
  explicit CreditManager(int depth);
  void configure(int depth) {
    depth_ = depth;
    available_ = depth;
    held_ = 0;
  }

  int depth() const { return depth_; }
  int available() const { return available_; }
  int held() const { return held_; }
  bool fault_injected() const { return fault_injected_; }

  bool try_acquire();
  void on_return();
  void reinitialize();
  // Injected faults. These are the only supported ways to break the sum.
  void inject_create();
  void inject_lose();

  std::vector<std::string> audit() const;

 private:
  int depth_ = 4;
  int available_ = 4;
  int held_ = 0;
  bool fault_injected_ = false;
};

}  // namespace ucie
