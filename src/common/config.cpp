#include "config.hpp"

#include <cctype>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace ucie {
namespace {

struct Node {
  bool is_map = true;
  std::string scalar;
  std::map<std::string, Node> map;
};

int indent_of(const std::string& line) {
  int n = 0;
  while (n < static_cast<int>(line.size()) && line[n] == ' ') ++n;
  return n;
}

std::string trim(std::string s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
  if (s.size() >= 2 && s.front() == '"' && s.back() == '"') s = s.substr(1, s.size() - 2);
  return s;
}

Node parse_yaml_subset(std::istream& in) {
  struct Frame {
    int indent;
    Node* node;
  };
  Node root;
  std::vector<Frame> stack{{ -1, &root }};
  std::string line;
  while (std::getline(in, line)) {
    auto hash = line.find('#');
    if (hash != std::string::npos) line = line.substr(0, hash);
    if (trim(line).empty()) continue;
    const int ind = indent_of(line);
    auto body = trim(line);
    auto colon = body.find(':');
    if (colon == std::string::npos) {
      throw std::runtime_error("config line is not a key: " + body);
    }
    auto key = trim(body.substr(0, colon));
    auto rest = trim(body.substr(colon + 1));
    while (stack.size() > 1 && ind <= stack.back().indent) stack.pop_back();
    Node child;
    child.is_map = rest.empty();
    child.scalar = rest;
    auto& slot = stack.back().node->map[key];
    slot = std::move(child);
    if (rest.empty()) stack.push_back(Frame{ind, &slot});
  }
  return root;
}

const Node* descend(const Node& root, std::initializer_list<const char*> path) {
  const Node* n = &root;
  for (const char* key : path) {
    auto it = n->map.find(key);
    if (it == n->map.end()) return nullptr;
    n = &it->second;
  }
  return n;
}

int as_int(const Node& n, int fallback) {
  if (n.is_map || n.scalar.empty()) return fallback;
  return std::stoi(n.scalar);
}

double as_double(const Node& n, double fallback) {
  if (n.is_map || n.scalar.empty()) return fallback;
  return std::stod(n.scalar);
}

bool as_bool(const Node& n, bool fallback) {
  if (n.is_map || n.scalar.empty()) return fallback;
  if (n.scalar == "true" || n.scalar == "yes") return true;
  if (n.scalar == "false" || n.scalar == "no") return false;
  return fallback;
}

}  // namespace

Config Config::defaults() { return Config{}; }

void Config::overlay_yaml(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open config: " + path);
  auto root = parse_yaml_subset(in);

  auto take_d = [&](double& dst, std::initializer_list<const char*> path) {
    if (const Node* n = descend(root, path)) dst = as_double(*n, dst);
  };
  auto take_i = [&](int& dst, std::initializer_list<const char*> path) {
    if (const Node* n = descend(root, path)) dst = as_int(*n, dst);
  };
  auto take_b = [&](bool& dst, std::initializer_list<const char*> path) {
    if (const Node* n = descend(root, path)) dst = as_bool(*n, dst);
  };

  take_d(clock_period_ns, {"clock_period_ns"});
  take_b(cycle_mode, {"cycle_mode"});
  take_i(axi_handshake_cycles, {"axi", "handshake_cycles"});
  take_i(aou_pack_cycles, {"aou", "pack_cycles"});
  take_i(serialization_cycles, {"ucie", "serialization_cycles"});
  take_i(propagation_cycles, {"ucie", "propagation_cycles"});
  take_i(buffer_cycles, {"ucie", "buffer_cycles"});
  take_i(arbitration_cycles, {"axi", "arbitration_cycles"});
  take_i(tx_buffer_entries, {"ucie", "tx_buffer_entries"});
  take_i(retry_buffer_entries, {"ucie", "retry_buffer_entries"});
  take_i(rx_buffer_entries, {"ucie", "rx_buffer_entries"});
  take_i(credit_depth, {"aou", "credit_depth"});
  take_i(replay_timeout_flits, {"spec", "replay_timeout_flits"});
  take_i(replay_limit, {"policy", "replay_limit"});
  take_d(retrain_ns, {"ucie", "retrain_ns"});
  take_d(link_reset_ns, {"ucie", "link_reset_ns"});
  take_d(link_error_residency_ns, {"spec", "link_error_residency_ns"});
  take_d(sideband_timeout_ns, {"spec", "sideband_timeout_ns"});
  take_d(reactivation_ns, {"ucie", "reactivation_ns"});
  take_d(sideband_message_ns, {"sideband", "message_ns"});
  take_d(priority_sideband_ns, {"sideband", "priority_ns"});
  take_i(target_accept_cycles, {"target", "accept_cycles"});
  take_i(target_commit_cycles, {"target", "commit_cycles"});
  take_i(target_response_cycles, {"target", "response_cycles"});
  take_i(completion_cycles, {"target", "completion_cycles"});
  take_b(retrain_preserves_retry, {"policy", "retrain_preserves_retry"});
  take_i(axi_queue_entries, {"axi", "request_queue"});
  take_i(aou_queue_entries, {"aou", "request_queue"});
  take_i(response_queue_entries, {"axi", "response_queue"});
  take_i(guest_queue_entries, {"virt", "guest_queue"});
  take_d(sim_timeout_ns, {"sim_timeout_ns"});
  take_b(partition_credits, {"aou", "partition_credits"});

  take_b(single_inflight_slot, {"ucie", "single_inflight_slot"});
  take_b(early_credit_return, {"ucie", "early_credit_return"});
  take_i(credit_update_threshold, {"ucie", "credit_update_threshold"});
  take_i(sideband_fifo_entries, {"sideband", "fifo_entries"});
  take_i(link_width, {"reference", "link_width"});
  take_i(fdi_width_bits, {"reference", "fdi_width_bits"});
  take_i(flit_format, {"reference", "flit_format"});
  take_b(crc_enabled, {"reference", "crc_enabled"});
  take_b(retry_enabled, {"reference", "retry_enabled"});
  take_b(l1_enabled, {"reference", "l1_enabled"});
  take_b(error_logging, {"reference", "error_logging"});
  take_b(priority_sideband, {"reference", "priority_sideband"});
  take_d(l1_entry_ns, {"ucie", "l1_entry_ns"});
  take_d(l1_exit_ns, {"ucie", "l1_exit_ns"});
  take_d(l1_hold_ns, {"ucie", "l1_hold_ns"});
  if (const Node* n = descend(root, {"reference", "ucie_revision"})) {
    if (!n->is_map && !n->scalar.empty()) ucie_revision = n->scalar;
  }
  // Also accept flat keys used by some experiment overlays.
  take_b(single_inflight_slot, {"single_inflight_slot"});
  take_b(crc_enabled, {"crc_enabled"});
  take_b(retry_enabled, {"retry_enabled"});

  if (const Node* n = descend(root, {"trace"})) {
    if (n->scalar == "none") trace = TraceLevel::None;
    else if (n->scalar == "errors") trace = TraceLevel::Errors;
    else if (n->scalar == "summary") trace = TraceLevel::Summary;
    else if (n->scalar == "transactions") trace = TraceLevel::Transactions;
    else if (n->scalar == "full") trace = TraceLevel::Full;
  }
  if (const Node* n = descend(root, {"policy", "recovery"})) {
    if (n->scalar == "discard" || n->scalar == "A") recovery = RecoveryPolicy::Discard;
    else if (n->scalar == "preserve_local" || n->scalar == "B") recovery = RecoveryPolicy::PreserveLocal;
    else if (n->scalar == "explicit_unknown" || n->scalar == "C") recovery = RecoveryPolicy::ExplicitUnknown;
    else if (n->scalar == "auto_retry" || n->scalar == "D") recovery = RecoveryPolicy::AutoRetry;
    else if (n->scalar == "no_auto_retry" || n->scalar == "E") recovery = RecoveryPolicy::NoAutoRetry;
  }
}

}  // namespace ucie
