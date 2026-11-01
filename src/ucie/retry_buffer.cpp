#include "retry_buffer.hpp"

namespace ucie {

RetryBuffer::RetryBuffer(std::size_t capacity) : capacity_(capacity < 1 ? 1 : capacity) {}

bool RetryBuffer::push(const Flit& flit) {
  if (entries_.size() >= capacity_) return false;
  entries_.push_back(Entry{flit});
  if (entries_.size() > max_occupancy_) max_occupancy_ = entries_.size();
  return true;
}

bool RetryBuffer::ack(std::uint8_t seq) {
  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    if (it->flit.seq == seq) {
      entries_.erase(it);
      return true;
    }
  }
  return false;
}

std::optional<Flit> RetryBuffer::copy_seq(std::uint8_t seq) const {
  for (const auto& entry : entries_) {
    if (entry.flit.seq == seq) return entry.flit;
  }
  return std::nullopt;
}

bool RetryBuffer::mark_nak(std::uint8_t seq) {
  for (auto& entry : entries_) {
    if (entry.flit.seq == seq) {
      entry.flit.nak_pending = true;
      return true;
    }
  }
  return false;
}

std::optional<Flit> RetryBuffer::take_nak() {
  for (auto& entry : entries_) {
    if (entry.flit.nak_pending) {
      entry.flit.nak_pending = false;
      entry.flit.deadline_ns = -1;
      return entry.flit;
    }
  }
  return std::nullopt;
}

std::vector<Flit> RetryBuffer::take_nak_replay() {
  std::vector<Flit> out;
  bool from_nak = false;
  for (auto& entry : entries_) {
    if (entry.flit.nak_pending) from_nak = true;
    if (!from_nak) continue;
    entry.flit.nak_pending = false;
    entry.flit.deadline_ns = -1;
    out.push_back(entry.flit);
  }
  return out;
}

std::optional<Flit> RetryBuffer::take_expired(double now_ns) {
  for (auto& entry : entries_) {
    if (entry.flit.deadline_ns >= 0 && entry.flit.deadline_ns <= now_ns && !entry.flit.nak_pending) {
      entry.flit.deadline_ns = -1;
      return entry.flit;
    }
  }
  return std::nullopt;
}

void RetryBuffer::set_deadline(std::uint8_t seq, double deadline_ns) {
  for (auto& entry : entries_) {
    if (entry.flit.seq == seq) entry.flit.deadline_ns = deadline_ns;
  }
}

std::vector<Flit> RetryBuffer::snapshot() const {
  std::vector<Flit> out;
  out.reserve(entries_.size());
  for (const auto& entry : entries_) out.push_back(entry.flit);
  return out;
}

void RetryBuffer::flush() { entries_.clear(); }

}  // namespace ucie
