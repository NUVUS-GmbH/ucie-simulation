#pragma once

// AXI4-Stream (AXIS-P1-Continuous) study types.
// Spec basis: ARM IHI 0051B. Architectural effects are MODEL PROFILE sinks,
// not AXI-Stream protocol outcomes. No BRESP exists on this interface.

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace ucie {

enum class AxisSinkKind : std::uint8_t {
  Imm,  // SINK-IMM: effect per remotely accepted transfer
  Pkt,  // SINK-PKT: buffer to TLAST; processing is a separate stage
};

// Optional study mechanisms — not part of the AXIS-P1 base interface.
enum class AxisAppAckMode : std::uint8_t {
  Off,
  SeparateReturn,  // MODEL: separate return message; can be lost
};

struct AxisDupSuppressConfig {
  bool enabled = false;
  // Lifetime of a remembered packet_op_id in ns. 0 = until reset/flush only.
  double lifetime_ns = 0;
  // Max remembered IDs. Evict oldest when full (MODEL PROFILE).
  int capacity = 16;
  // After destructive LinkReset/flush: clear table when true.
  bool clear_on_reset = true;
  // Allow reuse of an ID after lifetime expiry / eviction.
  bool allow_reuse_after_expiry = true;
};

struct AxisBeatOffer {
  int beat_index = 0;
  std::uint64_t tdata = 0;
  std::uint16_t tkeep = 0xffff;
  bool tlast = false;
  std::uint8_t tid = 0;
  std::uint8_t tdest = 0;
  std::uint8_t tuser = 0;
  int valid_bytes = 16;
};

struct AxisRecord {
  bool enabled = false;
  AxisSinkKind sink = AxisSinkKind::Imm;
  bool continuous_packets = true;
  int beat_count = 1;
  int data_width_bytes = 16;  // MODEL PROFILE: 128-bit TDATA
  int last_beat_valid_bytes = 16;
  std::uint8_t tid = 0;
  std::uint8_t tdest = 0;
  std::uint8_t tuser = 0;

  // Offered beat contents (stable while TVALID && !TREADY).
  std::vector<AxisBeatOffer> offers;
  int next_offer = 0;
  bool tvalid = false;
  bool tready = false;
  AxisBeatOffer held;

  // --- Dimension: local acceptance (TVALID∧TREADY only) ---
  int local_beats_accepted = 0;
  int local_bytes_accepted = 0;
  bool local_tlast_accepted = false;
  bool local_packet_done = false;
  double t_local_tlast_ns = -1;

  // --- Dimension: remote acceptance (UCIe delivery / sink RX) ---
  int remote_beats_accepted = 0;
  int remote_bytes_accepted = 0;
  bool remote_packet_complete = false;  // TLAST accepted remotely (reception only)
  double t_remote_packet_complete_ns = -1;

  // --- Dimension: application processing (explicit; not implied by reception) ---
  bool processing_started = false;
  double t_processing_start_ns = -1;
  int app_effects = 0;       // count of atomic application effects
  int bytes_committed = 0;   // bytes that entered architectural state
  std::vector<double> effect_times_ns;
  std::vector<std::string> effect_log;

  // Pending beats awaiting AoU pack / remote sink handling (slot queues carry only txn).
  std::deque<int> pending_pack_beats;
  std::deque<int> pending_remote_beats;
  // SINK-PKT receive buffer (bytes), filled before processing starts.
  std::vector<std::uint8_t> pkt_buffer;
  bool pkt_buffer_sealed = false;

  // Optional mechanisms (additional to base profile).
  AxisAppAckMode app_ack_mode = AxisAppAckMode::Off;
  bool app_ack_received = false;
  bool app_ack_lost = false;
  double t_app_ack_ns = -1;
  std::uint64_t packet_op_id = 0;
  bool packet_id_enabled = false;
  bool dup_suppress_hit = false;

  // Continuous_Packets / stability checks.
  bool protocol_error = false;
  std::string protocol_error_reason;

  // Stream lifecycle close without AXI BRESP (bridge abort / settle).
  bool stream_aborted = false;
  bool stream_settled = false;

  int valid_bytes_for_beat(int beat) const {
    if (beat < 0 || beat >= beat_count) return 0;
    if (beat + 1 == beat_count) return last_beat_valid_bytes;
    return data_width_bytes;
  }

  std::uint16_t keep_for_beat(int beat) const {
    const int vb = valid_bytes_for_beat(beat);
    if (vb >= 16) return 0xffff;
    return static_cast<std::uint16_t>((1u << vb) - 1u);
  }
};

}  // namespace ucie
