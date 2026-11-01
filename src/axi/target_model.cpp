#include "target_model.hpp"

#include <stdexcept>

namespace ucie {

std::string TargetModel::commit(Transaction& tx) {
  // Caller has already waited the configured commit latency. This statement
  // is the architectural commit point. [MODEL] That latency is not a UCIe value.
  tx.ground.architectural_commit_count += 1;
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

}  // namespace ucie
