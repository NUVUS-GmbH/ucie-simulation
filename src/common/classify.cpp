#include "transaction.hpp"

#include <stdexcept>

namespace ucie {

InitiatorView view_for(ObserverProfile profile, const Transaction& tx) {
  // Copy only initiator-visible fields. GroundTruth is not read.
  InitiatorView v;
  v.poisoned_request = tx.axi5.enabled && tx.axi5.wpoison;
  v.axi_completed_ok = tx.visible.axi_completed_ok;
  v.axi_completed_err = tx.visible.axi_completed_err;
  v.axi_resp = tx.visible.axi_resp;
  v.axi_resp_source = tx.visible.axi_resp_source;
  v.axi_accepted = tx.visible.axi_accepted;
  v.explicit_unknown = tx.visible.explicit_unknown;
  v.is_write = tx.is_write;
  v.status_has_axi_id = tx.visible.status_has_axi_id;
  v.status_has_commit_bit = tx.visible.status_has_commit_bit;
  v.txn_watchdog_fired = tx.visible.txn_watchdog_fired;

  if (profile == ObserverProfile::A_AxiOnly) return v;

  // Profile B may observe FDI link-down. Whether the classifier *uses* that
  // signal is separate from its presence on the transaction record.
  v.use_fdi = true;
  v.fdi_link_down = tx.visible.fdi_link_down;

  if (profile == ObserverProfile::B_AxiFdi) return v;

  // Profile C: standardized UCIe status (still no AXI id / commit bit).
  v.use_status = true;
  v.linkerror_status = tx.visible.linkerror_status;

  if (profile == ObserverProfile::C_AxiFdiUcieStatus) {
    // Optional bridge diagnostics: presence is gated; use is gated separately.
    if (tx.visible.use_diag_fields) {
      v.use_diag = true;
      v.diag_bridge_local_slverr = tx.visible.diag_bridge_local_slverr;
      v.diag_bridge_token_valid = tx.visible.diag_bridge_token_valid;
      v.diag_never_handed = tx.visible.diag_never_handed;
      v.diag_missing_ack = tx.visible.diag_missing_ack;
    }
    return v;
  }

  // Profile D: ACK/NAK disposition assumption + optional diagnostics.
  v.use_disposition = true;
  v.handed_to_ucie = tx.visible.handed_to_ucie;
  v.data_attempted = tx.visible.data_attempted;
  v.request_ack = tx.visible.request_ack;
  v.data_ack = tx.visible.data_ack;
  v.request_nak_only = tx.visible.request_nak_only;
  v.data_nak_only = tx.visible.data_nak_only;
  v.request_timeout = tx.visible.request_timeout;
  v.data_timeout = tx.visible.data_timeout;
  if (tx.visible.use_diag_fields) {
    v.use_diag = true;
    v.diag_bridge_local_slverr = tx.visible.diag_bridge_local_slverr;
    v.diag_bridge_token_valid = tx.visible.diag_bridge_token_valid;
    v.diag_never_handed = tx.visible.diag_never_handed;
    v.diag_missing_ack = tx.visible.diag_missing_ack;
  }
  return v;
}

Fate classify(const InitiatorView& obs) {
  if (obs.status_has_axi_id || obs.status_has_commit_bit) {
    throw std::logic_error(
        "a standardized UCIe structure was treated as carrying an AXI id or a commit bit");
  }

  // COMPLETED records successful transfer completion, not effect knowledge.
  if (obs.axi_completed_ok) return Fate::Completed;

  if (obs.explicit_unknown) return Fate::Unknown;
  if (!obs.axi_accepted) return Fate::NotCompleted;

  // [PROJECT REQUIREMENT / MODEL] Local or remote SLVERR completes the AXI
  // handshake but does not by itself prove presence or absence of a remote
  // architectural effect (AXI4: SLVERR = unsuccessful transaction; it does not
  // encode a commit bit). Default: UNKNOWN.
  //
  // Diagnostic evaluation rule (profiles C/D when use_diag_in_classifier):
  //   local SLVERR ∧ never handed to UCIe TX → NOT_COMPLETED
  //   missing ACK alone is NOT sufficient (delivery can precede ACK)
  // Diagnostics still cannot invent a commit bit / COMPLETED.

  if (obs.use_diag) {
    if (obs.axi_completed_err && obs.diag_bridge_local_slverr && obs.diag_never_handed) {
      return Fate::NotCompleted;
    }
    // Explicitly consulted so a missing-ACK bit cannot silently drive fate.
    (void)obs.diag_missing_ack;
    (void)obs.diag_bridge_token_valid;
  }

  if (obs.use_disposition) {
    // [ASSUMPTION] The endpoint can remember that it never handed the
    // transaction to UCIe. FDI pl_state_sts does not carry that fact.
    if (!obs.handed_to_ucie) return Fate::NotCompleted;

    // [ASSUMPTION] Device model: a write does not commit before its data is
    // delivered. The endpoint knows it never transmitted data only if that
    // fact was latched. FDI does not carry it.
    if (obs.is_write && !obs.data_attempted) return Fate::NotCompleted;

    // NAK-only without ACK: [ASSUMPTION] treated as request absent *only when*
    // a NAK was observed. A missing ACK alone (lost/delayed ACK after delivery)
    // must remain UNKNOWN — see fh_diag_ack_loss_after_delivery.
    if (obs.is_write) {
      const bool request_absent =
          obs.request_nak_only && !obs.request_ack && !obs.request_timeout;
      const bool data_absent =
          obs.data_attempted && obs.data_nak_only && !obs.data_ack && !obs.data_timeout;
      if (request_absent || data_absent) return Fate::NotCompleted;
      return Fate::Unknown;
    }
    const bool request_absent =
        obs.request_nak_only && !obs.request_ack && !obs.request_timeout;
    if (request_absent) return Fate::NotCompleted;
    return Fate::Unknown;
  }

  // Profiles A/B/C without disposition and without a conclusive diag rule:
  // SLVERR, timeout, or missing completion leave effect knowledge indeterminate.
  (void)obs.use_fdi;
  (void)obs.use_status;
  (void)obs.fdi_link_down;
  (void)obs.linkerror_status;
  (void)obs.txn_watchdog_fired;
  return Fate::Unknown;
}

EffectKnowledge classify_effect(const InitiatorView& obs) {
  // Profile contract: an unpoisoned successful MM response (or the modeled
  // AXIS application ACK) certifies the intended effect, not exactly-once.
  // Poison permits different target policies. The sender knows its Poison
  // attribute but not which effect occurred; neither target state nor GT is read.
  if (obs.axi_completed_ok) {
    return obs.poisoned_request ? EffectKnowledge::Indeterminate
                               : EffectKnowledge::SureEffect;
  }
  return classify(obs) == Fate::NotCompleted ? EffectKnowledge::SureNoEffect
                                             : EffectKnowledge::Indeterminate;
}

std::vector<InvariantIssue> check_transaction_invariants(const std::vector<Transaction>& txs) {
  std::vector<InvariantIssue> out;
  for (const auto& tx : txs) {
    InvariantIssue issue;
    issue.logical_id = tx.logical_id;
    const Fate label = tx.visible.fate_a;
    if (label == Fate::NotCompleted && tx.ground.architectural_commit_count != 0) {
      issue.false_safe_retry = true;
      issue.false_sure_no_effect = true;
    }
    if (tx.software_retry_count > 0 && tx.ground.commit_at_reissue > 0) {
      issue.false_safe_retry = true;
    }
    // Safe-retry gap: reissue armed while remote could still execute later,
    // and a commit eventually appeared. "No commit yet" at arming is insufficient.
    if (tx.software_retry_count > 0 && tx.ground.commit_at_reissue == 0 &&
        tx.ground.architectural_commit_count > 0 && tx.bridge.allow_remote_continue) {
      issue.false_safe_retry = true;
    }
    if (tx.axis.enabled) {
      // AXIS sinks may legally produce multiple IMM effects (one per beat) or
      // one PKT effect per attempt. At-most-once is relative to that profile.
      const int attempts = 1 + std::max(0, tx.software_retry_count);
      const int max_ok = (tx.axis.sink == AxisSinkKind::Imm)
                             ? tx.axis.beat_count * attempts
                             : attempts;
      if (tx.ground.architectural_commit_count > max_ok) issue.at_most_once = true;
    } else if (tx.ground.architectural_commit_count > 1) {
      issue.at_most_once = true;
    }
    // Knowledge is checked for every observer independently of transfer fate.
    // SURE_EFFECT means the intended effect occurred, not that it occurred once.
    const int required_effects = tx.axis.enabled && tx.axis.sink == AxisSinkKind::Imm
                                     ? tx.axis.beat_count : 1;
    for (auto knowledge : {tx.visible.knowledge_a, tx.visible.knowledge_b,
                           tx.visible.knowledge_c, tx.visible.knowledge_d}) {
      if (knowledge == EffectKnowledge::SureEffect &&
          tx.ground.architectural_commit_count < required_effects)
        issue.false_sure_effect = true;
      if (knowledge == EffectKnowledge::SureNoEffect &&
          tx.ground.architectural_commit_count != 0)
        issue.false_sure_no_effect = true;
    }
    // Protocol inconsistency is independent of the target's effect policy.
    issue.completion_inconsistent = tx.visible.axi_completed_ok && tx.visible.axi_completed_err;
    // SLVERR must not be treated as proof of "not executed".
    if (tx.visible.axi_completed_err && label == Fate::NotCompleted &&
        tx.ground.architectural_commit_count != 0) {
      issue.false_sure_no_effect = true;
      issue.false_safe_retry = true;
    }
    if (tx.visible.duplicate_completion) {
      issue.duplicate_completion = true;
      issue.second_local_response = true;
    }
    if (issue.false_safe_retry || issue.at_most_once || issue.completion_inconsistent ||
        issue.duplicate_completion || issue.false_sure_no_effect || issue.false_sure_effect ||
        issue.second_local_response) {
      out.push_back(issue);
    }
  }
  return out;
}

}  // namespace ucie
