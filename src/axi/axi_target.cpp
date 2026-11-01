#include "sim/modules.hpp"

namespace ucie {

AxiTarget::AxiTarget(sc_core::sc_module_name name, World& world) : sc_module(name), w_(world) {
  SC_THREAD(run);
}

void AxiTarget::run() {
  while (true) {
    std::uint32_t slot = 0;
    if (!w_.target_q.pop(slot)) continue;
    if (slot >= w_.txns.size()) continue;
    auto& tx = w_.slot(slot);
    const auto epoch = w_.link.epoch;
    sc_core::wait(w_.cycles(w_.cfg.target_accept_cycles));
    if (w_.link.epoch != epoch || tx.visible.classified) continue;
    w_.note(tx, Phase::TargetAccepted, "Target", "target accepts T" + std::to_string(tx.logical_id));
    if (tx.visible.classified || w_.link.destructive || w_.link.epoch != epoch) continue;

    sc_core::wait(w_.cycles(w_.cfg.target_commit_cycles));
    if (w_.link.epoch != epoch || tx.visible.classified) continue;
    // Architectural commit is this call, after the commit wait and before
    // any response exists. A later link fault cannot roll it back.
    const std::string effect = w_.target.commit(tx);
    w_.note(tx, Phase::ArchitecturalCommit, "Target", "ARCHITECTURAL COMMIT");
    w_.log(TraceLevel::Transactions, "commit", "Target", &tx, effect);
    if (tx.visible.classified || w_.link.destructive || w_.link.epoch != epoch) continue;

    sc_core::wait(w_.cycles(w_.cfg.target_response_cycles));
    if (w_.link.epoch != epoch || tx.visible.classified) continue;
    w_.note(tx, Phase::ResponseCreated, "Target", "response created");
    if (tx.visible.classified || w_.link.destructive || w_.link.epoch != epoch) continue;

    // [MODEL] Return-path AoU pack uses the same pack delay as the request.
    sc_core::wait(w_.cycles(w_.cfg.aou_pack_cycles));
    if (w_.link.epoch != epoch || tx.visible.classified) continue;
    w_.note(tx, Phase::ResponseQueued, "AoU", "response queued");
    if (tx.visible.classified || w_.link.destructive || w_.link.epoch != epoch) continue;

    Flit response;
    response.logical_id = tx.logical_id;
    response.slot = slot;
    response.guest_id = tx.guest_id;
    response.role = FlitRole::Response;
    w_.send_q[1].push(response);
    w_.sample("tx_b", w_.send_q[1].size());
  }
}

}  // namespace ucie
