#include "sim/modules.hpp"

#include <sys/stat.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

namespace ucie {
namespace {

std::string json_escape(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  for (char ch : text) {
    switch (ch) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      default: out += ch; break;
    }
  }
  return out;
}

void mkdir_p(const std::string& path) {
  if (path.empty()) return;
  std::string cur;
  for (std::size_t i = 0; i < path.size(); ++i) {
    cur.push_back(path[i]);
    if (path[i] == '/' || i + 1 == path.size()) {
      if (cur == "/") continue;
      mkdir(cur.c_str(), 0755);
    }
  }
}

double pct(std::vector<double> values, double p) {
  if (values.empty()) return -1;
  std::sort(values.begin(), values.end());
  const double rank = p * static_cast<double>(values.size() - 1);
  const auto lo = static_cast<std::size_t>(std::floor(rank));
  const auto hi = static_cast<std::size_t>(std::ceil(rank));
  if (lo == hi) return values[lo];
  const double w = rank - static_cast<double>(lo);
  return values[lo] * (1.0 - w) + values[hi] * w;
}

void write_outputs(const World& world, const RunResult& result, const std::string& dir) {
  if (dir.empty()) return;
  mkdir_p(dir);
  std::ofstream csv(dir + "/trace.csv");
  csv << "time_ns,event,component,transaction_id,guest_id,state,details\n";
  csv.setf(std::ios::fixed);
  csv.precision(3);
  for (const auto& row : world.trace) {
    csv << row.time_ns << ',' << row.event << ',' << row.component << ',' << row.transaction_id << ','
        << row.guest_id << ',' << row.state << ',' << '"' << json_escape(row.details) << "\"\n";
  }
  std::ofstream text(dir + "/trace.txt");
  text.setf(std::ios::fixed);
  text.precision(3);
  for (const auto& row : world.trace) {
    text << row.time_ns << " ns  " << row.details << "\n";
  }
  std::ofstream occ(dir + "/occupancy.csv");
  occ << "time_ns,queue,occupancy\n";
  occ.setf(std::ios::fixed);
  occ.precision(3);
  for (const auto& sample : world.occupancy) occ << sample.time_ns << ',' << sample.name << ',' << sample.occupancy << "\n";
  std::ofstream states(dir + "/link_state.csv");
  states << "time_ns,state\n";
  states.setf(std::ios::fixed);
  states.precision(3);
  for (const auto& sample : world.states) states << sample.time_ns << ',' << sample.state << "\n";

  std::ofstream js(dir + "/result.json");
  js.setf(std::ios::fixed);
  js.precision(3);
  js << "{\n";
  js << "  \"experiment\": \"" << json_escape(result.experiment) << "\",\n";
  js << "  \"pass\": " << (result.pass ? "true" : "false") << ",\n";
  js << "  \"counter\": " << result.counter << ",\n";
  js << "  \"atomic_value\": " << result.atomic_value << ",\n";
  js << "  \"atomic_ops_applied\": " << result.atomic_ops_applied << ",\n";
  js << "  \"axi5_commit_watermark\": " << result.axi5_commit_watermark << ",\n";
  js << "  \"converged\": " << (result.converged ? "true" : "false") << ",\n";
  js << "  \"deadlock\": " << (result.deadlock ? "true" : "false") << ",\n";
  js << "  \"crc_failures\": " << result.crc_failures << ",\n";
  js << "  \"retrain_events\": " << result.retrain_events << ",\n";
  js << "  \"linkerror_events\": " << result.linkerror_events << ",\n";
  js << "  \"max_retry_occupancy\": " << result.max_retry_occupancy << ",\n";
  js << "  \"resources\": {\n";
  js << "    \"retry_buffer_peak\": " << result.resources.retry_buffer_peak << ",\n";
  js << "    \"tx_slot_hold_ns\": [" << result.resources.tx_slot_hold_ns[0] << ", "
     << result.resources.tx_slot_hold_ns[1] << ", " << result.resources.tx_slot_hold_ns[2] << "],\n";
  js << "    \"tx_slot_hold_replay_ns\": [" << result.resources.tx_slot_hold_replay_ns[0] << ", "
     << result.resources.tx_slot_hold_replay_ns[1] << ", " << result.resources.tx_slot_hold_replay_ns[2]
     << "],\n";
  js << "    \"guest_b_wait_arbitration_ns\": " << result.resources.guest_b_wait_arbitration_ns << ",\n";
  js << "    \"guest_b_wait_guest_a_normal_ns\": " << result.resources.guest_b_wait_guest_a_normal_ns << ",\n";
  js << "    \"guest_b_wait_guest_a_replay_ns\": " << result.resources.guest_b_wait_guest_a_replay_ns << ",\n";
  js << "    \"guest_b_wait_own_replay_ns\": " << result.resources.guest_b_wait_own_replay_ns << ",\n";
  js << "    \"guest_b_wait_credit_ns\": " << result.resources.guest_b_wait_credit_ns << ",\n";
  js << "    \"guest_b_wait_target_ns\": " << result.resources.guest_b_wait_target_ns << ",\n";
  js << "    \"guest_b_wait_response_ns\": " << result.resources.guest_b_wait_response_ns << ",\n";
  js << "    \"guest_b_wait_recovery_ns\": " << result.resources.guest_b_wait_recovery_ns << ",\n";
  js << "    \"guest_a_replay_attempts\": " << result.resources.guest_a_replay_attempts << ",\n";
  js << "    \"reserved_slots_granted\": " << result.resources.reserved_slots_granted << ",\n";
  js << "    \"yield_events\": " << result.resources.yield_events << "\n";
  js << "  },\n";
  js << "  \"fault_time_ns\": " << result.fault_time_ns << ",\n";
  js << "  \"detection_time_ns\": " << result.detection_time_ns << ",\n";
  js << "  \"link_error_time_ns\": " << result.link_error_time_ns << ",\n";
  js << "  \"recovery_time_ns\": " << result.recovery_time_ns << ",\n";
  js << "  \"safe_notify_time_ns\": " << result.safe_notify_time_ns << ",\n";
  js << "  \"safe_entry_time_ns\": " << result.safe_entry_time_ns << ",\n";
  js << "  \"priority_sideband_ns\": " << result.priority_sideband_ns << ",\n";
  js << "  \"normal_sideband_ns\": " << result.normal_sideband_ns << ",\n";
  js << "  \"observational_equivalence_abc\": " << (result.observational_equivalence_abc ? "true" : "false") << ",\n";
  js << "  \"observational_equivalence_d\": " << (result.observational_equivalence_d ? "true" : "false") << ",\n";
  js << "  \"tx_peak\": " << result.tx_peak << ",\n";
  js << "  \"rx_peak\": " << result.rx_peak << ",\n";
  js << "  \"sideband_peak\": " << result.sideband_peak << ",\n";
  js << "  \"sideband_max_latency_ns\": " << result.sideband_max_latency_ns << ",\n";
  js << "  \"credits_consumed\": " << result.credits_consumed << ",\n";
  js << "  \"credits_returned\": " << result.credits_returned << ",\n";
  js << "  \"l1_events\": " << result.l1_events << ",\n";
  js << "  \"error_log_entries\": " << result.error_log_entries << ",\n";
  js << "  \"txn_watchdog_fires\": " << world.txn_watchdog_fires << ",\n";
  js << "  \"late_responses_discarded\": " << world.late_responses_discarded << ",\n";
  js << "  \"txn_watchdog_escalated\": " << (world.txn_watchdog_escalated ? "true" : "false") << ",\n";
  js << "  \"recovery_policy\": \"" << static_cast<int>(world.scenario.policy) << "\",\n";
  js << "  \"fate_equal_abc\": "
     << ((result.transactions.empty() ||
          (result.transactions[0].visible.fate_a == result.transactions[0].visible.fate_b &&
           result.transactions[0].visible.fate_a == result.transactions[0].visible.fate_c))
             ? "true"
             : "false")
     << ",\n";
  js << "  \"single_inflight_slot\": " << (world.cfg.single_inflight_slot ? "true" : "false") << ",\n";
  js << "  \"retry_buffer_entries\": " << world.cfg.retry_buffer_entries << ",\n";
  js << "  \"tx_buffer_entries\": " << world.cfg.tx_buffer_entries << ",\n";
  js << "  \"rx_buffer_entries\": " << world.cfg.rx_buffer_entries << ",\n";
  js << "  \"credit_update_threshold\": " << world.cfg.credit_update_threshold << ",\n";
  js << "  \"early_credit_return\": " << (world.cfg.early_credit_return ? "true" : "false") << ",\n";
  js << "  \"crc_enabled\": " << (world.cfg.crc_enabled ? "true" : "false") << ",\n";
  js << "  \"retry_enabled\": " << (world.cfg.retry_enabled ? "true" : "false") << ",\n";
  js << "  \"ucie_revision\": \"" << json_escape(world.cfg.ucie_revision) << "\",\n";
  js << "  \"error_log\": [\n";
  for (std::size_t i = 0; i < world.error_log.size(); ++i) {
    const auto& e = world.error_log[i];
    js << "    {\"time_ns\": " << e.time_ns << ", \"kind\": \"" << json_escape(e.kind)
       << "\", \"retry_count\": " << e.retry_count << ", \"retry_occupancy\": " << e.retry_occupancy
       << ", \"tx_occupancy\": " << e.tx_occupancy << ", \"rx_occupancy\": " << e.rx_occupancy
       << ", \"credit_available\": " << e.credit_available << ", \"credit_held\": " << e.credit_held
       << ", \"transaction_id\": " << e.transaction_id << ", \"details\": \"" << json_escape(e.details)
       << "\"}";
    if (i + 1 != world.error_log.size()) js << ",";
    js << "\n";
  }
  js << "  ],\n";
  js << "  \"recovery_snapshots\": [\n";
  for (std::size_t i = 0; i < world.recovery_snapshots.size(); ++i) {
    const auto& s = world.recovery_snapshots[i];
    js << "    {\"transition\": \"" << json_escape(s.transition) << "\", \"time_ns\": " << s.time_ns
       << ", \"tx_fifo\": " << s.tx_fifo << ", \"rx_fifo\": " << s.rx_fifo
       << ", \"retry_buffer\": " << s.retry_buffer << ", \"credit_available\": " << s.credit_available
       << ", \"credit_held\": " << s.credit_held << ", \"outstanding_axi\": " << s.outstanding_axi
       << ", \"outstanding_responses\": " << s.outstanding_responses << ", \"classification\": \""
       << json_escape(s.classification) << "\"}";
    if (i + 1 != world.recovery_snapshots.size()) js << ",";
    js << "\n";
  }
  js << "  ],\n";
  js << "  \"transactions\": [\n";
  for (std::size_t i = 0; i < result.transactions.size(); ++i) {
    const auto& tx = result.transactions[i];
    js << "    {\"id\": " << tx.logical_id << ", \"guest\": " << tx.guest_id
       << ", \"op\": \"" << to_string(tx.op) << "\""
       << ", \"commit_count\": " << tx.ground.architectural_commit_count
       << ", \"fate_a\": \"" << to_string(tx.visible.fate_a) << "\""
       << ", \"fate_b\": \"" << to_string(tx.visible.fate_b) << "\""
       << ", \"fate_c\": \"" << to_string(tx.visible.fate_c) << "\""
       << ", \"fate_d\": \"" << to_string(tx.visible.fate_d) << "\""
       << ", \"completed_ok\": " << (tx.visible.axi_completed_ok ? "true" : "false")
       << ", \"completed_err\": " << (tx.visible.axi_completed_err ? "true" : "false")
       << ", \"axi_resp\": \"" << to_string(tx.visible.axi_resp) << "\""
       << ", \"resp_source\": \"" << to_string(tx.visible.axi_resp_source) << "\""
       << ", \"proto_state\": \"" << to_string(tx.proto.state) << "\""
       << ", \"knowledge_a\": \"" << to_string(tx.visible.knowledge_a) << "\""
       << ", \"knowledge_d\": \"" << to_string(tx.visible.knowledge_d) << "\""
       << ", \"late_responses_discarded\": " << tx.bridge.late_responses_discarded
       << ", \"txn_watchdog_fired\": " << (tx.visible.txn_watchdog_fired ? "true" : "false")
       << ", \"burst_len\": " << tx.proto.burst_len
       << ", \"beats_responded\": " << tx.proto.beats_responded
       << ", \"beat_handshake_count\": " << tx.proto.beat_handshake_count
       << ", \"resp_backpressured\": " << (tx.proto.resp_backpressured ? "true" : "false")
       << ", \"resp_ready_to_handshake_ns\": " << tx.proto.resp_ready_to_handshake_ns
       << ", \"deadline_violated\": " << (tx.visible.deadline_violated ? "true" : "false")
       << ", \"latency_ns\": " << tx.latency_ns()
       << ", \"ambiguity_window_ns\": " << tx.ambiguity_window_ns
       << ", \"ambiguity_open\": " << (tx.ambiguity_open ? "true" : "false")
       << ", \"retry_count\": " << tx.retry_count
       << ", \"replay_count\": " << tx.replay_count
       << ", \"software_retries\": " << tx.software_retry_count
       << ", \"completion_received\": " << (tx.completion_received ? "true" : "false")
       << ", \"axi_id\": " << tx.axi_id
       << ", \"bridge_token\": " << tx.bridge.token
       << ", \"bridge_generation\": " << tx.bridge.generation
       << ", \"completing_token\": " << tx.bridge.completing_token
       << ", \"completing_token_set\": " << (tx.bridge.completing_token_set ? "true" : "false")
       << ", \"allow_remote_continue\": " << (tx.bridge.allow_remote_continue ? "true" : "false")
       << ", \"fate_c_without_diag\": \"" << to_string(tx.visible.fate_c_without_diag) << "\""
       << ", \"fate_d_without_diag\": \"" << to_string(tx.visible.fate_d_without_diag) << "\""
       << ", \"diag_never_handed\": " << (tx.visible.diag_never_handed ? "true" : "false")
       << ", \"diag_missing_ack\": " << (tx.visible.diag_missing_ack ? "true" : "false")
       << ", \"cancel_requested\": " << (tx.bridge.cancel_requested ? "true" : "false")
       << ", \"cancel_effective_ns\": " << tx.bridge.cancel_effective_ns
       << ", \"axis_enabled\": " << (tx.axis.enabled ? "true" : "false")
       << ", \"axis_sink\": \"" << (tx.axis.sink == AxisSinkKind::Imm ? "IMM" : "PKT") << "\""
       << ", \"axis_local_beats\": " << tx.axis.local_beats_accepted
       << ", \"axis_local_bytes\": " << tx.axis.local_bytes_accepted
       << ", \"axis_remote_beats\": " << tx.axis.remote_beats_accepted
       << ", \"axis_remote_bytes\": " << tx.axis.remote_bytes_accepted
       << ", \"axis_remote_packet_complete\": " << (tx.axis.remote_packet_complete ? "true" : "false")
       << ", \"axis_processing_started\": " << (tx.axis.processing_started ? "true" : "false")
       << ", \"axis_app_effects\": " << tx.axis.app_effects
       << ", \"axis_bytes_committed\": " << tx.axis.bytes_committed
       << ", \"axis_local_tlast\": " << (tx.axis.local_tlast_accepted ? "true" : "false")
       << ", \"axis_protocol_error\": " << (tx.axis.protocol_error ? "true" : "false")
       << ", \"axis_app_ack_received\": " << (tx.axis.app_ack_received ? "true" : "false")
       << ", \"axis_packet_op_id\": " << tx.axis.packet_op_id
       << ", \"axis_dup_suppress_hit\": " << (tx.axis.dup_suppress_hit ? "true" : "false")
       << ", \"axi5_enabled\": " << (tx.axi5.enabled ? "true" : "false")
       << ", \"axi5_form\": \"" << to_string(tx.axi5.form) << "\""
       << ", \"axi5_atomic_op\": \"" << to_string(tx.axi5.atomic_op) << "\""
       << ", \"axi5_awatop\": " << static_cast<int>(tx.axi5.awatop)
       << ", \"axi5_wpoison\": " << (tx.axi5.wpoison ? "true" : "false")
       << ", \"axi5_poison_rejected\": " << (tx.axi5.poison_rejected ? "true" : "false")
       << ", \"axi5_poison_accepted_marked\": " << (tx.axi5.poison_accepted_marked ? "true" : "false")
       << ", \"axi5_awtrace\": " << (tx.axi5.awtrace ? "true" : "false")
       << ", \"axi5_btrace\": " << (tx.axi5.btrace ? "true" : "false")
       << ", \"axi5_trace_used_in_classify\": " << (tx.axi5.trace_used_in_classify ? "true" : "false")
       << ", \"axi5_atomic_effects\": " << tx.axi5.atomic_effects
       << ", \"axi5_value_before\": " << tx.axi5.value_before
       << ", \"axi5_value_after\": " << tx.axi5.value_after
       << ", \"axi5_operand\": " << tx.axi5.operand
       << ", \"axi5_transfer_attempt\": " << tx.axi5.transfer_attempt
       << ", \"axi5_logical_op_id\": " << tx.axi5.logical_op_id
       << ", \"axi5_write_response_created\": " << (tx.axi5.write_response_created ? "true" : "false")
       << ", \"axi5_read_response_created\": " << (tx.axi5.read_response_created ? "true" : "false")
       << ", \"axi5_unsupported_form\": " << (tx.axi5.unsupported_form ? "true" : "false")
       << ", \"axi5_poison_policy\": \"" << to_string(tx.axi5.poison_policy) << "\""
       << ", \"wait_arbitration_ns\": " << tx.wait_arbitration_ns
       << ", \"wait_guest_a_normal_ns\": " << tx.wait_guest_a_normal_ns
       << ", \"wait_guest_a_replay_ns\": " << tx.wait_guest_a_replay_ns
       << ", \"wait_own_replay_ns\": " << tx.wait_own_replay_ns
       << ", \"wait_credit_ns\": " << tx.wait_credit_ns
       << ", \"observer_trace_a\": [";
    for (std::size_t j = 0; j < tx.observer_trace_a.size(); ++j) {
      if (j) js << ", ";
      js << "\"" << json_escape(tx.observer_trace_a[j]) << "\"";
    }
    js << "], \"observer_trace_b\": [";
    for (std::size_t j = 0; j < tx.observer_trace_b.size(); ++j) {
      if (j) js << ", ";
      js << "\"" << json_escape(tx.observer_trace_b[j]) << "\"";
    }
    js << "], \"observer_trace_c\": [";
    for (std::size_t j = 0; j < tx.observer_trace_c.size(); ++j) {
      if (j) js << ", ";
      js << "\"" << json_escape(tx.observer_trace_c[j]) << "\"";
    }
    js << "], \"observer_trace_d\": [";
    for (std::size_t j = 0; j < tx.observer_trace_d.size(); ++j) {
      if (j) js << ", ";
      js << "\"" << json_escape(tx.observer_trace_d[j]) << "\"";
    }
    js << "]}";
    if (i + 1 != result.transactions.size()) js << ",";
    js << "\n";
  }
  js << "  ],\n";
  std::vector<double> windows;
  for (const auto& tx : result.transactions) {
    if (tx.ambiguity_window_ns >= 0) windows.push_back(tx.ambiguity_window_ns);
  }
  double sum = 0;
  for (double v : windows) sum += v;
  js << "  \"ambiguity_min_ns\": " << pct(windows, 0) << ",\n";
  js << "  \"ambiguity_max_ns\": " << pct(windows, 1) << ",\n";
  js << "  \"ambiguity_mean_ns\": " << (windows.empty() ? -1 : sum / static_cast<double>(windows.size())) << ",\n";
  js << "  \"ambiguity_p50_ns\": " << pct(windows, 0.50) << ",\n";
  js << "  \"ambiguity_p99_ns\": " << pct(windows, 0.99) << ",\n";
  js << "  \"failures\": [";
  for (std::size_t i = 0; i < result.failures.size(); ++i) {
    if (i) js << ", ";
    js << "\"" << json_escape(result.failures[i]) << "\"";
  }
  js << "],\n  \"violations\": [";
  for (std::size_t i = 0; i < result.violations.size(); ++i) {
    if (i) js << ", ";
    js << "\"" << json_escape(result.violations[i]) << "\"";
  }
  js << "]\n}\n";
}

}  // namespace

ChipletSystem::ChipletSystem(sc_core::sc_module_name name, Config config, Scenario scenario, std::string output)
    : sc_module(name),
      world_(config, std::move(scenario)),
      hypervisor_("hypervisor", world_),
      interconnect_("interconnect", world_),
      axi_("axi", world_),
      aou_("aou_a", world_),
      adapter_a_("adapter_a", world_, 0),
      adapter_b_("adapter_b", world_, 1),
      link_("link", world_),
      target_("target", world_),
      lsm_("lsm", world_),
      sideband_("sideband", world_),
      safety_("safety", world_),
      faults_("faults", world_),
      txn_watchdog_("txn_watchdog", world_),
      output_(std::move(output)) {
  SC_THREAD(campaign);
  SC_THREAD(watchdog);
}

void ChipletSystem::watchdog() {
  sc_core::wait(world_.cfg.sim_timeout_ns, sc_core::SC_NS);
  if (!world_.finished) {
    world_.deadlock = true;
    world_.log(TraceLevel::Errors, "watchdog", "Sim", nullptr, "simulation timeout");
    world_.watchdog_ev.notify(sc_core::SC_ZERO_TIME);
  }
}

void ChipletSystem::campaign() {
  sc_core::wait(world_.quiescent | world_.watchdog_ev);
  for (auto& tx : world_.txns) {
    if (!tx.visible.classified) world_.abandon(tx, world_.link.linkerror_events > 0, false);
  }
  // Drain any credits held below the update threshold so conservation audits
  // are not false-failed by an incomplete final batch. Pending frees live on
  // the RX side and credit the peer TX pool.
  for (int side = 0; side < 2; ++side) {
    if (const int n = world_.take_credit_update_force(side)) {
      for (int i = 0; i < n; ++i) world_.credit[1 - side].on_return();
    }
  }
  world_.audit_credits();
  if (world_.retry[0].max_occupancy() > world_.retry[0].capacity() ||
      world_.retry[1].max_occupancy() > world_.retry[1].capacity()) {
    world_.internal_errors.push_back("retry occupancy exceeded capacity");
  }

  result_.experiment = world_.scenario.name;
  result_.transactions = world_.txns;
  result_.counter = world_.target.counter;
  result_.atomic_value = world_.target.atomic_value;
  result_.atomic_ops_applied = world_.target.atomic_ops_applied;
  result_.axi5_commit_watermark = world_.target.axi5_commit_watermark;
  result_.fifo_depth = static_cast<int>(world_.target.fifo.size());
  result_.dma_starts = world_.target.dma_starts;
  result_.converged = world_.link.converged && world_.link.state == LinkState::Active;
  result_.deadlock = world_.deadlock;
  result_.finished = true;
  result_.crc_failures = world_.link.crc_failures;
  result_.retrain_events = world_.link.retrain_events;
  result_.linkreset_events = world_.link.linkreset_events;
  result_.linkerror_events = world_.link.linkerror_events;
  result_.transparent_retries = world_.link.transparent_retries;
  result_.max_retry_occupancy = std::max(world_.retry[0].max_occupancy(), world_.retry[1].max_occupancy());
  world_.fold_guest_b_waits();
  world_.resources.retry_buffer_peak =
      std::max(world_.resources.retry_buffer_peak, result_.max_retry_occupancy);
  result_.resources = world_.resources;
  result_.fault_time_ns = world_.link.fault_time_ns;
  result_.detection_time_ns = world_.link.detection_time_ns;
  result_.link_error_time_ns = world_.link.link_error_time_ns;
  result_.recovery_time_ns = world_.link.recovery_time_ns;
  result_.safe_notify_time_ns = world_.safe_notify_ns;
  result_.safe_entry_time_ns = world_.safe_entry_ns;
  result_.priority_sideband_ns = world_.priority_sideband_ns;
  result_.normal_sideband_ns = world_.normal_sideband_ns;
  result_.rate_scale = world_.link.rate_scale;
  result_.credit_problems = world_.credit_problems;
  result_.internal_errors = world_.internal_errors;
  result_.sideband = world_.sideband_log;
  result_.tx_peak = std::max(world_.tx_peak[0], world_.tx_peak[1]);
  result_.rx_peak = std::max(world_.rx_peak[0], world_.rx_peak[1]);
  result_.sideband_peak = world_.sideband_peak;
  result_.sideband_max_latency_ns = world_.sideband_max_latency_ns;
  result_.credits_consumed = world_.credits_consumed[0] + world_.credits_consumed[1];
  result_.credits_returned = world_.credits_returned[0] + world_.credits_returned[1];
  result_.l1_events = world_.l1_events;
  result_.error_log_entries = static_cast<int>(world_.error_log.size());
  result_.txn_watchdog_fires = world_.txn_watchdog_fires;
  result_.late_responses_discarded = world_.late_responses_discarded;
  result_.txn_watchdog_escalated = world_.txn_watchdog_escalated;
  evaluate(world_.scenario, result_);
  write_outputs(world_, result_, output_);

  std::cout << "experiment: " << result_.experiment << "\n";
  std::cout << "counter: " << result_.counter << "\n";
  std::cout << "link_error_events: " << result_.linkerror_events << "\n";
  std::cout << "retrain_events: " << result_.retrain_events << "\n";
  std::cout << "crc_failures: " << result_.crc_failures << "\n";
  for (const auto& tx : result_.transactions) {
    std::cout << "T" << tx.logical_id << " guest " << tx.guest_id << " commit " << tx.ground.architectural_commit_count
              << " A " << to_string(tx.visible.fate_a) << " D " << to_string(tx.visible.fate_d)
              << " resp " << to_string(tx.visible.axi_resp) << "/" << to_string(tx.visible.axi_resp_source)
              << " proto " << to_string(tx.proto.state) << "\n";
  }
  if (!result_.violations.empty()) {
    std::cout << "violations:";
    for (const auto& line : result_.violations) std::cout << " [" << line << "]";
    std::cout << "\n";
  }
  if (result_.observational_equivalence_abc) {
    std::cout << "OBSERVATIONAL_EQUIVALENCE = true\n";
  }
  if (result_.pass) std::cout << "PASS\n";
  else {
    std::cout << "FAIL\n";
    for (const auto& why : result_.failures) std::cout << "  " << why << "\n";
  }
  world_.finished = true;
  sc_core::sc_stop();
}

}  // namespace ucie
