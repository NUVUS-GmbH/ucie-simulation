#include "credit_manager.hpp"

namespace ucie {

CreditManager::CreditManager(int depth) : depth_(depth), available_(depth) {}

bool CreditManager::try_acquire() {
  if (available_ <= 0) return false;
  --available_;
  ++held_;
  return true;
}

void CreditManager::on_return() {
  if (held_ <= 0) {
    // A return with nothing held creates a credit. Audit reports it
    // unless an injected fault already owns the mismatch.
    ++available_;
    return;
  }
  --held_;
  ++available_;
}

void CreditManager::reinitialize() {
  available_ = depth_;
  held_ = 0;
}

void CreditManager::inject_create() {
  ++available_;
  fault_injected_ = true;
}

void CreditManager::inject_lose() {
  if (available_ > 0) --available_;
  fault_injected_ = true;
}

std::vector<std::string> CreditManager::audit() const {
  std::vector<std::string> problems;
  if (fault_injected_) return problems;
  if (available_ + held_ != depth_) {
    problems.push_back("credit conservation broken: available " + std::to_string(available_) +
                       " + held " + std::to_string(held_) + " != depth " + std::to_string(depth_));
  }
  if (available_ < 0 || held_ < 0) {
    problems.push_back("credit count went negative");
  }
  return problems;
}

}  // namespace ucie
