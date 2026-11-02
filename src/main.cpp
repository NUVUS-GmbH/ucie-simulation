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
            << "  ref_cross_client\n"
            << "  fh_clean_success\n"
            << "  fh_loss_before_accept\n"
            << "  fh_commit_then_local_slverr\n"
            << "  fh_no_commit_local_slverr\n"
            << "  fh_watchdog_then_late_okay\n"
            << "  fh_timeout_then_remote_commit\n"
            << "  fh_blind_retry_after_slverr\n"
            << "  fh_no_retry_after_slverr\n"
            << "  fh_axi_id_reuse_late_resp\n"
            << "  fh_multi_outstanding_backpressure\n"
            << "  fh_read_burst_error_completion\n"
            << "  fh_race_resp_before_timeout\n"
            << "  fh_race_resp_simultaneous\n"
            << "  fh_race_resp_after_timeout\n"
            << "  fh_classifier_diag_compare\n"
            << "  fh_diag_ack_loss_after_delivery\n"
            << "  fh_observer_history_compare\n"
            << "  fh_safe_retry_cancel_remote\n"
            << "  fh_cancel_latency_race\n"
            << "  ax_clean_packet_imm\n"
            << "  ax_clean_packet_pkt\n"
            << "  ax_loss_mid_packet_imm\n"
            << "  ax_loss_mid_packet_pkt\n"
            << "  ax_loss_before_first_imm\n"
            << "  ax_loss_before_first_pkt\n"
            << "  ax_tlast_local_remote_loss\n"
            << "  ax_imm_partial_then_blind_retry\n"
            << "  ax_pkt_commit_then_local_timeout\n"
            << "  ax_ack_loss_after_delivery\n"
            << "  ax_observer_timed_vs_seq\n"
            << "  ax_safe_retry_cancel0\n"
            << "  ax_cancel_latency_race\n"
            << "  ax_backpressure_tready\n"
            << "  ax_continuous_tid_stable\n"
            << "  ax_appack_loss\n"
            << "  ax_reset_partial_packet\n"
            << "  ax_late_data_after_restart\n"
            << "  ax_multi_packet_inflight\n"
            << "  ax_packet_len_variants\n"
            << "  ax_replay_no_dup_effect\n"
            << "  ax_app_level_repeat_dup\n"
            << "  ax_dup_suppress\n"
            << "  ax_packet_id_reuse\n";
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
