#include "sim/modules.hpp"

namespace ucie {

UcieLink::UcieLink(sc_core::sc_module_name name, World& world) : sc_module(name), w_(world) {
  SC_THREAD(forward_ab);
  SC_THREAD(forward_ba);
}

void UcieLink::forward_ab() { forward(0); }
void UcieLink::forward_ba() { forward(1); }

void UcieLink::forward(int from) {
  const int to = 1 - from;
  while (true) {
    Flit flit;
    if (!w_.wire[from].pop(flit)) continue;
    // [MODEL] Propagation delay is a configured number of clock periods,
    // scaled if the link is throttled or degraded. It is not a trace-derived
    // flight time.
    auto extra = sc_core::sc_time(flit.extra_delay_ns > 0 ? flit.extra_delay_ns : 0, sc_core::SC_NS);
    sc_core::wait(w_.propagation() + extra);
    if (flit.drop) {
      w_.log(TraceLevel::Transactions, "drop", "Link", w_.find(flit.logical_id), "flit dropped");
      continue;
    }
    if (flit.link_epoch != w_.link.wire_gen) continue;
    const bool again = flit.duplicate;
    flit.duplicate = false;
    if (!w_.arrive[to].push(flit)) continue;
    if (again) w_.arrive[to].push(flit);
  }
}

}  // namespace ucie
