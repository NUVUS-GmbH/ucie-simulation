#include "sim/modules.hpp"

namespace ucie {
namespace {

void handle_axis_sink(World& w, Transaction& tx, std::uint64_t epoch) {
  if (tx.axis.pending_remote_beats.empty()) return;
  const int beat = tx.axis.pending_remote_beats.front();
  tx.axis.pending_remote_beats.pop_front();
  if (beat < 0 || beat >= static_cast<int>(tx.axis.offers.size())) return;
  const auto& offer = tx.axis.offers[static_cast<std::size_t>(beat)];

  sc_core::wait(w.cycles(w.cfg.target_accept_cycles));
  if (w.link.epoch != epoch && !w.remote_may_continue(tx)) return;
  if (tx.visible.classified && !w.remote_may_continue(tx)) return;
  w.note(tx, Phase::TargetAccepted, "Target",
         "AXIS sink accepts beat " + std::to_string(beat));
  tx.bridge.remote_inflight = true;

  if (tx.axis.sink == AxisSinkKind::Imm) {
    // Commit point: after accept wait + commit latency, per transfer.
    sc_core::wait(w.cycles(w.cfg.target_commit_cycles));
    if (w.link.epoch != epoch && !w.remote_may_continue(tx)) return;
    if (tx.visible.classified && !w.remote_may_continue(tx)) return;
    // Mid-packet LinkError while the local stream lifecycle is still open:
    // do not start further IMM effects. Late commit after local close remains
    // possible when allow_remote_continue is set (timeout/cancel races).
    if (w.link.destructive && !tx.bridge.local_closed) return;
    const std::string effect = w.target.axis_commit_imm_beat(tx, beat, w.now_ns());
    w.note(tx, Phase::ArchitecturalCommit, "Target", "AXIS-IMM ARCHITECTURAL EFFECT");
    w.log(TraceLevel::Transactions, "axis_imm_commit", "Target", &tx, effect);
    // Ground-truth effect only — must not enter observer A–D projections.
    w.record_observer_event(tx, "GT:AXIS_IMM_EFFECT_" + std::to_string(beat));

    if (offer.tlast && tx.axis.app_ack_mode != AxisAppAckMode::Off) {
      sc_core::wait(w.cycles(w.cfg.target_response_cycles));
      Flit ack;
      ack.logical_id = tx.logical_id;
      ack.slot = w.slot_index(tx);
      ack.guest_id = tx.guest_id;
      ack.axi_id = tx.axi_id;
      ack.role = FlitRole::Response;
      ack.app_ack = true;
      ack.bridge_token = tx.bridge.token;
      ack.bridge_generation = tx.bridge.generation;
      ack.packet_op_id = tx.axis.packet_op_id;
      w.send_q[1].push(ack);
    }
    return;
  }

  // SINK-PKT: (1) reception into buffer — not an application effect.
  for (int i = 0; i < offer.valid_bytes; ++i) {
    const auto b = static_cast<std::uint8_t>((offer.tdata >> (8 * (i % 8))) & 0xff);
    tx.axis.pkt_buffer.push_back(b);
  }
  w.log(TraceLevel::Transactions, "axis_pkt_buffer", "Target", &tx,
        "buffered beat=" + std::to_string(beat) + " bytes=" + std::to_string(offer.valid_bytes) +
            " sealed=" + (offer.tlast ? "pending_tlast" : "no"));

  if (!offer.tlast) return;

  // (1) Complete packet reception already flagged in accept_axis_segment.
  // (2) Explicit processing start — NOT derived solely from reception.
  sc_core::wait(w.cycles(std::max(0, w.cfg.axis_sink_process_start_cycles)));
  if (w.link.epoch != epoch && !w.remote_may_continue(tx)) return;
  if (tx.visible.classified && !w.remote_may_continue(tx)) return;
  if (w.link.destructive && !tx.bridge.local_closed) return;
  if (!tx.axis.remote_packet_complete) {
    w.log(TraceLevel::Errors, "axis_pkt_process_refused", "Target", &tx,
          "processing start refused: packet not complete");
    return;
  }
  tx.axis.processing_started = true;
  tx.axis.t_processing_start_ns = w.now_ns();
  w.log(TraceLevel::Transactions, "axis_pkt_process_start", "Target", &tx,
        "SINK-PKT processing started (separate from packet_complete)");
  w.record_observer_event(tx, "GT:AXIS_PKT_PROCESS_START");

  // (3) Atomic application effect(s) after explicit process-start.
  sc_core::wait(w.cycles(std::max(1, w.cfg.axis_sink_effect_cycles)));
  if (w.link.epoch != epoch && !w.remote_may_continue(tx)) return;
  if (tx.visible.classified && !w.remote_may_continue(tx)) return;
  const std::string effect = w.target.axis_commit_pkt_effect(tx, w.now_ns());
  w.note(tx, Phase::ArchitecturalCommit, "Target", "AXIS-PKT ARCHITECTURAL EFFECT");
  w.log(TraceLevel::Transactions, "axis_pkt_commit", "Target", &tx, effect);
  w.record_observer_event(tx, "GT:AXIS_PKT_EFFECT");

  if (tx.axis.app_ack_mode != AxisAppAckMode::Off) {
    sc_core::wait(w.cycles(w.cfg.target_response_cycles));
    Flit ack;
    ack.logical_id = tx.logical_id;
    ack.slot = w.slot_index(tx);
    ack.guest_id = tx.guest_id;
    ack.axi_id = tx.axi_id;
    ack.role = FlitRole::Response;
    ack.app_ack = true;
    ack.bridge_token = tx.bridge.token;
    ack.bridge_generation = tx.bridge.generation;
    ack.packet_op_id = tx.axis.packet_op_id;
    w.send_q[1].push(ack);
  }
}

}  // namespace

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

    if (tx.axis.enabled) {
      handle_axis_sink(w_, tx, epoch);
      continue;
    }

    // Once the request reached the remote target queue, architectural progress
    // may continue even if the initiator bridge already closed locally
    // (WDT-03/ERR-05). Ground truth is still recorded for evaluation.
    const bool local_already_closed =
        tx.bridge.local_closed || tx.visible.axi_completed_ok || tx.visible.axi_completed_err;

    sc_core::wait(w_.cycles(w_.cfg.target_accept_cycles));
    if (w_.link.epoch != epoch && !local_already_closed) continue;
    if (tx.visible.classified && !w_.remote_may_continue(tx)) continue;
    w_.note(tx, Phase::TargetAccepted, "Target", "target accepts T" + std::to_string(tx.logical_id));
    tx.bridge.remote_inflight = true;
    if (w_.link.destructive && !local_already_closed) continue;

    sc_core::wait(w_.cycles(w_.cfg.target_commit_cycles));
    if (w_.link.epoch != epoch && !w_.remote_may_continue(tx)) continue;
    if (tx.visible.classified && !w_.remote_may_continue(tx)) continue;
    // Architectural commit is this call, after the commit wait and before
    // any response exists. A later link fault cannot roll it back.
    // A later Poison/error signal also cannot undo it (IHI0022L A16.1 does not
    // define undo; study invariant: commit_count / atomic_value never shrink).
    const std::string effect = w_.target.commit(tx);
    if (tx.ground.architectural_commit_count > 0 && tx.ground.commit_time_ns < 0) {
      tx.ground.commit_time_ns = w_.now_ns();
      tx.ground.commit_times_ns.push_back(tx.ground.commit_time_ns);
    }
    w_.note(tx, Phase::ArchitecturalCommit, "Target",
            tx.axi5.enabled && tx.axi5.form == Axi5AtomicForm::AtomicStore
                ? "ATOMICSTORE ARCHITECTURAL EFFECT"
                : "ARCHITECTURAL COMMIT");
    w_.log(TraceLevel::Transactions, "commit", "Target", &tx, effect);
    // Architectural effect / poison disposition at the target are ground truth.
    // They must not enter InitiatorView, classify(), retry policy, or observer
    // A–D projections (prefix GT: is evaluation-only).
    if (tx.axi5.enabled && tx.axi5.atomic_effect_applied) {
      w_.record_observer_event(tx, "GT:AXI5_ATOMIC_ADD_EFFECT");
    }
    if (tx.axi5.enabled && tx.axi5.poison_observed_at_target) {
      w_.record_observer_event(tx, tx.axi5.poison_rejected ? "GT:AXI5_POISON_REJECT"
                                                           : "GT:AXI5_POISON_MARKED");
    }

    sc_core::wait(w_.cycles(w_.cfg.target_response_cycles));
    if (tx.visible.classified && !w_.remote_may_continue(tx)) continue;
    w_.note(tx, Phase::ResponseCreated, "Target", "response created");

    // [MODEL] Return-path AoU pack uses the same pack delay as the request.
    sc_core::wait(w_.cycles(w_.cfg.aou_pack_cycles));
    if (tx.visible.classified && !w_.remote_may_continue(tx)) continue;
    w_.note(tx, Phase::ResponseQueued, "AoU", "response queued");

    // AtomicStore (IHI0022L A6.4.1 / A6.4.7 Fig A6.4): single B response, no R.
    // Ordinary reads: one response flit per beat. Ordinary writes: one B.
    const bool atomic_store =
        tx.axi5.enabled && tx.axi5.form == Axi5AtomicForm::AtomicStore;
    if (atomic_store) {
      Flit response;
      response.logical_id = tx.logical_id;
      response.slot = slot;
      response.guest_id = tx.guest_id;
      response.axi_id = tx.axi_id;
      response.role = FlitRole::Response;
      response.bridge_token = tx.bridge.token;
      response.bridge_generation = tx.bridge.generation;
      response.resp = tx.axi5.intended_bresp;
      response.beat_index = 0;
      response.rlast = false;
      // Trace echo (IHI0022L A12.3 recommendation): AWTRACE → BTRACE.
      if (w_.cfg.axi5_trace_signals_present && tx.axi5.awtrace) {
        response.btrace = true;
        tx.axi5.btrace = true;
        tx.axi5.trace_exported = true;
        w_.record_observer_event(tx, "DISP:AXI5_BTRACE");
      }
      response.awatop = tx.axi5.awatop;
      tx.axi5.write_response_created = true;
      tx.axi5.read_response_created = false;
      tx.ground.response_created = true;
      w_.send_q[1].push(response);
    } else {
      const int beats = tx.is_write ? 1 : std::max(1, tx.proto.burst_len);
      for (int b = 0; b < beats; ++b) {
        Flit response;
        response.logical_id = tx.logical_id;
        response.slot = slot;
        response.guest_id = tx.guest_id;
        response.axi_id = tx.axi_id;
        response.role = FlitRole::Response;
        response.bridge_token = tx.bridge.token;
        response.bridge_generation = tx.bridge.generation;
        response.resp = tx.axi5.enabled ? tx.axi5.intended_bresp : AxiResp::Okay;
        response.beat_index = b;
        response.rlast = (!tx.is_write && b + 1 == beats);
        if (w_.cfg.axi5_trace_signals_present && tx.axi5.awtrace && tx.is_write) {
          response.btrace = true;
          tx.axi5.btrace = true;
          tx.axi5.trace_exported = true;
        }
        if (tx.is_write) tx.axi5.write_response_created = true;
        else tx.axi5.read_response_created = true;
        w_.send_q[1].push(response);
      }
      tx.ground.response_created = true;
    }
    w_.sample("tx_b", w_.send_q[1].size());
  }
}

}  // namespace ucie
