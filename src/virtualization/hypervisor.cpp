#include "sim/modules.hpp"

namespace ucie {

Hypervisor::Hypervisor(sc_core::sc_module_name name, World& world) : sc_module(name), w_(world) {
  SC_THREAD(run);
}

void Hypervisor::run() {
  sc_core::wait(w_.up_ev);
  w_.origin_ns = w_.now_ns();
  std::size_t next = 0;
  const auto& issues = w_.scenario.issues;
  while (true) {
    if (!w_.reissue.empty()) {
      const auto slot = w_.reissue.front();
      w_.reissue.pop_front();
      if (slot < w_.txns.size()) {
        auto& tx = w_.slot(slot);
        w_.log(TraceLevel::Transactions, "reissue", "Hypervisor", &tx,
               "automatic retry of the same logical transaction");
        w_.guest_q[tx.guest_id].push(slot);
      }
      continue;
    }
    if (next < issues.size()) {
      const auto& issue = issues[next];
      if (issue.wait_for_previous) {
        bool ready = !w_.link.busy && w_.reissue.empty();
        for (const auto& tx : w_.txns) {
          if (!tx.visible.classified) ready = false;
        }
        if (!ready) {
          sc_core::wait(w_.progress | w_.link.idle_ev);
          continue;
        }
      }
      const double due = w_.origin_ns + issue.delay_ns;
      if (w_.now_ns() + 1e-6 < due) {
        sc_core::wait(sc_core::sc_time(due - w_.now_ns(), sc_core::SC_NS), w_.progress);
        continue;
      }
      int limit = 8;
      if (issue.guest >= 0 && issue.guest < static_cast<int>(w_.scenario.guest_outstanding.size())) {
        limit = w_.scenario.guest_outstanding[issue.guest];
      }
      int active = 0;
      for (const auto& tx : w_.txns) {
        if (tx.guest_id == static_cast<std::uint32_t>(issue.guest) && !tx.visible.classified) ++active;
      }
      if (active >= limit) {
        sc_core::wait(w_.progress);
        continue;
      }
      if (issue.counter_reset >= 0) w_.target.counter = issue.counter_reset;
      const auto slot = w_.allocate(issue, static_cast<int>(next));
      ++next;
      if (issue.guest >= 0 && issue.guest < 3) w_.guest_q[issue.guest].push(slot);
      if (next == issues.size()) w_.all_issued = true;
      continue;
    }
    bool terminal = w_.all_issued && !w_.link.busy && w_.reissue.empty();
    for (const auto& tx : w_.txns) {
      if (!tx.visible.classified) terminal = false;
    }
    if (terminal) {
      w_.quiescent.notify(sc_core::SC_ZERO_TIME);
      return;
    }
    sc_core::wait(w_.progress | w_.link.idle_ev);
  }
}

}  // namespace ucie
