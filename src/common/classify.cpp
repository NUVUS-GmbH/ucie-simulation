#include "transaction.hpp"

#include <stdexcept>

namespace ucie {

InitiatorView view_for(ObserverProfile profile, const Transaction& tx) {
  // Copy only initiator-visible fields. GroundTruth is not read.
  InitiatorView v;
  v.axi_completed_ok = tx.visible.axi_completed_ok;
  v.axi_accepted = tx.visible.axi_accepted;
  v.explicit_unknown = tx.visible.explicit_unknown;
  v.is_write = tx.is_write;
  v.status_has_axi_id = tx.visible.status_has_axi_id;
  v.status_has_commit_bit = tx.visible.status_has_commit_bit;

  if (profile == ObserverProfile::A_AxiOnly) return v;

  // Profiles B and C may *see* FDI and standardized status. Those signals
  // still do not name an AXI transaction, so they are not copied into the
  // fields classify() uses to pick NOT_COMPLETED. Recording them on the
  // transaction is what lets a report show that B and C observed the link
  // event and still produced the same label as A.
  if (profile == ObserverProfile::D_AxiHypervisorMetadata) {
    // [ASSUMPTION] ACK/NAK history latched before the retry buffer is flushed
    // is not a UCIe-required export, and it is not tied to an AXI id by the
    // specification text used here. Only observer D reads it.
    v.use_disposition = true;
    v.handed_to_ucie = tx.visible.handed_to_ucie;
    v.data_attempted = tx.visible.data_attempted;
    v.request_ack = tx.visible.request_ack;
    v.data_ack = tx.visible.data_ack;
    v.request_nak_only = tx.visible.request_nak_only;
    v.data_nak_only = tx.visible.data_nak_only;
    v.request_timeout = tx.visible.request_timeout;
    v.data_timeout = tx.visible.data_timeout;
  }
  return v;
}

Fate classify(const InitiatorView& obs) {
  if (obs.status_has_axi_id || obs.status_has_commit_bit) {
    throw std::logic_error(
        "a standardized UCIe structure was treated as carrying an AXI id or a commit bit");
  }
  if (obs.axi_completed_ok) return Fate::Completed;
  if (obs.explicit_unknown) return Fate::Unknown;
  if (!obs.axi_accepted) return Fate::NotCompleted;
  if (!obs.use_disposition) return Fate::Unknown;

  // [ASSUMPTION] The endpoint can remember that it never handed the
  // transaction to UCIe. FDI pl_state_sts does not carry that fact.
  if (!obs.handed_to_ucie) return Fate::NotCompleted;

  // [ASSUMPTION] Device model: a write does not commit before its data is
  // delivered. The endpoint knows it never transmitted data only if that
  // fact was latched. FDI does not carry it.
  if (obs.is_write && !obs.data_attempted) return Fate::NotCompleted;

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

std::vector<InvariantIssue> check_transaction_invariants(const std::vector<Transaction>& txs) {
  std::vector<InvariantIssue> out;
  for (const auto& tx : txs) {
    InvariantIssue issue;
    issue.logical_id = tx.logical_id;
    const Fate label = tx.visible.fate_a;
    if (label == Fate::NotCompleted && tx.ground.architectural_commit_count != 0) {
      issue.false_safe_retry = true;
    }
    if (tx.software_retry_count > 0 && tx.ground.commit_at_reissue > 0) {
      issue.false_safe_retry = true;
    }
    if (tx.ground.architectural_commit_count > 1) issue.at_most_once = true;
    if (label == Fate::Completed && tx.ground.architectural_commit_count != 1) {
      issue.completion_inconsistent = true;
    }
    if (tx.visible.duplicate_completion) issue.duplicate_completion = true;
    if (issue.false_safe_retry || issue.at_most_once || issue.completion_inconsistent ||
        issue.duplicate_completion) {
      out.push_back(issue);
    }
  }
  return out;
}

}  // namespace ucie
