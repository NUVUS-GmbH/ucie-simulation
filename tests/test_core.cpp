#include "axi/target_model.hpp"
#include "common/arbitration.hpp"
#include "common/transaction.hpp"
#include "ucie/credit_manager.hpp"
#include "ucie/retry_buffer.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int fails = 0;

void expect(bool cond, const char* expr, int line) {
  if (!cond) {
    std::cerr << "FAIL " << line << " " << expr << "\n";
    ++fails;
  }
}

#define EXPECT(c) expect((c), #c, __LINE__)

ucie::Transaction accepted_write() {
  ucie::Transaction tx;
  tx.logical_id = 1;
  tx.is_write = true;
  tx.idempotent = false;
  tx.op = ucie::OpKind::IncrementCounter;
  tx.visible.axi_accepted = true;
  return tx;
}

}  // namespace

int main() {
  using namespace ucie;

  {
    auto tx = accepted_write();
    tx.visible.axi_completed_ok = true;
    EXPECT(classify(view_for(ObserverProfile::A_AxiOnly, tx)) == Fate::Completed);
    tx.ground.architectural_commit_count = 99;
    EXPECT(classify(view_for(ObserverProfile::A_AxiOnly, tx)) == Fate::Completed);
  }
  {
    auto tx = accepted_write();
    EXPECT(classify(view_for(ObserverProfile::A_AxiOnly, tx)) == Fate::Unknown);
    EXPECT(classify(view_for(ObserverProfile::B_AxiFdi, tx)) == Fate::Unknown);
    tx.visible.fdi_link_down = true;
    tx.visible.linkerror_status = true;
    tx.visible.crc_status = true;
    EXPECT(classify(view_for(ObserverProfile::C_AxiFdiUcieStatus, tx)) == Fate::Unknown);
  }
  {
    // SLVERR completes AXI but does not imply NOT_COMPLETED or COMPLETED.
    auto tx = accepted_write();
    tx.visible.axi_completed_err = true;
    tx.visible.axi_resp = AxiResp::Slverr;
    tx.visible.axi_resp_source = RespSource::Bridge;
    EXPECT(classify(view_for(ObserverProfile::A_AxiOnly, tx)) == Fate::Unknown);
    EXPECT(classify(view_for(ObserverProfile::B_AxiFdi, tx)) == Fate::Unknown);
  }
  {
    // Diagnostic rule: local SLVERR ∧ never handed → NOT_COMPLETED for C.
    // Missing ACK alone must remain UNKNOWN (delivery can precede ACK).
    auto tx = accepted_write();
    tx.visible.axi_completed_err = true;
    tx.visible.axi_resp = AxiResp::Slverr;
    tx.visible.axi_resp_source = RespSource::Bridge;
    tx.visible.use_diag_fields = true;
    tx.visible.diag_bridge_local_slverr = true;
    tx.visible.diag_never_handed = true;
    EXPECT(classify(view_for(ObserverProfile::C_AxiFdiUcieStatus, tx)) == Fate::NotCompleted);
    tx.visible.diag_never_handed = false;
    tx.visible.diag_missing_ack = true;
    EXPECT(classify(view_for(ObserverProfile::C_AxiFdiUcieStatus, tx)) == Fate::Unknown);
    tx.visible.use_diag_fields = false;
    tx.visible.diag_never_handed = true;
    EXPECT(classify(view_for(ObserverProfile::C_AxiFdiUcieStatus, tx)) == Fate::Unknown);
  }
  {
    Transaction tx;
    tx.is_write = true;
    EXPECT(classify(view_for(ObserverProfile::A_AxiOnly, tx)) == Fate::NotCompleted);
  }
  {
    auto tx = accepted_write();
    tx.visible.handed_to_ucie = true;
    tx.visible.data_attempted = true;
    tx.visible.request_nak_only = true;
    tx.visible.data_nak_only = true;
    EXPECT(classify(view_for(ObserverProfile::D_AxiHypervisorMetadata, tx)) == Fate::NotCompleted);
    tx.visible.request_ack = true;
    tx.visible.request_nak_only = false;
    tx.visible.data_ack = true;
    tx.visible.data_nak_only = false;
    EXPECT(classify(view_for(ObserverProfile::D_AxiHypervisorMetadata, tx)) == Fate::Unknown);
  }
  {
    auto tx = accepted_write();
    tx.visible.explicit_unknown = true;
    EXPECT(classify(view_for(ObserverProfile::A_AxiOnly, tx)) == Fate::Unknown);
  }
  {
    auto tx = accepted_write();
    tx.visible.status_has_commit_bit = true;
    bool threw = false;
    try {
      classify(view_for(ObserverProfile::C_AxiFdiUcieStatus, tx));
    } catch (const std::logic_error&) {
      threw = true;
    }
    EXPECT(threw);
  }

  {
    auto tx = accepted_write();
    tx.visible.axi_completed_ok = false;
    tx.ground.architectural_commit_count = 1;
    tx.visible.fate_a = Fate::NotCompleted;
    auto issues = check_transaction_invariants({tx});
    EXPECT(!issues.empty() && issues[0].false_safe_retry);
  }
  {
    auto tx = accepted_write();
    tx.software_retry_count = 1;
    tx.ground.commit_at_reissue = 1;
    tx.ground.architectural_commit_count = 2;
    tx.visible.axi_completed_ok = true;
    tx.visible.fate_a = Fate::Completed;
    auto issues = check_transaction_invariants({tx});
    EXPECT(!issues.empty());
    EXPECT(issues[0].false_safe_retry && issues[0].at_most_once && !issues[0].completion_inconsistent);
  }

  {
    CreditManager credits(4);
    EXPECT(credits.try_acquire());
    EXPECT(credits.available() == 3 && credits.held() == 1);
    credits.on_return();
    EXPECT(credits.audit().empty());
    EXPECT(credits.available() == 4 && credits.held() == 0);
    credits.inject_create();
    EXPECT(credits.audit().empty());
    CreditManager leaked(2);
    leaked.try_acquire();
    leaked.on_return();
    leaked.on_return();
    EXPECT(!leaked.audit().empty());
  }

  {
    RetryBuffer buffer(2);
    Flit flit;
    flit.seq = 1;
    EXPECT(buffer.push(flit));
    flit.seq = 2;
    EXPECT(buffer.push(flit));
    flit.seq = 3;
    EXPECT(!buffer.push(flit));
    EXPECT(buffer.max_occupancy() == 2);
    EXPECT(buffer.mark_nak(1));
    auto nak = buffer.take_nak();
    EXPECT(nak.has_value() && nak->seq == 1);
    EXPECT(buffer.ack(1));
    EXPECT(buffer.size() == 1);
    buffer.flush();
    EXPECT(buffer.empty());
  }

  {
    Arbiter rr(ArbPolicy::RoundRobin);
    std::vector<int> ready{0, 1};
    std::vector<int> prio{0, 0};
    std::vector<int> weight{1, 1};
    int first = ready[rr.select(ready, prio, weight)];
    int second = ready[rr.select(ready, prio, weight)];
    EXPECT(first != second);
    Arbiter strict(ArbPolicy::StrictPriority);
    prio = {1, 5, 0};
    ready = {0, 1, 2};
    EXPECT(ready[strict.select(ready, prio, weight)] == 1);
    EXPECT(ready[strict.select(ready, prio, weight)] == 1);
  }

  {
    TargetModel target;
    target.counter = 7;
    Transaction tx;
    tx.op = OpKind::IncrementCounter;
    tx.is_write = true;
    auto text = target.commit(tx);
    EXPECT(target.counter == 8);
    EXPECT(tx.ground.architectural_commit_count == 1);
    EXPECT(text.find("7") != std::string::npos && text.find("8") != std::string::npos);
    target.commit(tx);
    EXPECT(tx.ground.architectural_commit_count == 2);
    EXPECT(target.counter == 9);
  }

  // AXIS-P1: SINK-IMM commits per beat; SINK-PKT separates reception / process / effect.
  {
    TargetModel target;
    target.counter = 0;
    Transaction tx;
    tx.axis.enabled = true;
    tx.axis.sink = AxisSinkKind::Imm;
    tx.axis.beat_count = 2;
    tx.axis.data_width_bytes = 16;
    tx.axis.last_beat_valid_bytes = 4;
    for (int b = 0; b < 2; ++b) {
      AxisBeatOffer o;
      o.beat_index = b;
      o.tdata = static_cast<std::uint64_t>(0x10 + b);
      o.tkeep = tx.axis.keep_for_beat(b);
      o.tlast = (b == 1);
      o.valid_bytes = tx.axis.valid_bytes_for_beat(b);
      tx.axis.offers.push_back(o);
    }
    target.axis_commit_imm_beat(tx, 0, 1.0);
    EXPECT(tx.axis.app_effects == 1);
    EXPECT(tx.ground.architectural_commit_count == 1);
    EXPECT(tx.axis.bytes_committed == 16);
    target.axis_commit_imm_beat(tx, 1, 2.0);
    EXPECT(tx.axis.app_effects == 2);
    EXPECT(tx.axis.bytes_committed == 20);
    EXPECT(target.counter == 20);
  }
  {
    TargetModel target;
    Transaction tx;
    tx.axis.enabled = true;
    tx.axis.sink = AxisSinkKind::Pkt;
    tx.axis.beat_count = 2;
    tx.axis.remote_packet_complete = true;
    tx.axis.pkt_buffer = {1, 2, 3, 4};
    // Complete reception alone must not commit.
    EXPECT(tx.axis.app_effects == 0);
    EXPECT(tx.ground.architectural_commit_count == 0);
    auto refused = target.axis_commit_pkt_effect(tx, 1.0);
    EXPECT(refused.find("processing not started") != std::string::npos);
    EXPECT(tx.axis.app_effects == 0);
    tx.axis.processing_started = true;
    auto ok = target.axis_commit_pkt_effect(tx, 2.0);
    EXPECT(tx.axis.app_effects == 1);
    EXPECT(tx.ground.architectural_commit_count == 1);
    EXPECT(target.axis_packets_committed == 1);
    EXPECT(ok.find("AXIS-PKT") != std::string::npos);
  }
  {
    // Continuous_Packets keep mask: non-TLAST all bytes valid.
    AxisRecord axis;
    axis.beat_count = 2;
    axis.data_width_bytes = 16;
    axis.last_beat_valid_bytes = 3;
    EXPECT(axis.keep_for_beat(0) == 0xffff);
    EXPECT(axis.valid_bytes_for_beat(1) == 3);
    EXPECT(axis.keep_for_beat(1) == 0x7);
  }
  {
    // Optional dup-suppress by packet_op_id (ADDITIONAL MECHANISM).
    TargetModel target;
    target.axis_dup_suppress = true;
    target.axis_dup_capacity = 2;
    target.axis_dup_lifetime_ns = 0;
    Transaction tx;
    tx.axis.enabled = true;
    tx.axis.sink = AxisSinkKind::Pkt;
    tx.axis.packet_id_enabled = true;
    tx.axis.packet_op_id = 99;
    tx.axis.processing_started = true;
    tx.axis.pkt_buffer = {9};
    target.axis_commit_pkt_effect(tx, 10.0);
    EXPECT(tx.ground.architectural_commit_count == 1);
    Transaction again = tx;
    again.ground.architectural_commit_count = 0;
    again.axis.app_effects = 0;
    again.axis.dup_suppress_hit = false;
    again.axis.processing_started = true;
    again.axis.pkt_buffer = {9};
    target.axis_commit_pkt_effect(again, 11.0);
    EXPECT(again.axis.dup_suppress_hit);
    EXPECT(again.ground.architectural_commit_count == 0);
    EXPECT(target.axis_duplicates_suppressed == 1);
  }

  // AXI5-MM-P1: AtomicStore(ADD) — no prior-value return; double ADD visible in memory.
  {
    TargetModel target;
    target.atomic_value = 10;
    Transaction tx;
    tx.axi5.enabled = true;
    tx.axi5.form = Axi5AtomicForm::AtomicStore;
    tx.axi5.atomic_op = Axi5AtomicOp::Add;
    tx.axi5.awatop = kAwatopAtomicStoreAddLe;
    tx.axi5.operand = 5;
    tx.data = 5;
    tx.address = 0xA000;
    tx.is_write = true;
    auto line = target.commit_atomic_store(tx);
    EXPECT(target.atomic_value == 15);
    EXPECT(target.atomic_ops_applied == 1);
    EXPECT(tx.ground.architectural_commit_count == 1);
    EXPECT(tx.axi5.value_before == 10 && tx.axi5.value_after == 15);
    EXPECT(!tx.axi5.read_response_created);
    EXPECT(line.find("AtomicStore(ADD)") != std::string::npos);
    // Blind-retry style second application (same logical op, new attempt).
    Transaction retry = tx;
    retry.ground.architectural_commit_count = 1;  // prior attempt already counted on same logical tx in sim;
    // Unit test uses a fresh count path via commit_atomic_store again:
    retry.ground.architectural_commit_count = 0;
    retry.axi5.atomic_effects = 0;
    retry.axi5.transfer_attempt = 1;
    target.commit_atomic_store(retry);
    EXPECT(target.atomic_value == 20);
    EXPECT(target.atomic_ops_applied == 2);
  }
  // Poison reject: no commit, no undo; AcceptMarked commits.
  {
    TargetModel target;
    target.atomic_value = 10;
    target.poison_policy = PoisonTargetPolicy::RejectWithoutCommitSlverr;
    Transaction tx;
    tx.axi5.enabled = true;
    tx.axi5.form = Axi5AtomicForm::AtomicStore;
    tx.axi5.atomic_op = Axi5AtomicOp::Add;
    tx.axi5.awatop = kAwatopAtomicStoreAddLe;
    tx.axi5.wpoison = true;
    tx.axi5.operand = 5;
    tx.data = 5;
    target.commit_atomic_store(tx);
    EXPECT(tx.axi5.poison_rejected);
    EXPECT(tx.ground.architectural_commit_count == 0);
    EXPECT(target.atomic_value == 10);
    EXPECT(tx.axi5.intended_bresp == AxiResp::Slverr);
  }
  {
    TargetModel target;
    target.atomic_value = 10;
    target.poison_policy = PoisonTargetPolicy::RejectWithoutCommitOkay;
    Transaction tx;
    tx.axi5.enabled = true;
    tx.axi5.form = Axi5AtomicForm::AtomicStore;
    tx.axi5.atomic_op = Axi5AtomicOp::Add;
    tx.axi5.awatop = kAwatopAtomicStoreAddLe;
    tx.axi5.wpoison = true;
    tx.axi5.operand = 5;
    tx.data = 5;
    target.commit_atomic_store(tx);
    EXPECT(tx.axi5.poison_rejected);
    EXPECT(target.atomic_value == 10);
    EXPECT(tx.axi5.intended_bresp == AxiResp::Okay);
  }
  {
    TargetModel target;
    target.atomic_value = 10;
    target.poison_policy = PoisonTargetPolicy::AcceptMarkedOkay;
    Transaction tx;
    tx.axi5.enabled = true;
    tx.axi5.form = Axi5AtomicForm::AtomicStore;
    tx.axi5.atomic_op = Axi5AtomicOp::Add;
    tx.axi5.awatop = kAwatopAtomicStoreAddLe;
    tx.axi5.wpoison = true;
    tx.axi5.operand = 5;
    tx.data = 5;
    target.commit_atomic_store(tx);
    EXPECT(tx.axi5.poison_accepted_marked);
    EXPECT(target.atomic_value == 15);
    EXPECT(tx.ground.architectural_commit_count == 1);
  }
  // Poison after commit must not undo.
  {
    TargetModel target;
    target.atomic_value = 10;
    target.poison_policy = PoisonTargetPolicy::RejectWithoutCommitSlverr;
    Transaction clean;
    clean.axi5.enabled = true;
    clean.axi5.form = Axi5AtomicForm::AtomicStore;
    clean.axi5.atomic_op = Axi5AtomicOp::Add;
    clean.axi5.awatop = kAwatopAtomicStoreAddLe;
    clean.axi5.operand = 5;
    clean.data = 5;
    target.commit_atomic_store(clean);
    EXPECT(target.atomic_value == 15);
    Transaction poison;
    poison.axi5.enabled = true;
    poison.axi5.form = Axi5AtomicForm::AtomicStore;
    poison.axi5.atomic_op = Axi5AtomicOp::Add;
    poison.axi5.awatop = kAwatopAtomicStoreAddLe;
    poison.axi5.wpoison = true;
    poison.axi5.operand = -5;
    poison.data = -5;
    target.commit_atomic_store(poison);
    EXPECT(target.atomic_value == 15);
    EXPECT(target.axi5_commit_watermark >= 1);
  }
  // Trace must not appear in InitiatorView / classify inputs.
  {
    Transaction tx;
    tx.is_write = true;
    tx.visible.axi_accepted = true;
    tx.visible.axi_completed_err = true;
    tx.visible.axi_resp = AxiResp::Slverr;
    tx.visible.handed_to_ucie = true;
    tx.visible.data_attempted = true;
    tx.axi5.enabled = true;
    tx.axi5.awtrace = true;
    tx.axi5.btrace = true;
    tx.axi5.trace_used_in_classify = false;
    Fate with_trace = classify(view_for(ObserverProfile::A_AxiOnly, tx));
    Fate with_trace_d = classify(view_for(ObserverProfile::D_AxiHypervisorMetadata, tx));
    tx.axi5.awtrace = false;
    tx.axi5.btrace = false;
    Fate without_trace = classify(view_for(ObserverProfile::A_AxiOnly, tx));
    Fate without_trace_d = classify(view_for(ObserverProfile::D_AxiHypervisorMetadata, tx));
    EXPECT(with_trace == Fate::Unknown);
    EXPECT(with_trace == without_trace);
    EXPECT(with_trace_d == without_trace_d);
    EXPECT(!tx.axi5.trace_used_in_classify);
  }
  // AtomicLoad encoding refused in P1.
  {
    TargetModel target;
    Transaction tx;
    tx.axi5.enabled = true;
    tx.axi5.form = Axi5AtomicForm::AtomicLoad;
    tx.axi5.atomic_op = Axi5AtomicOp::Add;
    tx.axi5.awatop = kAwatopAtomicLoadAddLe;
    auto msg = target.commit_atomic_store(tx);
    EXPECT(tx.axi5.unsupported_form);
    EXPECT(target.atomic_ops_applied == 0);
    EXPECT(msg.find("only AtomicStore") != std::string::npos);
  }

  {
    Transaction tx;
    tx.visible.axi_accepted = true;
    tx.visible.axi_completed_ok = true;
    tx.axi5.enabled = true;
    tx.axi5.wpoison = true;
    for (auto profile : {ObserverProfile::A_AxiOnly, ObserverProfile::B_AxiFdi,
                         ObserverProfile::C_AxiFdiUcieStatus,
                         ObserverProfile::D_AxiHypervisorMetadata}) {
      EXPECT(classify(view_for(profile, tx)) == Fate::Completed);
      EXPECT(classify_effect(view_for(profile, tx)) == EffectKnowledge::Indeterminate);
      tx.ground.architectural_commit_count = 2; // changing GT cannot change knowledge
      EXPECT(classify_effect(view_for(profile, tx)) == EffectKnowledge::Indeterminate);
    }
    tx.axi5.wpoison = false;
    EXPECT(classify_effect(view_for(ObserverProfile::A_AxiOnly, tx)) == EffectKnowledge::SureEffect);
    tx.visible.knowledge_a = EffectKnowledge::SureEffect;
    auto issues = check_transaction_invariants({tx});
    EXPECT(issues.size() == 1 && issues[0].at_most_once && !issues[0].false_sure_effect);
    tx.visible.knowledge_a = EffectKnowledge::Indeterminate;
    tx.visible.knowledge_d = EffectKnowledge::SureEffect; // negative control: D must be checked
    tx.ground.architectural_commit_count = 0;
    issues = check_transaction_invariants({tx});
    EXPECT(issues.size() == 1 && issues[0].false_sure_effect);
    tx.visible.knowledge_d = EffectKnowledge::Indeterminate;
    EXPECT(check_transaction_invariants({tx}).empty());
    tx.visible.axi_completed_err = true;
    issues = check_transaction_invariants({tx});
    EXPECT(issues.size() == 1 && issues[0].completion_inconsistent);
  }

  if (fails) {
    std::cerr << fails << " assertion(s) failed\n";
    return 1;
  }
  std::cout << "core tests passed\n";
  return 0;
}
