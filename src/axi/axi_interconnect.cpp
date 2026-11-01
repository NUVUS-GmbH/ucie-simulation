#include "sim/modules.hpp"

namespace ucie {

AxiInterconnect::AxiInterconnect(sc_core::sc_module_name name, World& world) : sc_module(name), w_(world) {
  SC_THREAD(run);
}

void AxiInterconnect::run() {
  while (true) {
    std::vector<int> ready;
    for (int guest = 0; guest < 3; ++guest) {
      if (w_.guest_q[guest].size() > 0) ready.push_back(guest);
    }
    if (ready.empty()) {
      sc_core::wait(w_.guest_q[0].data_event() | w_.guest_q[1].data_event() | w_.guest_q[2].data_event());
      continue;
    }
    // FIFO keeps the ready vector in guest-id order, which matches single-queue
    // arrival when the scheduler fills one guest at a time. Strict, round-robin,
    // and weighted policies use the shared arbiter.
    int index = 0;
    if (w_.scenario.arbitration != ArbPolicy::Fifo) {
      index = w_.arbiter.select(ready, w_.scenario.guest_priority, w_.scenario.guest_weight);
    }
    if (index < 0) continue;
    const int guest = ready[index];
    std::uint32_t slot = 0;
    if (!w_.guest_q[guest].try_pop(slot)) continue;
    // [MODEL] Arbitration delay at the shared AXI port.
    const double arb_start = w_.now_ns();
    sc_core::wait(w_.cycles(w_.cfg.arbitration_cycles));
    if (slot < w_.txns.size()) {
      auto& tx = w_.slot(slot);
      if (tx.visible.classified) continue;
      if (tx.guest_id == 1) {
        w_.attribute_guest_b_wait(WaitCause::Arbitration, arb_start, w_.now_ns());
      }
    }
    w_.axi_q.push(slot);
  }
}

}  // namespace ucie
