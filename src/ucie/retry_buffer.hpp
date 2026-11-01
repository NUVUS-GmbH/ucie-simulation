#pragma once

// [SPEC] UCIe 2.0 §3.8 retry buffer holds transmitted flits until they are
// acknowledged. There is no Rx retry buffer in the UCIe deltas to PCIe.
// [SPEC] Flush on LinkReset and on LinkError cleanup is a stated rule
// (UCIe 2.0 §10.3.3.5, §10.3.3.7, retained by the 3.0 study notes).
// [POLICY] Whether payload bytes survive Retrain is not stated as a flush
// rule. retrain_preserves_retry selects that policy.

#include "flit.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace ucie {

class RetryBuffer {
 public:
  RetryBuffer() = default;
  explicit RetryBuffer(std::size_t capacity);
  void configure(std::size_t capacity) { capacity_ = capacity < 1 ? 1 : capacity; }

  std::size_t capacity() const { return capacity_; }
  std::size_t size() const { return entries_.size(); }
  std::size_t max_occupancy() const { return max_occupancy_; }
  bool empty() const { return entries_.empty(); }

  // False when the buffer is at its configured capacity.
  bool push(const Flit& flit);
  bool ack(std::uint8_t seq);
  bool mark_nak(std::uint8_t seq);
  std::optional<Flit> copy_seq(std::uint8_t seq) const;
  std::optional<Flit> take_nak();
  // Go-back-N: return the NAK'd flit and every later unacked flit in order.
  std::vector<Flit> take_nak_replay();
  std::optional<Flit> take_expired(double now_ns);
  void set_deadline(std::uint8_t seq, double deadline_ns);
  std::vector<Flit> snapshot() const;
  void flush();

 private:
  struct Entry {
    Flit flit;
  };
  std::size_t capacity_ = 1;
  std::size_t max_occupancy_ = 0;
  std::vector<Entry> entries_;
};

}  // namespace ucie
