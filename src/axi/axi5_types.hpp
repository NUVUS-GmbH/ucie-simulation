#pragma once

// AXI5-MM-P1 subset (MODEL PROFILE). Spec binding: ARM IHI 0022 Issue L.
// Implemented: AtomicStore(ADD) via AWATOP, WPOISON/RPOISON observation,
// Trace signals as observability-only. Explicitly NOT implemented: AtomicLoad,
// AtomicSwap, AtomicCompare, Exclusive, MTE, MPAM, Parity A16.2.
//
// AtomicStore(ADD) does NOT return the previous memory value (IHI0022L A6.4.1):
// single write response on B, no R-channel data.

#include "common/enums.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ucie {

// AWATOP[5:0] encodings (IHI0022L Tables A6.8 / A6.9).
// AtomicStore little-endian ADD: 0b01 e=0 xxx=000 → 0b010000 = 0x10.
inline constexpr std::uint8_t kAwatopNonAtomic = 0x00;
inline constexpr std::uint8_t kAwatopAtomicStoreAddLe = 0x10;  // 0b010000
inline constexpr std::uint8_t kAwatopAtomicLoadAddLe = 0x20;   // out of P1 scope
inline constexpr std::uint8_t kAwatopAtomicSwap = 0x30;        // out of P1 scope
inline constexpr std::uint8_t kAwatopAtomicCompare = 0x31;     // out of P1 scope

enum class Axi5AtomicForm : std::uint8_t {
  None = 0,
  AtomicStore,
  AtomicLoad,     // not implemented in P1
  AtomicSwap,     // not implemented
  AtomicCompare,  // not implemented
};

enum class Axi5AtomicOp : std::uint8_t {
  Add = 0,
  Clr,
  Eor,
  Set,
  Smax,
  Smin,
  Umax,
  Umin,
};

// Named TARGET POLICIES — Spec A16.1 leaves commit reaction open and states
// Poison is independent of error-response signaling.
enum class PoisonTargetPolicy : std::uint8_t {
  // Default profile: poisoned write/AtomicStore does not commit; BRESP=SLVERR.
  RejectWithoutCommitSlverr,
  // Same no-commit, but BRESP=OKAY (documents Poison⊥Error independence).
  RejectWithoutCommitOkay,
  // Accept architectural effect; mark location poisoned; BRESP=OKAY.
  AcceptMarkedOkay,
  // Accept effect; mark poisoned; BRESP=SLVERR (error without blocking poison).
  AcceptMarkedSlverr,
};

inline const char* to_string(PoisonTargetPolicy p) {
  switch (p) {
    case PoisonTargetPolicy::RejectWithoutCommitSlverr: return "reject_without_commit_slverr";
    case PoisonTargetPolicy::RejectWithoutCommitOkay: return "reject_without_commit_okay";
    case PoisonTargetPolicy::AcceptMarkedOkay: return "accept_marked_okay";
    case PoisonTargetPolicy::AcceptMarkedSlverr: return "accept_marked_slverr";
  }
  return "?";
}

inline const char* to_string(Axi5AtomicForm f) {
  switch (f) {
    case Axi5AtomicForm::None: return "NONE";
    case Axi5AtomicForm::AtomicStore: return "ATOMIC_STORE";
    case Axi5AtomicForm::AtomicLoad: return "ATOMIC_LOAD";
    case Axi5AtomicForm::AtomicSwap: return "ATOMIC_SWAP";
    case Axi5AtomicForm::AtomicCompare: return "ATOMIC_COMPARE";
  }
  return "?";
}

inline const char* to_string(Axi5AtomicOp op) {
  switch (op) {
    case Axi5AtomicOp::Add: return "ADD";
    case Axi5AtomicOp::Clr: return "CLR";
    case Axi5AtomicOp::Eor: return "EOR";
    case Axi5AtomicOp::Set: return "SET";
    case Axi5AtomicOp::Smax: return "SMAX";
    case Axi5AtomicOp::Smin: return "SMIN";
    case Axi5AtomicOp::Umax: return "UMAX";
    case Axi5AtomicOp::Umin: return "UMIN";
  }
  return "?";
}

// Per-transaction AXI5-MM-P1 record. Completely separate from AXIS AxisRecord.
struct Axi5MmRecord {
  bool enabled = false;
  Axi5AtomicForm form = Axi5AtomicForm::None;
  Axi5AtomicOp atomic_op = Axi5AtomicOp::Add;
  std::uint8_t awatop = kAwatopNonAtomic;
  bool little_endian = true;

  // Poison (WPOISON on write data). Spec: 1 bit / 64-bit granule.
  bool wpoison = false;
  bool rpoison = false;  // unused for AtomicStore (no R data)
  bool poison_observed_at_target = false;
  bool poison_rejected = false;
  bool poison_accepted_marked = false;
  PoisonTargetPolicy poison_policy = PoisonTargetPolicy::RejectWithoutCommitSlverr;

  // Trace (A12.3): present for observation; NEVER fed to classify()/InitiatorView.
  bool awtrace = false;
  bool wtrace = false;
  bool btrace = false;  // echoed by subordinate when AWTRACE was asserted
  bool trace_exported = false;
  bool trace_used_in_classify = false;  // must remain false

  // Separation: logical operation vs individual transfer attempts.
  std::uint64_t logical_op_id = 0;
  int transfer_attempt = 0;  // 0 = first issue; increments on software retry

  // Target-side atomic effect bookkeeping (evaluation / GT, not classifier).
  bool atomic_effect_applied = false;
  int atomic_effects = 0;
  int value_before = 0;
  int value_after = 0;
  int operand = 0;
  bool unsupported_form = false;  // AtomicLoad/Swap/Compare/Exclusive requested
  std::string unsupported_reason;

  // Response path: AtomicStore → B only; must not fabricate R completions.
  bool write_response_created = false;
  bool read_response_created = false;  // must stay false for AtomicStore
  AxiResp intended_bresp = AxiResp::Okay;

  std::vector<std::string> effect_log;
};

}  // namespace ucie
