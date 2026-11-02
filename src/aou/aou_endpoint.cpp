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
    // Bridge-AXIS-P1: one StreamSegment flit per locally accepted transfer.
    // This is a named MODEL PROFILE bridge policy — not an AoU-spec claim
    // (AoU full text remains a documented source gap).
    sc_core::wait(w_.cycles(w_.cfg.aou_pack_cycles));
    if (w_.link.epoch != epoch || tx.visible.classified) continue;
    w_.note(tx, Phase::AouPacked, "AoU", "AoU packet ready");
    if (tx.visible.classified || w_.link.destructive || w_.link.epoch != epoch) continue;

    if (tx.axis.enabled) {
      if (tx.axis.pending_pack_beats.empty()) continue;
      const int pack_beat = tx.axis.pending_pack_beats.front();
      tx.axis.pending_pack_beats.pop_front();
      if (pack_beat < 0 || pack_beat >= static_cast<int>(tx.axis.offers.size())) continue;
      const auto& offer = tx.axis.offers[static_cast<std::size_t>(pack_beat)];
      Flit seg;
      seg.logical_id = tx.logical_id;
      seg.slot = slot;
      seg.guest_id = tx.guest_id;
      seg.axi_id = tx.axi_id;
      seg.role = FlitRole::StreamSegment;
      seg.axis_segment = true;
      seg.beat_index = offer.beat_index;
      seg.tlast = offer.tlast;
      seg.tkeep = offer.tkeep;
      seg.tid = offer.tid;
      seg.tdest = offer.tdest;
      seg.tuser = offer.tuser;
      seg.tdata = offer.tdata;
      seg.valid_bytes = offer.valid_bytes;
      seg.packet_op_id = tx.axis.packet_op_id;
      seg.bridge_token = tx.bridge.token;
      seg.bridge_generation = tx.bridge.generation;
      if (!w_.send_q[0].push(seg)) continue;
      w_.sample("tx_a", w_.send_q[0].size());
      continue;
    }

    Flit request;
    request.logical_id = tx.logical_id;
    request.slot = slot;
    request.guest_id = tx.guest_id;
    request.axi_id = tx.axi_id;
    request.role = FlitRole::Request;
    request.bridge_token = tx.bridge.token;
    request.bridge_generation = tx.bridge.generation;
    // AXI5-MM-P1 fields on the modeled request flit (MODEL PROFILE packing).
    if (tx.axi5.enabled) {
      request.awatop = tx.axi5.awatop;
      request.awtrace = tx.axi5.awtrace;
      request.operand = tx.axi5.operand != 0 ? tx.axi5.operand : tx.data;
    }
    if (!w_.send_q[0].push(request)) continue;
    w_.sample("tx_a", w_.send_q[0].size());
    if (!tx.is_write) continue;
    if (w_.link.epoch != epoch || tx.visible.classified) continue;
    Flit data = request;
    data.role = FlitRole::WriteData;
    if (tx.axi5.enabled) {
      data.wpoison = tx.axi5.wpoison;
      data.wtrace = tx.axi5.wtrace;
      data.operand = tx.axi5.operand != 0 ? tx.axi5.operand : tx.data;
    }
    w_.send_q[0].push(data);
    w_.sample("tx_a", w_.send_q[0].size());
  }
}

}  // namespace ucie
