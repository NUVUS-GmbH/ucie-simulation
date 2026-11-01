#include "sim/modules.hpp"

namespace ucie {

AouEndpoint::AouEndpoint(sc_core::sc_module_name name, World& world) : sc_module(name), w_(world) {
  SC_THREAD(run);
}

void AouEndpoint::run() {
  while (true) {
    std::uint32_t slot = 0;
    if (!w_.aou_q.pop(slot)) continue;
    if (slot >= w_.txns.size()) continue;
    auto& tx = w_.slot(slot);
    const auto epoch = w_.link.epoch;
    // [MODEL] AoU packing latency. One message per flit. The granule encoding
    // is not modeled. [ASSUMPTION] Credits are one per message.
    sc_core::wait(w_.cycles(w_.cfg.aou_pack_cycles));
    if (w_.link.epoch != epoch || tx.visible.classified) continue;
    w_.note(tx, Phase::AouPacked, "AoU", "AoU packet ready");
    if (tx.visible.classified || w_.link.destructive || w_.link.epoch != epoch) continue;

    Flit request;
    request.logical_id = tx.logical_id;
    request.slot = slot;
    request.guest_id = tx.guest_id;
    request.role = FlitRole::Request;
    if (!w_.send_q[0].push(request)) continue;
    w_.sample("tx_a", w_.send_q[0].size());
    if (!tx.is_write) continue;
    if (w_.link.epoch != epoch || tx.visible.classified) continue;
    Flit data = request;
    data.role = FlitRole::WriteData;
    w_.send_q[0].push(data);
    w_.sample("tx_a", w_.send_q[0].size());
  }
}

}  // namespace ucie
