#include "sim/modules.hpp"

#include <algorithm>

namespace ucie {

TxnWatchdog::TxnWatchdog(sc_core::sc_module_name name, World& world) : sc_module(name), w_(world) {
  SC_THREAD(run);
}

void TxnWatchdog::run() {
  while (true) {
    if (!w_.cfg.txn_watchdog_enabled) {
      sc_core::wait(sc_core::sc_time(1, sc_core::SC_MS));
      continue;
    }
    const double now = w_.now_ns();
    double next_due = -1;
    for (auto& tx : w_.txns) {
      if (!tx.visible.axi_accepted) continue;
      if (tx.visible.classified || tx.visible.axi_completed_ok || tx.visible.axi_completed_err) continue;
      if (tx.bridge.local_closed) continue;
      if (tx.t_watchdog_arm_ns < 0) continue;
      if (!w_.cfg.txn_watchdog_during_recovery && w_.link.busy) continue;

      const double due = tx.t_watchdog_arm_ns + w_.cfg.txn_watchdog_ns;
      if (now + 1e-9 >= due) {
        tx.t_watchdog_fire_ns = now;
        tx.visible.txn_watchdog_fired = true;
        ++tx.visible.txn_timeout_count;
        ++w_.txn_watchdog_fires;
        ++w_.txn_timeout_streak;
        w_.log(TraceLevel::Transactions, "txn_watchdog_fire", "WDT", &tx,
               "[PROJECT WDT-03] transaction timeout id=" + std::to_string(tx.axi_id) +
                   " addr=0x" + std::to_string(tx.address) + " dir=" + (tx.is_write ? "W" : "R") +
                   " cause=txn_watchdog");
        w_.record_error("txn_watchdog", &tx, "AXI transaction watchdog timeout");
        w_.local_slverr(tx, RespSource::Watchdog,
                        "[PROJECT WDT-03] local SLVERR after transaction watchdog");
        // ERR-07 is armed in AxiMaster after the SLVERR handshake completes.
        if (w_.txn_timeout_streak >= w_.cfg.txn_watchdog_escalate_after) {
          w_.txn_watchdog_escalated = true;
          w_.log(TraceLevel::Summary, "txn_watchdog_escalate", "WDT", &tx,
                 "[PROJECT CONFIG] escalate after repeated transaction timeouts");
          // Escalation is recorded; link recovery request is a policy choice.
          w_.request(RecoveryCmd::LinkError);
          w_.txn_timeout_streak = 0;
        }
        continue;
      }
      if (next_due < 0 || due < next_due) next_due = due;
    }
    if (next_due < 0) {
      sc_core::wait(sc_core::sc_time(std::max(1.0, w_.cfg.txn_watchdog_ns / 10.0), sc_core::SC_NS),
                    w_.progress | w_.link.state_ev);
    } else {
      const double wait_ns = std::max(1.0, next_due - w_.now_ns());
      sc_core::wait(sc_core::sc_time(wait_ns, sc_core::SC_NS), w_.progress | w_.link.state_ev);
    }
  }
}

}  // namespace ucie
