#include "common/config.hpp"
#include "experiments/scenario.hpp"
#include "sim/modules.hpp"

#include <systemc>

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void usage() {
  std::cerr << "usage: ucie_sim --experiment <name> [--config file] [--seed N] [--output dir]\n"
            << "experiments:\n"
            << "  baseline_unknown\n"
            << "  precommit\n"
            << "  blind_retry\n"
            << "  observational_equivalence\n"
            << "  multi_guest\n"
            << "  retry_storm\n"
            << "  guest_a_storm\n"
            << "  shared_quiet\n"
            << "  guest_b_isolated\n"
            << "  guest_b_storm\n"
            << "  mit_safety_priority\n"
            << "  mit_per_guest_slots\n"
            << "  mit_replay_budget\n"
            << "  mit_reserved_bw\n"
            << "  mit_virtual_channels\n"
            << "  priority_sideband\n"
            << "  fast_throttle\n"
            << "  emergency_shutdown\n"
            << "  degraded_link\n"
            << "  random_campaign\n"
            << "  ref_retry_buffer\n"
            << "  ref_fifo_occupancy\n"
            << "  ref_credit_behavior\n"
            << "  ref_sustained_replay\n"
            << "  ref_commit_vs_ack\n"
            << "  ref_recovery_matrix\n"
            << "  ref_diagnostics\n"
            << "  ref_sideband\n"
            << "  ref_l1\n"
            << "  ref_throughput_tradeoff\n"
            << "  ref_cross_client\n";
}

}  // namespace

int sc_main(int argc, char* argv[]) {
  std::string experiment_name = "baseline_unknown";
  std::string config_path;
  std::string output;
  std::uint64_t seed = 1;
  bool seed_set = false;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto need = [&](const char* flag) {
      if (i + 1 >= argc) {
        std::cerr << "missing value for " << flag << "\n";
        std::exit(2);
      }
      return std::string(argv[++i]);
    };
    if (arg == "--help" || arg == "-h") {
      usage();
      return 0;
    } else if (arg == "--experiment") {
      experiment_name = need("--experiment");
    } else if (arg == "--config") {
      config_path = need("--config");
    } else if (arg == "--output") {
      output = need("--output");
    } else if (arg == "--seed") {
      seed = static_cast<std::uint64_t>(std::stoull(need("--seed")));
      seed_set = true;
    } else {
      std::cerr << "unknown argument " << arg << "\n";
      usage();
      return 2;
    }
  }

  ucie::Experiment experiment;
  if (!ucie::experiment_from_name(experiment_name, experiment)) {
    std::cerr << "unknown experiment " << experiment_name << "\n";
    usage();
    return 2;
  }

  // Before any sc_time object. Handshake and buffer waits are multiples of
  // this resolution. Recovery timers are sc_time values, not PHY cycles.
  sc_core::sc_set_time_resolution(1, sc_core::SC_PS);

  ucie::Config config = ucie::Config::defaults();
  if (!config_path.empty()) config.overlay_yaml(config_path);
  if (seed_set) config.seed = seed;
  if (output.empty()) output = std::string("timed_results/") + experiment_name;

  ucie::Scenario scenario = ucie::make_scenario(experiment, config);
  ucie::ChipletSystem system("ucie", config, std::move(scenario), output);
  sc_core::sc_start();
  return system.result().pass ? 0 : 1;
}
