#include "sim/modules.hpp"

namespace ucie {

AxiMaster::AxiMaster(sc_core::sc_module_name name, World& world) : sc_module(name), w_(world) {
  SC_THREAD(accept);
  SC_THREAD(complete);
}

void AxiMaster::accept() {
  while (true) {
    std::uint32_t slot = 0;
    if (!w_.axi_q.pop(slot)) continue;
    if (slot >= w_.txns.size()) continue;
    auto& tx = w_.slot(slot);
    const auto epoch = w_.link.epoch;
    // [MODEL] AXI handshake latency. Not defined by UCIe.
    sc_core::wait(w_.cycles(w_.cfg.axi_handshake_cycles));
    if (w_.link.epoch != epoch || tx.visible.classified) continue;
    tx.visible.axi_accepted = true;
    w_.note(tx, Phase::AxiAccepted, "AXI", "AXI accepts T" + std::to_string(tx.logical_id));
    if (tx.visible.classified || w_.link.destructive || w_.link.epoch != epoch) continue;
    w_.aou_q.push(slot);
  }
}

void AxiMaster::complete() {
  while (true) {
    std::uint32_t slot = 0;
    if (!w_.completion_q.pop(slot)) continue;
    if (slot >= w_.txns.size()) continue;
    auto& tx = w_.slot(slot);
    const auto epoch = w_.link.epoch;
    sc_core::wait(w_.cycles(w_.cfg.completion_cycles));
    if (tx.visible.classified || tx.visible.axi_completed_ok) continue;
    if (w_.link.epoch != epoch && w_.scenario.policy != RecoveryPolicy::PreserveLocal) continue;
    w_.complete(tx);
  }
}

}  // namespace ucie
