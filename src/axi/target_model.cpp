#include "target_model.hpp"

#include <stdexcept>

namespace ucie {
namespace {

void append_bytes(TargetModel& m, Transaction& tx, int valid_bytes, std::uint64_t tdata) {
  for (int i = 0; i < valid_bytes; ++i) {
    const auto b = static_cast<std::uint8_t>((tdata >> (8 * (i % 8))) & 0xff);
    m.axis_bytes.push_back(b);
    tx.axis.pkt_buffer.push_back(b);
  }
}

}  // namespace

bool TargetModel::axis_dup_blocks(std::uint64_t packet_op_id, double now_ns) {
  if (!axis_dup_suppress || packet_op_id == 0) return false;
  for (auto it = axis_dup_table.begin(); it != axis_dup_table.end();) {
    if (axis_dup_lifetime_ns > 0 && it->committed_ns >= 0 &&
        now_ns - it->committed_ns > axis_dup_lifetime_ns) {
      it = axis_dup_table.erase(it);
      continue;
    }
    if (it->packet_op_id == packet_op_id) return true;
    ++it;
  }
  return false;
}

void TargetModel::axis_dup_remember(std::uint64_t packet_op_id, double now_ns) {
  if (!axis_dup_suppress || packet_op_id == 0) return;
  for (const auto& e : axis_dup_table) {
    if (e.packet_op_id == packet_op_id) return;
  }
  while (static_cast<int>(axis_dup_table.size()) >= axis_dup_capacity && !axis_dup_table.empty()) {
    axis_dup_table.pop_front();
  }
  axis_dup_table.push_back(AxisDupEntry{packet_op_id, now_ns});
}

void TargetModel::axis_dup_on_reset() {
  if (axis_dup_clear_on_reset) axis_dup_table.clear();
}

std::string TargetModel::commit_atomic_store(Transaction& tx) {
  // IHI0022L A6.4.1 AtomicStore: Subordinate performs op using sent data and
  // addressed value; stores result; single write response WITHOUT data.
  // A6.4.2 ADD: memory := memory + sent_data.
  // This path never fabricates an R-channel return of the prior value.
  if (tx.axi5.form != Axi5AtomicForm::AtomicStore || tx.axi5.atomic_op != Axi5AtomicOp::Add) {
    tx.axi5.unsupported_form = true;
    tx.axi5.unsupported_reason = "AXI5-MM-P1 implements only AtomicStore(ADD)";
    tx.axi5.intended_bresp = AxiResp::Slverr;
    return tx.axi5.unsupported_reason;
  }
  if (tx.axi5.awatop != kAwatopAtomicStoreAddLe && tx.axi5.awatop != 0) {
    // Accept only the documented LE ADD encoding (or 0 when already validated).
    if ((tx.axi5.awatop & 0x37) != kAwatopAtomicStoreAddLe) {
      tx.axi5.unsupported_form = true;
      tx.axi5.unsupported_reason = "unsupported AWATOP encoding for AXI5-MM-P1";
      tx.axi5.intended_bresp = AxiResp::Slverr;
      return tx.axi5.unsupported_reason;
    }
  }

  tx.axi5.poison_observed_at_target = tx.axi5.wpoison;
  const int operand = tx.axi5.operand != 0 ? tx.axi5.operand : tx.data;
  tx.axi5.operand = operand;
  tx.axi5.value_before = atomic_value;

  // Poison policy (TARGET POLICY, not Spec mandate). Spec A16.1: Poison marks
  // previously corrupted data and is independent of error responses. Spec does
  // NOT require that Poison prevent commit — named policies below.
  if (tx.axi5.wpoison) {
    switch (poison_policy) {
      case PoisonTargetPolicy::RejectWithoutCommitSlverr:
        tx.axi5.poison_rejected = true;
        tx.axi5.intended_bresp = AxiResp::Slverr;
        poisoned_locations[tx.address] = true;
        tx.axi5.effect_log.push_back("poison reject (no commit) SLVERR");
        return "poison reject_without_commit_slverr; atomic_value unchanged=" +
               std::to_string(atomic_value);
      case PoisonTargetPolicy::RejectWithoutCommitOkay:
        tx.axi5.poison_rejected = true;
        tx.axi5.intended_bresp = AxiResp::Okay;
        poisoned_locations[tx.address] = true;
        tx.axi5.effect_log.push_back("poison reject (no commit) OKAY");
        return "poison reject_without_commit_okay; atomic_value unchanged=" +
               std::to_string(atomic_value);
      case PoisonTargetPolicy::AcceptMarkedOkay:
        tx.axi5.poison_accepted_marked = true;
        tx.axi5.intended_bresp = AxiResp::Okay;
        poisoned_locations[tx.address] = true;
        break;
      case PoisonTargetPolicy::AcceptMarkedSlverr:
        tx.axi5.poison_accepted_marked = true;
        tx.axi5.intended_bresp = AxiResp::Slverr;
        poisoned_locations[tx.address] = true;
        break;
    }
  } else {
    tx.axi5.intended_bresp = AxiResp::Okay;
  }

  if (duplicate_suppress && tx.bridge.token != 0) {
    auto it = committed_tokens.find(tx.bridge.token);
    if (it != committed_tokens.end()) {
      ++duplicates_suppressed;
      tx.axi5.effect_log.push_back("dup suppress");
      return "atomic dup suppress token=" + std::to_string(tx.bridge.token);
    }
    committed_tokens[tx.bridge.token] = 1;
  }

  // Architectural ADD — this is the commit point for AtomicStore(ADD).
  atomic_value += operand;
  memory[tx.address] = atomic_value;
  ++atomic_ops_applied;
  tx.axi5.atomic_effect_applied = true;
  tx.axi5.atomic_effects += 1;
  tx.axi5.value_after = atomic_value;
  tx.ground.architectural_commit_count += 1;
  if (tx.ground.commit_time_ns < 0) {
    // Caller may overwrite with wall time; leave unset here if unknown.
  }
  tx.ground.target_accepted = true;
  if (tx.ground.architectural_commit_count > axi5_commit_watermark) {
    axi5_commit_watermark = tx.ground.architectural_commit_count;
  }
  // Poison must never undo: watermark / ops_applied / atomic_value only grow
  // for successful ADDs; reject paths leave them unchanged.
  std::string line = "AtomicStore(ADD) " + std::to_string(tx.axi5.value_before) + "+" +
                     std::to_string(operand) + " -> " + std::to_string(atomic_value) +
                     " ops=" + std::to_string(atomic_ops_applied);
  tx.axi5.effect_log.push_back(line);
  return line;
}

std::string TargetModel::commit(Transaction& tx) {
  // Caller has already waited the configured commit latency. This statement
  // is the architectural commit point. [MODEL] That latency is not a UCIe value.
  if (tx.axi5.enabled && tx.axi5.unsupported_form) {
    tx.axi5.intended_bresp = AxiResp::Slverr;
    return "AXI5-MM-P1 unsupported form refused: " + tx.axi5.unsupported_reason;
  }
  if (tx.axi5.enabled && tx.axi5.form == Axi5AtomicForm::AtomicStore) {
    return commit_atomic_store(tx);
  }
  // Ordinary MM write with poison attribute (non-atomic). Same named policies.
  if (tx.axi5.enabled && tx.axi5.wpoison && tx.axi5.form == Axi5AtomicForm::None) {
    tx.axi5.poison_observed_at_target = true;
    switch (poison_policy) {
      case PoisonTargetPolicy::RejectWithoutCommitSlverr:
        tx.axi5.poison_rejected = true;
        tx.axi5.intended_bresp = AxiResp::Slverr;
        poisoned_locations[tx.address] = true;
        return "poisoned write rejected (no commit) SLVERR";
      case PoisonTargetPolicy::RejectWithoutCommitOkay:
        tx.axi5.poison_rejected = true;
        tx.axi5.intended_bresp = AxiResp::Okay;
        poisoned_locations[tx.address] = true;
        return "poisoned write rejected (no commit) OKAY";
      case PoisonTargetPolicy::AcceptMarkedOkay:
        tx.axi5.poison_accepted_marked = true;
        tx.axi5.intended_bresp = AxiResp::Okay;
        poisoned_locations[tx.address] = true;
        break;
      case PoisonTargetPolicy::AcceptMarkedSlverr:
        tx.axi5.poison_accepted_marked = true;
        tx.axi5.intended_bresp = AxiResp::Slverr;
        poisoned_locations[tx.address] = true;
        break;
    }
  }

  if (duplicate_suppress && tx.bridge.token != 0) {
    auto it = committed_tokens.find(tx.bridge.token);
    if (it != committed_tokens.end()) {
      ++duplicates_suppressed;
      return "duplicate suppress token=" + std::to_string(tx.bridge.token);
    }
    committed_tokens[tx.bridge.token] = 1;
  }
  tx.ground.architectural_commit_count += 1;
  if (tx.ground.architectural_commit_count > axi5_commit_watermark) {
    axi5_commit_watermark = tx.ground.architectural_commit_count;
  }
  switch (tx.op) {
    case OpKind::IncrementCounter: {
      const int old = counter;
      counter += 1;
      return "counter " + std::to_string(old) + " -> " + std::to_string(counter);
    }
    case OpKind::IncrementActuator: {
      const int old = actuator;
      actuator += 1;
      return "actuator " + std::to_string(old) + " -> " + std::to_string(actuator);
    }
    case OpKind::PushFifo: {
      const auto old = fifo.size();
      fifo.push_back(tx.data);
      return "fifo " + std::to_string(old) + " -> " + std::to_string(fifo.size());
    }
    case OpKind::PopFifo: {
      const auto old = fifo.size();
      tx.data = fifo.empty() ? 0 : fifo.front();
      if (!fifo.empty()) fifo.erase(fifo.begin());
      return "fifo pop " + std::to_string(old) + " -> " + std::to_string(fifo.size());
    }
    case OpKind::TriggerDma:
      dma_starts += 1;
      return "dma starts " + std::to_string(dma_starts);
    case OpKind::StartAccelerator:
      accelerator_starts += 1;
      return "accelerator starts " + std::to_string(accelerator_starts);
    case OpKind::RingDoorbell:
      doorbell_rings += 1;
      return "doorbell " + std::to_string(doorbell_rings);
    case OpKind::AckInterrupt:
      interrupts_acked += 1;
      return "irq ack " + std::to_string(interrupts_acked);
    case OpKind::MemoryWrite:
      memory[tx.address] = tx.data;
      return "memory write";
    case OpKind::MemoryRead:
      tx.data = memory.count(tx.address) ? memory[tx.address] : 0;
      return "memory read";
  }
  throw std::logic_error("unhandled target op");
}

std::string TargetModel::axis_commit_imm_beat(Transaction& tx, int beat_index, double now_ns) {
  if (beat_index < 0 || beat_index >= static_cast<int>(tx.axis.offers.size())) {
    return "axis imm beat out of range";
  }
  const auto& offer = tx.axis.offers[static_cast<std::size_t>(beat_index)];
  if (tx.axis.packet_id_enabled && axis_dup_blocks(tx.axis.packet_op_id, now_ns)) {
    ++axis_duplicates_suppressed;
    tx.axis.dup_suppress_hit = true;
    return "axis dup suppress packet_op_id=" + std::to_string(tx.axis.packet_op_id);
  }

  append_bytes(*this, tx, offer.valid_bytes, offer.tdata);
  tx.axis.bytes_committed += offer.valid_bytes;
  tx.axis.app_effects += 1;
  tx.axis.effect_times_ns.push_back(now_ns);
  tx.ground.architectural_commit_count += 1;
  tx.ground.beats_committed += 1;
  tx.ground.commit_times_ns.push_back(now_ns);
  if (tx.ground.commit_time_ns < 0) tx.ground.commit_time_ns = now_ns;
  tx.ground.target_accepted = true;
  counter += offer.valid_bytes;  // GT counter tracks committed stream bytes
  if (offer.tlast && tx.axis.packet_id_enabled) {
    axis_dup_remember(tx.axis.packet_op_id, now_ns);
  }
  std::string line = "AXIS-IMM beat=" + std::to_string(beat_index) +
                     " bytes=" + std::to_string(offer.valid_bytes) +
                     " effects=" + std::to_string(tx.axis.app_effects);
  tx.axis.effect_log.push_back(line);
  return line;
}

std::string TargetModel::axis_commit_pkt_effect(Transaction& tx, double now_ns) {
  // Explicit atomic packet effect. Must not be inferred from remote_packet_complete
  // alone — caller is responsible for process-start staging.
  if (!tx.axis.processing_started) {
    return "axis pkt effect refused: processing not started";
  }
  if (tx.axis.packet_id_enabled && axis_dup_blocks(tx.axis.packet_op_id, now_ns)) {
    ++axis_duplicates_suppressed;
    tx.axis.dup_suppress_hit = true;
    return "axis dup suppress packet_op_id=" + std::to_string(tx.axis.packet_op_id);
  }
  const int nbytes = static_cast<int>(tx.axis.pkt_buffer.size());
  for (auto b : tx.axis.pkt_buffer) axis_bytes.push_back(b);
  tx.axis.bytes_committed += nbytes;
  tx.axis.app_effects += 1;
  axis_packets_committed += 1;
  tx.axis.effect_times_ns.push_back(now_ns);
  tx.ground.architectural_commit_count += 1;
  tx.ground.beats_committed += tx.axis.remote_beats_accepted;
  tx.ground.commit_times_ns.push_back(now_ns);
  if (tx.ground.commit_time_ns < 0) tx.ground.commit_time_ns = now_ns;
  tx.ground.target_accepted = true;
  counter += 1;  // one packet-level architectural effect
  if (tx.axis.packet_id_enabled) axis_dup_remember(tx.axis.packet_op_id, now_ns);
  std::string line = "AXIS-PKT effect bytes=" + std::to_string(nbytes) +
                     " effects=" + std::to_string(tx.axis.app_effects);
  tx.axis.effect_log.push_back(line);
  return line;
}

}  // namespace ucie
