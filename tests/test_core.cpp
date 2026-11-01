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
    EXPECT(issues[0].false_safe_retry && issues[0].at_most_once && issues[0].completion_inconsistent);
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

  if (fails) {
    std::cerr << fails << " assertion(s) failed\n";
    return 1;
  }
  std::cout << "core tests passed\n";
  return 0;
}
