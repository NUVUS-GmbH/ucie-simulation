"""Minimized event traces. One step is one protocol event, not a cycle."""

from __future__ import annotations

from simulation.model.axi import safe_to_retry
from simulation.model.system import RunResult
from simulation.model.types import Fate


def _minimize(events: list[str]) -> list[str]:
    """Drop link bring-up that happens before the first AXI accept.

    Recovery bring-up stays, because it is after that accept and it is part
    of the fate of the transaction.
    """

    for index, event in enumerate(events):
        if event.startswith("AXI request accepted"):
            return events[index:]
    return events


def format_case(result: RunResult, tx_index: int = 0) -> str:
    tx = result.transactions[tx_index]
    lines = [
        f"Transaction T{tx.logical_id}: {tx.name}",
        f"Address: {tx.address:#x}",
        f"Operation: {tx.op.value}",
        f"Idempotent: {'yes' if tx.idempotent else 'no'}",
        f"Policy: {result.scenario.policy.value} ({result.scenario.policy.name})",
        f"Observation used for the initiator label: {result.scenario.observation.value}",
        "",
    ]
    events = _minimize(result.events)
    for number, event in enumerate(events, start=1):
        lines.append(f"Step {number}: {event}")
    lines.extend(
        [
            "",
            "Ground truth:",
            f"    architectural_commit_count = {tx.ground_commit_count}",
            f"    target_accepted = {str(tx.target_accepted).lower()}",
            f"    response_generated = {str(tx.response_generated).lower()}",
            "",
            "Initiator observation:",
            f"    completion_received = {str(tx.requester_received_response).lower()}",
            f"    axi_accepted = {str(tx.axi_accepted).lower()}",
            f"    request_acknowledged = {str(tx.request_ack).lower()}",
            f"    write_data_acknowledged = {str(tx.data_ack).lower()}",
            f"    request_nak_only = {str(tx.request_nak_only).lower()}",
            f"    write_data_transmitted = {str(tx.data_attempted).lower()}",
            "",
            "Classification:",
            f"    {tx.initiator_class.value if tx.initiator_class else 'UNCLASSIFIED'}",
            "",
            "Classification if only FDI state and AXI completion are visible:",
            f"    {tx.fdi_class.value if tx.fdi_class else 'UNCLASSIFIED'}",
            "",
            "Classification if retry disposition was latched before flush (assumption):",
            f"    {tx.disposition_class.value if tx.disposition_class else 'UNCLASSIFIED'}",
            "",
            "Safe automatic retry:",
            f"    {'YES' if tx.initiator_class and safe_to_retry(tx.initiator_class) else 'NO'}",
            "",
            "Safe automatic retry from FDI and AXI alone:",
            f"    {'YES' if tx.fdi_class and safe_to_retry(tx.fdi_class) else 'NO'}",
        ]
    )
    if result.violations:
        lines.append("")
        lines.append("Invariant results:")
        for item in result.violations:
            lines.append(f"    {item}")
    else:
        lines.append("")
        lines.append("Invariant results:")
        lines.append("    none fired")
    return "\n".join(lines)


def comparison(after_commit: RunResult, before: RunResult, before_fdi: RunResult) -> str:
    """Explain the two milestone traces from their actual labels."""

    left = after_commit.transactions[0]
    right = before.transactions[0]
    right_fdi = before_fdi.transactions[0]
    lines = [
        "Comparison",
        "",
        "The two traces are not the same execution. In the first, the target "
        f"commits (architectural_commit_count = {left.ground_commit_count}) and the "
        "write response is then lost. In the second, every copy of the request "
        "fails CRC before the target accepts it "
        f"(architectural_commit_count = {right.ground_commit_count}).",
        "",
        "What the initiating AXI manager can see, once FDI has gone through "
        "LinkError and come back, is the same in both traces: the request was "
        "accepted, and no B response arrived. "
        f"FDI/AXI classification of the committed trace: {left.fdi_class.value}. "
        f"FDI/AXI classification of the uncommitted trace: {right_fdi.fdi_class.value}.",
        "",
    ]
    if left.fdi_class == Fate.UNKNOWN and right_fdi.fdi_class == Fate.UNKNOWN and (
        left.ground_commit_count != right.ground_commit_count
    ):
        lines.append(
            "Those two initiator views match. UCIe reports link state on FDI "
            "(pl_state_sts) and AoU 0.8 §8.3.3, as cited, returns the activation "
            "FSM to DISABLED on that link-down. Neither interface identifies which "
            "outstanding AXI transaction had reached architectural commit. The "
            "initiator therefore cannot tell the committed increment from the "
            "increment that never ran."
        )
    else:
        lines.append(
            "The FDI/AXI labels differed in this run. The paragraph above is "
            "withheld because it would not match the labels just printed."
        )
    lines.append("")
    if (
        right.disposition_class == Fate.NOT_COMPLETED
        and right.ground_commit_count == 0
        and left.disposition_class == Fate.UNKNOWN
        and left.ground_commit_count >= 1
    ):
        lines.append(
            "If the initiator's adapter latches retry disposition before the "
            "LinkError flush, the two traces separate. The uncommitted request "
            "was only NAK'd (UCIe 2.0 §3.7 does not deliver a CRC-invalid flit) "
            f"and is classified {right.disposition_class.value}. The committed "
            f"request was acknowledged and its response was lost, so it stays "
            f"{left.disposition_class.value}. That latch is an assumption. "
            "UCIe 2.0 flushes retry buffers on this path and does not require "
            "the NAK/ACK history to be kept, tied to an AXI id, and shown to "
            "software. AoU 0.8, in the text available for this model, does not "
            "require it either."
        )
    lines.append("")
    lines.append(
        "A label of NOT_COMPLETED is safe for the second trace because the "
        f"commit count is {right.ground_commit_count}. Applying that same label "
        "to the first trace would be a false safe retry: the commit count there "
        f"is {left.ground_commit_count}."
    )
    return "\n".join(lines)
