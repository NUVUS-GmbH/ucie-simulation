#pragma once

// SystemC modules for one initiator chiplet, one link, and one target chiplet.
//
// Cycle-quantized [MODEL] waits, aligned to clock_period_ns:
//   AXI handshake, AoU pack, arbitration, buffer, serialization, propagation,
//   target accept / commit / response, completion handshake.
// Timed abstractions, not claimed to be cycle-accurate:
//   replay timeout (spec flit count times the modeled flit time),
//   Retrain dwell, LinkReset dwell, LinkError residency, sideband latency,
//   reactivation. LinkError residency and the sideband timeout default to the
//   UCIe durations cited in config.hpp. Retrain and reactivation lengths are
//   configuration, not a universal specification number.

#include "sim/world.hpp"

#include <systemc>

#include <string>

namespace ucie {

class Hypervisor : public sc_core::sc_module {
 public:
  Hypervisor(sc_core::sc_module_name name, World& world);
 private:
  void run();
  World& w_;
};

class AxiInterconnect : public sc_core::sc_module {
 public:
  AxiInterconnect(sc_core::sc_module_name name, World& world);
 private:
  void run();
  World& w_;
};

class AxiMaster : public sc_core::sc_module {
 public:
  AxiMaster(sc_core::sc_module_name name, World& world);
 private:
  void accept();
  void complete();
  World& w_;
};

class AouEndpoint : public sc_core::sc_module {
 public:
  AouEndpoint(sc_core::sc_module_name name, World& world);
 private:
  void run();
  World& w_;
};

class UcieAdapter : public sc_core::sc_module {
 public:
  UcieAdapter(sc_core::sc_module_name name, World& world, int side);
 private:
  void tx();
  void rx();
  void monitor();
  World& w_;
  int side_;
};

class UcieLink : public sc_core::sc_module {
 public:
  UcieLink(sc_core::sc_module_name name, World& world);
 private:
  void forward_ab();
  void forward_ba();
  void forward(int from);
  World& w_;
};

class AxiTarget : public sc_core::sc_module {
 public:
  AxiTarget(sc_core::sc_module_name name, World& world);
 private:
  void run();
  World& w_;
};

class LinkStateMachine : public sc_core::sc_module {
 public:
  LinkStateMachine(sc_core::sc_module_name name, World& world);
 private:
  void run();
  void enter_link_error(const std::string& reason);
  void enter_link_reset();
  void do_retrain();
  void emergency();
  void reactivate();
  World& w_;
};

class Sideband : public sc_core::sc_module {
 public:
  Sideband(sc_core::sc_module_name name, World& world);
 private:
  void run();
  void deliver(SidebandRecord msg, int mainband_ahead);
  World& w_;
};

class SafetyManager : public sc_core::sc_module {
 public:
  SafetyManager(sc_core::sc_module_name name, World& world);
 private:
  void run();
  World& w_;
};

class FaultInjector : public sc_core::sc_module {
 public:
  FaultInjector(sc_core::sc_module_name name, World& world);
 private:
  void run();
  World& w_;
};

class ChipletSystem : public sc_core::sc_module {
 public:
  ChipletSystem(sc_core::sc_module_name name, Config config, Scenario scenario, std::string output);
  const RunResult& result() const { return result_; }

 private:
  World world_;
  Hypervisor hypervisor_;
  AxiInterconnect interconnect_;
  AxiMaster axi_;
  AouEndpoint aou_;
  UcieAdapter adapter_a_;
  UcieAdapter adapter_b_;
  UcieLink link_;
  AxiTarget target_;
  LinkStateMachine lsm_;
  Sideband sideband_;
  SafetyManager safety_;
  FaultInjector faults_;
  RunResult result_;
  std::string output_;
  void campaign();
  void watchdog();
};

}  // namespace ucie
