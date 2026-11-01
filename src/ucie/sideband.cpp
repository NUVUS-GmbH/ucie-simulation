#include "sim/modules.hpp"

#include <algorithm>

namespace ucie {

Sideband::Sideband(sc_core::sc_module_name name, World& world) : sc_module(name), w_(world) {
  SC_THREAD(run);
}

void Sideband::deliver(SidebandRecord msg, int mainband_ahead) {
  // [MODEL] A priority sideband message does not wait behind mainband flits.
  // A non-priority message waits one modeled flit time per queued mainband
  // flit. UCIe 3.0's priority sideband packet is not an AXI completion and
  // carries no commit bit (transaction-level study, Rev 3.0 §4.1.5.2).
  sc_core::sc_time latency(msg.priority ? w_.cfg.priority_sideband_ns : w_.cfg.sideband_message_ns, sc_core::SC_NS);
  if (!msg.priority && mainband_ahead > 0) latency += w_.serialization() * mainband_ahead;
  sc_core::wait(latency);
  msg.t_rx_ns = w_.now_ns();
  msg.latency_ns = msg.t_rx_ns - msg.t_gen_ns;
  if (msg.priority && w_.priority_sideband_ns < 0) w_.priority_sideband_ns = msg.latency_ns;
  if (!msg.priority && msg.reason == "management" && w_.normal_sideband_ns < 0) {
    w_.normal_sideband_ns = msg.latency_ns;
  }
  w_.sideband_log.push_back(msg);
  w_.sideband_peak = std::max(w_.sideband_peak, w_.sb_out.size() + 1);
  if (msg.latency_ns >= 0) {
    w_.sideband_max_latency_ns = std::max(w_.sideband_max_latency_ns, msg.latency_ns);
  }
  w_.sample("sideband", w_.sb_out.size());
  w_.log(TraceLevel::Transactions, "sideband_rx", "Sideband", nullptr,
         std::string(msg.priority ? "priority" : "normal") + " sideband delivered (" + msg.reason + ")");
  w_.sb_in.push(msg);
}

void Sideband::run() {
  if (w_.scenario.sideband_saturation) {
    while (w_.send_q[0].size() < 4 && w_.link.state != LinkState::LinkError) {
      sc_core::wait(w_.send_q[0].data_event() | w_.progress);
    }
    const int ahead = static_cast<int>(w_.send_q[0].size() + w_.replay_q[0].size());
    SidebandRecord fast;
    fast.priority = true;
    fast.reason = "safety";
    fast.t_gen_ns = w_.now_ns();
    SidebandRecord slow;
    slow.priority = false;
    slow.reason = "management";
    slow.t_gen_ns = fast.t_gen_ns;
    for (auto& tx : w_.txns) tx.visible.priority_sideband = true;
    deliver(fast, 0);
    deliver(slow, std::max(ahead, 1));
  }
  while (true) {
    SidebandRecord msg;
    if (!w_.sb_out.pop(msg)) continue;
    deliver(msg, 0);
  }
}

SafetyManager::SafetyManager(sc_core::sc_module_name name, World& world) : sc_module(name), w_(world) {
  SC_THREAD(run);
}

void SafetyManager::run() {
  while (true) {
    SidebandRecord msg;
    if (!w_.sb_in.pop(msg)) continue;
    if (w_.safe_notify_ns < 0) w_.safe_notify_ns = w_.now_ns();
    SafetyAction action = SafetyAction::Record;
    if (msg.reason == "LinkError") action = w_.scenario.on_link_error;
    else if (msg.reason == "EmergencyShutdown") action = w_.scenario.on_shutdown;
    else if (msg.reason == "deadline") action = w_.scenario.on_deadline;
    else if (msg.reason == "throttle") action = w_.scenario.on_thermal;
    w_.log(TraceLevel::Summary, "safety", "Safety", nullptr,
           "safety manager received " + msg.reason + " and records the configured action");
    if (action == SafetyAction::SafeState && !w_.safe_entered) {
      // [MODEL] One clock for the manager to enter the safe state after the
      // message. The policy is the scenario's, not a required UCIe behavior.
      sc_core::wait(w_.cycles(1));
      w_.safe_entered = true;
      w_.safe_entry_ns = w_.now_ns();
      w_.log(TraceLevel::Summary, "safe_state", "Safety", nullptr, "safe state entered");
    } else if (action == SafetyAction::Degraded) {
      w_.link.rate_scale *= 2;
    }
  }
}

FaultInjector::FaultInjector(sc_core::sc_module_name name, World& world) : sc_module(name), w_(world) {
  SC_THREAD(run);
}

void FaultInjector::run() {
  std::vector<FaultSpec*> timed;
  for (auto& spec : w_.scenario.faults) {
    if (spec.trigger == Trigger::AbsoluteTime) timed.push_back(&spec);
  }
  if (timed.empty()) return;
  sc_core::wait(w_.up_ev);
  for (auto* spec : timed) {
    if (spec->fired) continue;
    const double due = w_.origin_ns + spec->at_ns;
    if (w_.now_ns() < due) sc_core::wait(due - w_.now_ns(), sc_core::SC_NS);
    spec->fired = true;
    if (!w_.link.fault_time_set) {
      w_.link.fault_time_set = true;
      w_.link.fault_time_ns = w_.now_ns();
    }
    w_.log(TraceLevel::Transactions, "fault_time", "Injector", nullptr, "absolute-time fault");
    if (spec->kind == FaultKind::ForcedLinkError) w_.request(RecoveryCmd::LinkError);
    else if (spec->kind == FaultKind::EmergencyShutdown) w_.request(RecoveryCmd::EmergencyShutdown);
    else if (spec->kind == FaultKind::ForcedRetrain) w_.request(RecoveryCmd::Retrain);
    else if (spec->kind == FaultKind::ForcedLinkReset) w_.request(RecoveryCmd::LinkReset);
    else if (spec->kind == FaultKind::EnterL1) w_.request(RecoveryCmd::EnterL1);
    else if (spec->kind == FaultKind::ExitL1) w_.request(RecoveryCmd::ExitL1);
    else if (spec->kind == FaultKind::FastThrottle) w_.link.rate_scale *= spec->throttle_factor;
    else if (spec->kind == FaultKind::RateReduction && spec->bandwidth_factor > 0) {
      w_.link.rate_scale /= spec->bandwidth_factor;
    }
  }
}

}  // namespace ucie
