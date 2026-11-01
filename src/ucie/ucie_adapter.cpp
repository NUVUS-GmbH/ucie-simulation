#include "sim/modules.hpp"

#include <sstream>

namespace ucie {
namespace {

void note_nak(Transaction& tx, FlitRole role) {
  if (role == FlitRole::Request && !tx.visible.request_ack) tx.visible.request_nak_only = true;
  if (role == FlitRole::WriteData && !tx.visible.data_ack) tx.visible.data_nak_only = true;
  if (role == FlitRole::Response && !tx.visible.response_ack) tx.visible.response_nak_only = true;
}

void note_ack(Transaction& tx, FlitRole role) {
  if (role == FlitRole::Request) {
    tx.visible.request_ack = true;
    tx.visible.request_nak_only = false;
  } else if (role == FlitRole::WriteData) {
    tx.visible.data_ack = true;
    tx.visible.data_nak_only = false;
  } else if (role == FlitRole::Response) {
    tx.visible.response_ack = true;
    tx.visible.response_nak_only = false;
  }
}

void note_timeout(Transaction& tx, FlitRole role) {
  if (role == FlitRole::Request) tx.visible.request_timeout = true;
  if (role == FlitRole::WriteData) tx.visible.data_timeout = true;
  if (role == FlitRole::Response) tx.visible.response_timeout = true;
}

WaitCause block_cause_for_guest_b(const World& w, int side) {
  if (w.link.destructive || w.link.state != LinkState::Active) return WaitCause::Recovery;
  if (w.resources.tx_slot_owner == 0) {
    return w.resources.tx_slot_is_replay ? WaitCause::GuestAReplay : WaitCause::GuestANormalTraffic;
  }
  if (w.resources.tx_slot_owner == 1) return WaitCause::OwnReplay;
  if (!w.retry[side].empty()) {
    // Fallback when owner tracking is momentarily unset.
    return WaitCause::GuestANormalTraffic;
  }
  return WaitCause::None;
}

}  // namespace

UcieAdapter::UcieAdapter(sc_core::sc_module_name name, World& world, int side)
    : sc_module(name), w_(world), side_(side) {
  SC_THREAD(tx);
  SC_THREAD(rx);
  SC_THREAD(monitor);
}

void UcieAdapter::tx() {
  while (true) {
    while (!w_.mainband_active()) sc_core::wait(w_.link.state_ev);
    const auto epoch = w_.link.epoch;
    Flit flit;
    bool from_ctrl = false;
    if (w_.ctrl_q[side_].try_pop(flit)) {
      from_ctrl = true;
    } else if (w_.replay_q[side_].try_pop(flit)) {
      flit.replay = true;
      // Replay-budget / reserved-bandwidth yield: if Guest B has a flit that
      // is allowed to use a TX slot now, defer this Guest A replay.
      if (side_ == 0 && flit.guest_id == 0 && w_.prefer_safety_now()) {
        Flit safety;
        if (w_.send_q[side_].try_pop_if(safety, [&](const Flit& f) {
              return f.guest_id == 1 && !w_.tx_slot_blocks(side_, f);
            })) {
          w_.resources.yield_events += 1;
          w_.log(TraceLevel::Transactions, "replay_yield", "UCIe", w_.find(flit.logical_id),
                 "Guest A replay deferred so Guest B can use the shared TX slot");
          w_.replay_q[side_].push(flit);
          flit = safety;
          flit.replay = false;
          if (w_.scenario.mitigation.reserved_safety_bandwidth) {
            w_.resources.reserved_slots_granted += 1;
            w_.reserved_countdown = 4;
          }
        }
      }
    } else {
      // Prefer a safety-critical Guest B flit when a mitigation asks for it.
      bool took = false;
      if (side_ == 0 && w_.prefer_safety_now()) {
        took = w_.send_q[side_].try_pop_if(flit, [](const Flit& f) { return f.guest_id == 1; });
        if (took) {
          flit.replay = false;
          if (w_.scenario.mitigation.reserved_safety_bandwidth) {
            w_.resources.reserved_slots_granted += 1;
            w_.reserved_countdown = 4;
          }
        }
      }
      if (!took) {
        Flit peek;
        if (w_.send_q[side_].try_peek(peek) && w_.tx_slot_blocks(side_, peek)) {
          const double wait_start = w_.now_ns();
          const WaitCause cause = (peek.guest_id == 1) ? block_cause_for_guest_b(w_, side_) : WaitCause::None;
          sc_core::wait(w_.ack_ev[side_] | w_.replay_q[side_].data_event() | w_.ctrl_q[side_].data_event() |
                        w_.link.state_ev | w_.link.flushed);
          if (cause != WaitCause::None) w_.attribute_guest_b_wait(cause, wait_start, w_.now_ns());
          continue;
        }
        if (w_.send_q[side_].try_pop(flit)) {
          flit.replay = false;
          if (side_ == 0 && w_.reserved_countdown > 0) --w_.reserved_countdown;
        } else {
          sc_core::wait(w_.ctrl_q[side_].data_event() | w_.replay_q[side_].data_event() |
                        w_.send_q[side_].data_event() | w_.link.state_ev);
          continue;
        }
      }
    }
    if (!w_.mainband_active() || w_.link.epoch != epoch) continue;

    if (from_ctrl) {
      if (w_.arm.suppress_ack && flit.role == FlitRole::Ack) {
        w_.arm.suppress_ack = false;
        w_.log(TraceLevel::Transactions, "ack_loss", "UCIe", w_.find(flit.logical_id),
               "[MODEL] ACK withheld so the replay timer can expire");
        continue;
      }
      flit.link_epoch = w_.link.wire_gen;
      flit.crc_ok = true;
      sc_core::wait(w_.cycles(1));
      if (w_.link.wire_gen != flit.link_epoch) continue;
      w_.wire[side_].push(flit);
      continue;
    }

    if (!flit.replay) {
      if (flit.slot >= w_.txns.size()) continue;
      auto& tx = w_.slot(flit.slot);
      if (tx.visible.classified) continue;
      if (!w_.credit[side_].try_acquire()) {
        w_.note(tx, Phase::WaitingForCredit, "AoU", "waiting for an AoU credit");
        const double credit_start = w_.now_ns();
        bool got = false;
        while (!(got = w_.credit[side_].try_acquire())) {
          sc_core::wait(w_.credit_ev[side_] | w_.link.flushed | w_.link.state_ev);
          if (w_.link.epoch != epoch || tx.visible.classified || !w_.mainband_active()) break;
        }
        if (tx.guest_id == 1) w_.attribute_guest_b_wait(WaitCause::Credit, credit_start, w_.now_ns());
        if (!got) continue;
      }
      ++w_.credits_consumed[side_];
      if (w_.link.epoch != epoch || tx.visible.classified || !w_.mainband_active()) continue;
      while (static_cast<int>(w_.retry[side_].size()) >= w_.max_unacked()) {
        w_.log(TraceLevel::Full, "backpressure", "UCIe", &tx, "retry buffer is at the unacked cap");
        const double bp_start = w_.now_ns();
        sc_core::wait(w_.ack_ev[side_] | w_.link.flushed);
        if (tx.guest_id == 1) w_.attribute_guest_b_wait(WaitCause::Credit, bp_start, w_.now_ns());
        if (w_.link.epoch != epoch || tx.visible.classified) break;
      }
      if (w_.link.epoch != epoch || tx.visible.classified) continue;

      // Re-check slot occupancy after credit waits (another guest may have taken it).
      if (w_.tx_slot_blocks(side_, flit)) {
        const double wait_start = w_.now_ns();
        const WaitCause cause = (flit.guest_id == 1) ? block_cause_for_guest_b(w_, side_) : WaitCause::None;
        w_.send_q[side_].push(flit);
        w_.tx_peak[side_] = std::max(w_.tx_peak[side_], w_.send_q[side_].size());
        w_.sample("tx_" + std::to_string(side_), w_.send_q[side_].size());
        sc_core::wait(w_.ack_ev[side_] | w_.replay_q[side_].data_event() | w_.ctrl_q[side_].data_event() |
                      w_.link.state_ev | w_.link.flushed);
        if (cause != WaitCause::None) w_.attribute_guest_b_wait(cause, wait_start, w_.now_ns());
        continue;
      }

      if (w_.uses_per_guest_seq() && flit.guest_id < 3) {
        flit.vc = static_cast<int>(flit.guest_id);
        flit.seq = w_.link.next_seq_guest[side_][flit.vc];
        w_.link.next_seq_guest[side_][flit.vc] =
            static_cast<std::uint8_t>(w_.link.next_seq_guest[side_][flit.vc] + 1);
      } else {
        flit.vc = -1;
        flit.seq = w_.link.next_seq[side_];
        w_.link.next_seq[side_] = static_cast<std::uint8_t>(w_.link.next_seq[side_] + 1);
      }
      flit.link_epoch = w_.link.wire_gen;
      w_.note(tx, Phase::QueuedForLink, "UCIe", "queued for the link");
      if (flit.role == FlitRole::Request) {
        tx.visible.handed_to_ucie = true;
        w_.note(tx, Phase::Transmitting, "UCIe", "UCIe TX begins");
      } else if (flit.role == FlitRole::WriteData) {
        tx.visible.data_attempted = true;
        w_.log(TraceLevel::Transactions, "wdata_tx", "UCIe", &tx, "write-data TX begins");
      } else if (flit.role == FlitRole::Response) {
        w_.note(tx, Phase::ResponseTransmitting, "UCIe", "response TX");
      }
      if (tx.visible.classified || w_.link.destructive || w_.link.epoch != epoch) continue;
      if (w_.cfg.retry_enabled) {
        if (!w_.retry[side_].push(flit)) {
          w_.internal_errors.push_back("retry buffer push exceeded capacity");
          w_.record_error("retry_buffer_full", &tx, "retry buffer push exceeded capacity");
          continue;
        }
        w_.acquire_tx_slot(side_, flit);
      }
      // When retry is disabled, flits are not retained; outstanding-slot
      // accounting is skipped so the TX path is limited only by FIFO/credits.
      w_.tx_peak[side_] = std::max(w_.tx_peak[side_], w_.send_q[side_].size());
      w_.sample("retry_" + std::to_string(side_), w_.retry[side_].size());
      w_.sample("tx_" + std::to_string(side_), w_.send_q[side_].size());
      w_.sample("queue_guest_" + std::to_string(tx.guest_id), w_.guest_q[tx.guest_id].size());
    } else {
      flit.link_epoch = w_.link.wire_gen;
      flit.crc_ok = true;
      flit.corrupt_bits = 0;
      flit.drop = false;
      flit.duplicate = false;
      if (side_ == 0 && flit.guest_id == 0) {
        ++w_.resources.guest_a_replay_attempts;
        w_.resources.tx_slot_is_replay = true;
      }
      w_.acquire_tx_slot(side_, flit);
    }

    w_.apply_tx(flit);
    sc_core::wait(w_.serialization());
    if (w_.link.epoch != epoch) continue;
    if (flit.slot < w_.txns.size() && w_.slot(flit.slot).visible.classified && !flit.replay) continue;
    if (!w_.wire[side_].push(flit)) continue;
    w_.arm_replay_deadline(side_, flit.seq);
  }
}

void UcieAdapter::rx() {
  while (true) {
    Flit flit;
    if (!w_.arrive[side_].pop(flit)) continue;
    if (flit.link_epoch != w_.link.wire_gen) continue;

    if (flit.role == FlitRole::CreditReturn) {
      w_.credit[side_].on_return();
      w_.credit_ev[side_].notify(sc_core::SC_ZERO_TIME);
      continue;
    }
    if (flit.role == FlitRole::Ack) {
      if (!w_.cfg.retry_enabled) {
        w_.ack_ev[side_].notify(sc_core::SC_ZERO_TIME);
        continue;
      }
      if (auto stored = w_.retry[side_].copy_seq(flit.seq)) {
        if (auto* tx = w_.find(stored->logical_id)) note_ack(*tx, stored->role);
        if (stored->replay) ++w_.link.transparent_retries;
        const bool was_replay = stored->replay;
        w_.retry[side_].ack(flit.seq);
        w_.release_tx_slot(side_, *stored, was_replay);
        w_.link.streak[side_] = 0;
        w_.ack_ev[side_].notify(sc_core::SC_ZERO_TIME);
      }
      continue;
    }
    if (flit.role == FlitRole::Nak) {
      if (w_.retry[side_].mark_nak(flit.seq)) w_.nak_ev[side_].notify(sc_core::SC_ZERO_TIME);
      continue;
    }

    // [SPEC] UCIe 2.0 §3.7: a CRC mismatch makes the flit invalid. Replay is
    // requested. The flit is not forwarded. One, two, and three bit errors are
    // detected. This model does not execute the CRC LFSR.
    // [REFERENCE-SLIDE] CRC may be disabled for the throughput-overhead experiment.
    // [MODEL] With multiple outstanding flits, NAK the expected sequence so the
    // transmitter can go-back-N rather than treating a later in-flight flit as
    // an uncorrectable sequence error.
    if (w_.cfg.crc_enabled && !flit.crc_ok) {
      if (flit.corrupt_bits > 0 && flit.corrupt_bits <= w_.cfg.crc_detection_bits) {
        w_.note_detection();
        ++w_.link.crc_failures;
      }
      auto* tx = w_.find(flit.logical_id);
      w_.record_error("crc_error", tx, "CRC mismatch; NAK requested");
      const int sender = 1 - side_;
      std::uint8_t expect = w_.link.expected_seq[sender];
      if (flit.vc >= 0 && flit.vc < 3) expect = w_.link.expected_seq_guest[sender][flit.vc];
      Flit nak;
      nak.role = FlitRole::Nak;
      nak.seq = expect;
      nak.logical_id = flit.logical_id;
      nak.slot = flit.slot;
      nak.link_epoch = w_.link.wire_gen;
      w_.ctrl_q[side_].push(nak);
      continue;
    }

    sc_core::wait(w_.cycles(w_.cfg.buffer_cycles));
    if (flit.link_epoch != w_.link.wire_gen) continue;
    w_.rx_peak[side_] = std::max(w_.rx_peak[side_], w_.arrive[side_].size());
    w_.sample("rx_" + std::to_string(side_), w_.arrive[side_].size());

    if (w_.already_accepted(side_, flit)) {
      // [SPEC] UCIe 2.0 §3.8 duplicate suppression: a replay of an already
      // accepted sequence is acknowledged and not forwarded again.
      ++w_.link.transparent_retries;
      if (auto* tx = w_.find(flit.logical_id)) {
        w_.log(TraceLevel::Transactions, "duplicate_suppress", "UCIe", tx,
               "duplicate " + std::string(to_string(flit.role)) + " suppressed");
      }
      Flit ack;
      ack.role = FlitRole::Ack;
      ack.seq = flit.seq;
      ack.logical_id = flit.logical_id;
      ack.slot = flit.slot;
      ack.link_epoch = w_.link.wire_gen;
      w_.ctrl_q[side_].push(ack);
      continue;
    }

    const int sender = 1 - side_;
    std::uint8_t* expect =
        (flit.vc >= 0 && flit.vc < 3) ? &w_.link.expected_seq_guest[sender][flit.vc]
                                     : &w_.link.expected_seq[sender];
    if (flit.seq != *expect) {
      // [MODEL] Go-back-N receive: discard flits ahead of the expected sequence.
      // Do not emit an extra NAK here — the CRC/timeout path already requested
      // replay of *expect. Extra NAKs would inflate the retry streak.
      w_.log(TraceLevel::Transactions, "sequence_discard", "UCIe", w_.find(flit.logical_id),
             "[MODEL] discard flit ahead of expected seq (awaiting go-back-N replay)");
      continue;
    }
    *expect = static_cast<std::uint8_t>(*expect + 1);
    w_.remember(side_, flit);
    w_.link.streak[sender] = 0;

    if (flit.slot >= w_.txns.size()) continue;
    auto& tx = w_.slot(flit.slot);
    if (tx.visible.classified) continue;

    if (side_ == 1 && (flit.role == FlitRole::Request || flit.role == FlitRole::WriteData)) {
      if (tx.t_remote_rx_ns < 0) {
        w_.note(tx, Phase::RemoteReceived, "UCIe", "remote endpoint receives the request");
      }
      auto& part = w_.partial[flit.slot];
      if (flit.role == FlitRole::Request) part.request = true;
      if (flit.role == FlitRole::WriteData) part.data = true;
      if (part.request && (!tx.is_write || part.data) && !part.queued) {
        part.queued = true;
        w_.target_q.push(flit.slot);
      }
    } else if (side_ == 0 && flit.role == FlitRole::Response) {
      tx.visible.response_at_local_aou = true;
      w_.note(tx, Phase::ResponseReceived, "AoU", "response received by the initiating AoU");
      w_.completion_q.push(flit.slot);
    }

    Flit ack;
    ack.role = FlitRole::Ack;
    ack.seq = flit.seq;
    ack.logical_id = flit.logical_id;
    ack.slot = flit.slot;
    ack.link_epoch = w_.link.wire_gen;
    w_.ctrl_q[side_].push(ack);

    // [REFERENCE-SLIDE] Early credit return + credit update threshold.
    // Credits are freed when the RX path accepts the flit (early). Updates are
    // batched until threshold flits are freed.
    w_.note_credit_freed(side_);
    if (const int n = w_.take_credit_update(side_)) {
      for (int i = 0; i < n; ++i) {
        Flit cred;
        cred.role = FlitRole::CreditReturn;
        cred.logical_id = flit.logical_id;
        cred.slot = flit.slot;
        cred.link_epoch = w_.link.wire_gen;
        w_.ctrl_q[side_].push(cred);
      }
    }
  }
}

void UcieAdapter::monitor() {
  while (true) {
    if (w_.link.resend[side_] && w_.link.state == LinkState::Active && !w_.link.busy) {
      w_.link.resend[side_] = false;
      for (auto flit : w_.retry[side_].snapshot()) {
        flit.replay = true;
        flit.crc_ok = true;
        flit.corrupt_bits = 0;
        flit.drop = false;
        w_.replay_q[side_].push(flit);
      }
    }
    if (w_.link.busy || w_.link.state != LinkState::Active) {
      sc_core::wait(w_.link.idle_ev | w_.link.state_ev);
      continue;
    }
    {
      auto replay_set = w_.retry[side_].take_nak_replay();
      if (!replay_set.empty()) {
        Transaction* tx = w_.find(replay_set.front().logical_id);
        if (tx && !tx->visible.classified) {
          note_nak(*tx, replay_set.front().role);
          ++tx->retry_count;
          ++tx->replay_count;
        }
        ++w_.link.streak[side_];
        std::ostringstream text;
        text << "UCIe CRC failure on the " << to_string(replay_set.front().role)
             << " flit (1-bit corruption); go-back-N replay of " << replay_set.size()
             << " flit(s), streak " << w_.link.streak[side_];
        w_.log(TraceLevel::Transactions, "crc", "UCIe", tx, text.str());
        for (auto& spec : w_.scenario.faults) {
          if (spec.fired || spec.trigger != Trigger::Condition) continue;
          if (tx && tx->retry_count >= spec.retry_at) {
            spec.fired = true;
            w_.link.fail_retrain = spec.fail_retrain;
            w_.request(RecoveryCmd::Retrain);
          }
        }
        if (w_.link.streak[side_] >= w_.cfg.replay_limit) {
          w_.log(TraceLevel::Transactions, "retry_threshold", "UCIe", tx, "Retry threshold exceeded");
          w_.record_error("retry_exhaustion", tx, "Retry threshold exceeded; Retrain requested");
          // [SPEC] UCIe 2.0 §3.8: a condition that sends PCIe to Recovery makes
          // the adapter initiate Retrain. The integer limit is a parameter.
          w_.request(RecoveryCmd::Retrain);
          continue;
        }
        for (auto flit : replay_set) {
          flit.replay = true;
          flit.crc_ok = true;
          flit.corrupt_bits = 0;
          flit.drop = false;
          w_.replay_q[side_].push(flit);
        }
        w_.record_error("retry", tx, "CRC NAK go-back-N replay scheduled");
        continue;
      }
    }
    if (auto expired = w_.retry[side_].take_expired(w_.now_ns())) {
      Transaction* tx = w_.find(expired->logical_id);
      if (tx && !tx->visible.classified) {
        note_timeout(*tx, expired->role);
        ++tx->retry_count;
        ++tx->replay_count;
        w_.log(TraceLevel::Transactions, "replay_timeout", "UCIe", tx,
               "[SPEC] replay timeout reached (REPLAY_TIMEOUT_FLIT_COUNT >= 375, UCIe 2.0 3.8)");
      } else if (tx && tx->visible.classified) {
        // Entry already removed by take_expired; free slot accounting only.
        w_.release_tx_slot(side_, *expired, true);
        w_.ack_ev[side_].notify(sc_core::SC_ZERO_TIME);
        continue;
      }
      ++w_.link.streak[side_];
      if (w_.link.streak[side_] >= w_.cfg.replay_limit) {
        w_.log(TraceLevel::Transactions, "retry_threshold", "UCIe", tx, "Retry threshold exceeded");
        w_.record_error("retry_exhaustion", tx, "Replay timeout exhaustion; Retrain requested");
        w_.request(RecoveryCmd::Retrain);
        continue;
      }
      expired->replay = true;
      expired->crc_ok = true;
      expired->corrupt_bits = 0;
      w_.replay_q[side_].push(*expired);
      continue;
    }
    sc_core::wait(sc_core::sc_time(1, sc_core::SC_US),
                  w_.nak_ev[side_] | w_.timer_ev[side_] | w_.link.state_ev | w_.link.idle_ev);
  }
}

}  // namespace ucie
