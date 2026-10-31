"""Aggregate counters. Every number in the report is computed from run results."""

from __future__ import annotations

from collections import defaultdict

from simulation.model.system import RunResult
from simulation.model.types import BUCKET_ORDER, Fate, Policy
from simulation.verification.state_explorer import ExplorerSummary


def _watched(result: RunResult):
    if not result.transactions:
        return None
    index = min(result.scenario.fault_tx, len(result.transactions) - 1)
    return result.transactions[index]


def tally(results: list[RunResult]) -> dict[str, int]:
    out = defaultdict(int)
    for key in (
        "simulations",
        "fault_injections",
        "transparent_retries",
        "retrain_events",
        "linkreset_events",
        "linkerror_events",
        "liveness_failures",
        "false_safe_retry",
        "duplicate_commit",
        "credit_violations",
        "overflow_violations",
        "fdi_completed",
        "fdi_not_completed",
        "fdi_unknown",
        "transactions_commit_gt1",
    ):
        out[key] = 0
    out["simulations"] = len(results)
    for result in results:
        out["fault_injections"] += result.stats.fault_injections
        out["transparent_retries"] += result.stats.transparent_retries
        out["retrain_events"] += result.stats.retrain_events
        out["linkreset_events"] += result.stats.linkreset_events
        out["linkerror_events"] += result.stats.linkerror_events
        if result.deadlock or not result.link_converged:
            out["liveness_failures"] += 1
        kinds = {violation.split(":")[0] for violation in result.violations}
        if "property 1" in kinds:
            out["false_safe_retry"] += 1
        if "property 2" in kinds:
            out["duplicate_commit"] += 1
        if "property 4" in kinds:
            out["credit_violations"] += 1
        if "property 5" in kinds:
            out["overflow_violations"] += 1
        for tx in result.transactions:
            if tx.fdi_class == Fate.COMPLETED:
                out["fdi_completed"] += 1
            elif tx.fdi_class == Fate.NOT_COMPLETED:
                out["fdi_not_completed"] += 1
            elif tx.fdi_class == Fate.UNKNOWN:
                out["fdi_unknown"] += 1
            if tx.ground_commit_count > 1:
                out["transactions_commit_gt1"] += 1
    return dict(out)


def bucket_rates(results: list[RunResult], which: str) -> list[tuple[str, int, int, int, int]]:
    """Return (bucket, completed, not_completed, unknown, total) for the watched transaction."""

    from simulation.model.types import fault_bucket

    counts = {
        name: {Fate.COMPLETED: 0, Fate.NOT_COMPLETED: 0, Fate.UNKNOWN: 0}
        for name in BUCKET_ORDER
    }
    for result in results:
        tx = _watched(result)
        if tx is None or tx.fault_phase is None:
            continue
        bucket = fault_bucket(tx.fault_phase)
        if bucket not in counts:
            continue
        fate = tx.fdi_class if which == "fdi" else tx.disposition_class
        if fate in counts[bucket]:
            counts[bucket][fate] += 1
    rows = []
    for name in BUCKET_ORDER:
        completed = counts[name][Fate.COMPLETED]
        not_completed = counts[name][Fate.NOT_COMPLETED]
        unknown = counts[name][Fate.UNKNOWN]
        rows.append((name, completed, not_completed, unknown, completed + not_completed + unknown))
    return rows


def format_rate_table(rows: list[tuple[str, int, int, int, int]]) -> str:
    lines = [
        f"{'Fault position':<32} {'COMPLETED':>10} {'NOT_COMPLETED':>14} {'UNKNOWN':>8} {'n':>5} {'UNKNOWN rate':>13}",
        "-" * 86,
    ]
    for name, completed, not_completed, unknown, total in rows:
        rate = "n/a" if total == 0 else f"{(100.0 * unknown / total):.1f}%"
        lines.append(
            f"{name:<32} {completed:>10} {not_completed:>14} {unknown:>8} {total:>5} {rate:>13}"
        )
    return "\n".join(lines)


def policy_differences(results: list[RunResult]) -> list[str]:
    """Report externally visible divergence between policies on paired names."""

    by_name = {result.scenario.name: result for result in results}
    lines: list[str] = []
    interesting = []
    for policy in Policy:
        key_u = f"policy_{policy.name}_unknown"
        key_b = f"policy_{policy.name}_before"
        if key_u in by_name and key_b in by_name:
            interesting.append((policy, by_name[key_u], by_name[key_b]))
    if len(interesting) < 2:
        return ["No paired policy runs were present."]
    baseline = interesting[0][1]
    for policy, unknown_run, before_run in interesting:
        tx_u = unknown_run.transactions[0]
        tx_b = before_run.transactions[0]
        base_u = baseline.transactions[0]
        visible = (
            tx_u.initiator_class,
            tx_u.ground_commit_count,
            tx_u.requester_received_response,
            tx_u.explicit_unknown,
            len(unknown_run.violations),
        )
        base_visible = (
            base_u.initiator_class,
            base_u.ground_commit_count,
            base_u.requester_received_response,
            base_u.explicit_unknown,
            len(baseline.violations),
        )
        same = visible == base_visible
        lines.append(
            f"Policy {policy.value} ({policy.name}), FDI/AXI observation, on the post-commit loss: "
            f"initiator {tx_u.initiator_class.value if tx_u.initiator_class else '?'}, "
            f"commit {tx_u.ground_commit_count}, "
            f"completion_received {str(tx_u.requester_received_response).lower()}, "
            f"explicit_unknown {str(tx_u.explicit_unknown).lower()}, "
            f"violations {len(unknown_run.violations)}"
            + ("" if same or policy == interesting[0][0] else "  [differs from policy A]")
        )
        lines.append(
            f"Policy {policy.value} ({policy.name}), FDI/AXI observation, on the pre-acceptance loss: "
            f"initiator {tx_b.initiator_class.value if tx_b.initiator_class else '?'}, "
            f"commit {tx_b.ground_commit_count}, "
            f"violations {len(before_run.violations)}"
        )
    preserve = by_name.get("retrain_preserve")
    drop = by_name.get("retrain_reinitialize")
    if preserve and drop and preserve.transactions and drop.transactions:
        p = preserve.transactions[0]
        d = drop.transactions[0]
        lines.append(
            "Retrain while the response is still unacknowledged: "
            f"preserve-retry ends as {p.initiator_class.value if p.initiator_class else '?'} "
            f"commit {p.ground_commit_count} deadlock {str(preserve.deadlock).lower()}; "
            f"reinitialize-retry ends as {d.initiator_class.value if d.initiator_class else '?'} "
            f"commit {d.ground_commit_count} deadlock {str(drop.deadlock).lower()}."
        )
        if p.initiator_class != d.initiator_class or preserve.deadlock != drop.deadlock:
            lines.append(
                "These two outcomes are an integration ambiguity: UCIe 2.0 §3.8 "
                "reinitializes the IDLE-handshake retry variables on Retrain, and "
                "§10.3.3.5 states a flush for LinkReset, not for Retrain."
            )
    discard = by_name.get("local_rx_discard")
    keep = by_name.get("local_rx_preserve")
    if discard and keep and discard.transactions and keep.transactions:
        dtx = discard.transactions[0]
        ktx = keep.transactions[0]
        lines.append(
            "Write response sitting in the initiating AoU, not yet accepted on AXI, "
            "then LinkError: "
            f"policy A initiator {dtx.initiator_class.value if dtx.initiator_class else '?'} "
            f"commit {dtx.ground_commit_count}; "
            f"policy B initiator {ktx.initiator_class.value if ktx.initiator_class else '?'} "
            f"commit {ktx.ground_commit_count}."
        )
        if dtx.initiator_class != ktx.initiator_class:
            lines.append(
                "Policy A and policy B disagree here. UCIe does not say whether a "
                "response already queued above the adapter survives LinkError cleanup. "
                "That is an integration choice, not a detected coding bug."
            )
    return lines


def conclusion(systematic: dict[str, int], randomized: dict[str, int], milestones_show_gap: bool, duplicate: bool) -> str:
    """Select the research paragraphs from measured flags, not from expectation."""

    unknown = systematic.get("fdi_unknown", 0) + randomized.get("fdi_unknown", 0)
    paragraphs: list[str] = []
    if unknown > 0 and milestones_show_gap:
        paragraphs.append(
            "Systematic fault injection identified legal execution traces in which "
            "a remote AXI operation reached its architectural commit point before "
            "UCIe entered LinkError, while the corresponding completion response "
            "was lost during error recovery. In these traces, the initiating "
            "endpoint could not distinguish a committed operation from an operation "
            "that had not executed. The modeled AoU 0.8/UCIe 2.0 stack therefore "
            "requires an explicit UNKNOWN transaction state or an additional "
            "end-to-end recovery mechanism for safe handling of non-idempotent "
            "transactions."
        )
    elif unknown > 0:
        paragraphs.append(
            f"The initiator FDI/AXI classifier returned UNKNOWN for {unknown} "
            "transactions. The milestone pair did not by itself show a committed "
            "trace and an uncommitted trace with the same initiator label, so the "
            "stronger indistinguishability claim is not made from these runs."
        )
    else:
        paragraphs.append(
            "No transaction-fate ambiguity was found within the explored model and "
            "bounded state space. All interrupted transactions could be classified "
            "as completed or not completed under the modeled recovery rules. This "
            "result is conditional on the stated assumptions and does not constitute "
            "proof for behaviors outside the explored state space."
        )
    if duplicate or systematic.get("duplicate_commit", 0) or randomized.get("duplicate_commit", 0):
        paragraphs.append(
            "Automatic retry after UCIe link recovery can result in duplicate "
            "architectural effects when a transaction commits remotely before its "
            "completion response is lost. Exactly-once semantics therefore cannot "
            "be assumed without additional transaction sequencing, persistent "
            "commit tracking or duplicate suppression."
        )
    return "\n\n".join(paragraphs)


def render_report(
    milestones: dict[str, RunResult],
    explorer: ExplorerSummary,
    random_results: list[RunResult],
    seed: int,
) -> str:
    systematic = tally(explorer.results)
    randomized = tally(random_results)
    combined_sims = systematic["simulations"] + randomized["simulations"]
    gap = False
    unknown = milestones["unknown"].transactions[0]
    before = milestones["before_disposition"].transactions[0]
    before_fdi = milestones["before_fdi"].transactions[0]
    if (
        unknown.fdi_class == Fate.UNKNOWN
        and before_fdi.fdi_class == Fate.UNKNOWN
        and unknown.ground_commit_count >= 1
        and before.ground_commit_count == 0
    ):
        gap = True
    duplicate = any(tx.ground_commit_count > 1 for result in explorer.results for tx in result.transactions)

    from simulation.reporting.trace import comparison, format_case

    parts = [
        "AoU 0.8 over UCIe 2.0: transaction fate after link recovery",
        "",
        "This report is generated from the simulator. Counts are not hand-written.",
        "The AoU 0.8 specification text is not in this workspace. AoU behavior is",
        "limited to the publicly cited §8.3.3 link-down rule (activation returns",
        "to DISABLED; Retrain is not that event) plus an explicit credit and",
        "packing abstraction. Outstanding-transaction fate is a policy.",
        "UCIe rules that are implemented are cited in simulation/model/ucie.py.",
        "",
        "Case: response lost after architectural commit",
        "",
        format_case(milestones["unknown"]),
        "",
        "Case: fault before target acceptance",
        "",
        format_case(milestones["before_disposition"]),
        "",
        "Same pre-acceptance trace, classified from FDI state and AXI completion only",
        "",
        format_case(milestones["before_fdi"]),
        "",
        comparison(milestones["unknown"], milestones["before_disposition"], milestones["before_fdi"]),
        "",
        "Summary",
        "",
        f"total simulations: {combined_sims}",
        f"    systematic: {systematic['simulations']}",
        f"    randomized (seed {seed}): {randomized['simulations']}",
        f"total fault injections: {systematic['fault_injections'] + randomized['fault_injections']}",
        f"successful transparent retries: {systematic['transparent_retries'] + randomized['transparent_retries']}",
        f"Retrain events: {systematic['retrain_events'] + randomized['retrain_events']}",
        f"LinkReset events: {systematic['linkreset_events'] + randomized['linkreset_events']}",
        f"LinkError events: {systematic['linkerror_events'] + randomized['linkerror_events']}",
        "FDI/AXI classification counts (systematic + randomized transactions):",
        f"    COMPLETED: {systematic['fdi_completed'] + randomized['fdi_completed']}",
        f"    NOT_COMPLETED: {systematic['fdi_not_completed'] + randomized['fdi_not_completed']}",
        f"    UNKNOWN: {systematic['fdi_unknown'] + randomized['fdi_unknown']}",
        f"duplicate-commit violations: {systematic['duplicate_commit'] + randomized['duplicate_commit']}",
        f"false-safe-retry violations: {systematic['false_safe_retry'] + randomized['false_safe_retry']}",
        f"deadlocks or liveness failures: {systematic['liveness_failures'] + randomized['liveness_failures']}",
        f"credit invariant violations: {systematic['credit_violations'] + randomized['credit_violations']}",
        f"receiver-overflow violations: {systematic['overflow_violations'] + randomized['overflow_violations']}",
        "",
        "Randomized simulation, initiator class by fault position",
        "(watched transaction only; n is the number of injected faults in that bucket).",
        "A non-zero COMPLETED count means replay or a non-destructive fault still",
        "delivered the response. UNKNOWN rate uses UNKNOWN/n from this table.",
        "",
        format_rate_table(bucket_rates(random_results, "fdi")),
        "",
        "Same runs, UNKNOWN rate if retry disposition is latched (assumption)",
        "",
        format_rate_table(bucket_rates(random_results, "disposition")),
        "",
        "Policy and interpretation differences",
        "",
        *policy_differences(explorer.results),
        "",
        "Conclusion",
        "",
        conclusion(systematic, randomized, gap, duplicate),
        "",
    ]
    return "\n".join(parts) + "\n"
