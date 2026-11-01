#pragma once

// Bounded queue. push() blocks when full (backpressure). flush() wakes every
// waiter and makes the in-flight push/pop return false.
// [MODEL] Occupancy is protocol buffering, not a PHY elastic buffer.

#include <systemc>

#include <cstddef>
#include <deque>
#include <utility>

namespace ucie {

template <typename T>
class TimedQueue {
 public:
  TimedQueue() = default;
  explicit TimedQueue(std::size_t capacity) : capacity_(capacity < 1 ? 1 : capacity) {}

  void configure(std::size_t capacity) { capacity_ = capacity < 1 ? 1 : capacity; }

  TimedQueue(const TimedQueue&) = delete;
  TimedQueue& operator=(const TimedQueue&) = delete;

  std::size_t size() const { return q_.size(); }
  std::size_t capacity() const { return capacity_; }
  std::size_t max_occupancy() const { return max_; }
  std::uint64_t blocked_pushes() const { return blocked_; }
  sc_core::sc_time blocked_time() const { return blocked_time_; }
  sc_core::sc_event& data_event() { return can_pop_; }
  bool over_capacity() const { return max_ > capacity_; }

  bool try_pop(T& out) {
    if (q_.empty()) return false;
    out = std::move(q_.front());
    q_.pop_front();
    can_push_.notify(sc_core::SC_ZERO_TIME);
    return true;
  }

  bool try_peek(T& out) const {
    if (q_.empty()) return false;
    out = q_.front();
    return true;
  }

  // Pop the first element that satisfies pred, preserving relative order of the rest.
  template <typename Pred>
  bool try_pop_if(T& out, Pred pred) {
    for (auto it = q_.begin(); it != q_.end(); ++it) {
      if (pred(*it)) {
        out = std::move(*it);
        q_.erase(it);
        can_push_.notify(sc_core::SC_ZERO_TIME);
        return true;
      }
    }
    return false;
  }

  bool pop(T& out) {
    const auto gen = epoch_;
    while (q_.empty()) {
      sc_core::wait(can_pop_ | flushed_);
      if (epoch_ != gen) return false;
    }
    if (epoch_ != gen) return false;
    out = std::move(q_.front());
    q_.pop_front();
    can_push_.notify(sc_core::SC_ZERO_TIME);
    return true;
  }

  bool push(T item) {
    const auto gen = epoch_;
    const auto start = sc_core::sc_time_stamp();
    while (q_.size() >= capacity_) {
      ++blocked_;
      sc_core::wait(can_push_ | flushed_);
      if (epoch_ != gen) return false;
    }
    blocked_time_ += sc_core::sc_time_stamp() - start;
    if (epoch_ != gen) return false;
    q_.push_back(std::move(item));
    if (q_.size() > max_) max_ = q_.size();
    can_pop_.notify(sc_core::SC_ZERO_TIME);
    return true;
  }

  void flush() {
    q_.clear();
    ++epoch_;
    flushed_.notify(sc_core::SC_ZERO_TIME);
    can_push_.notify(sc_core::SC_ZERO_TIME);
    can_pop_.notify(sc_core::SC_ZERO_TIME);
  }

 private:
  std::size_t capacity_ = 1;
  std::deque<T> q_;
  std::size_t max_ = 0;
  std::uint64_t blocked_ = 0;
  std::uint64_t epoch_ = 1;
  sc_core::sc_time blocked_time_{sc_core::SC_ZERO_TIME};
  sc_core::sc_event can_pop_;
  sc_core::sc_event can_push_;
  sc_core::sc_event flushed_;
};

}  // namespace ucie
