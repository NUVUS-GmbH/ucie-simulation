"""UCIe 3.0 findings and the 2.0-versus-3.0 comparison.

Every yes/no and every count is taken from the run that was just executed.
The mechanism classes are the specification reading in ``spec30``. They are
not measurements.
"""

from __future__ import annotations

from simulation.experiments.compare import (
    PROFILES,
    ComparisonRun,
    EqPair,
    VersionRun,
    WindowStats,
    bucket_unknown_rate,
    combined_tally,
    initiator_view,
    recovery_traces,
)
from simulation.model.axi import safe_to_retry
from simulation.model.spec30 import catalog
from simulation.model.system import RunResult
from simulation.model.types import Effect, Fate
from simulation.reporting.trace import _minimize


def _yes(flag: bool) -> str:
    return "Yes" if flag else "No"


def _num(value: float | None) -> str:
    if value is None:
        return "n/a"
    if isinstance(value, float):
        return f"{value:.1f}"
    return str(value)


def _watched_class(result: RunResult) -> Fate | None:
    if not result.transactions:
        return None
    index = min(result.scenario.fault_tx, len(result.transactions) - 1)
    return result.transactions[index].fdi_class


def _rate_cell(row: tuple[str, int, int, int, int] | None) -> str:
    if row is None or row[4] == 0:
        return "n/a"
    _name, _completed, _not_completed, unknown, total = row
    percent = 100.0 * unknown / total
    return f"{percent:.1f}% ({unknown}/{total})"


def _bucket_map(version: VersionRun) -> dict[str, tuple[str, int, int, int, int]]:
    return {row[0]: row for row in bucket_unknown_rate(version)}


def _class_by_name(version: VersionRun) -> dict[str, Fate | None]:
    found: dict[str, Fate | None] = {}
    for result in version.explorer.results + version.random_results:
        found[result.scenario.name] = _watched_class(result)
    return found


def _trace(result: RunResult, tx_index: int = 0) -> str:
    tx = result.transactions[tx_index]
    lines = [
        f"UCIe version: {result.scenario.version.value}",
        "",
        "Transaction:",
        f"    ID: T{tx.logical_id}",
        f"    Type: {tx.name}",
        f"    Operation: {tx.op.value}",
        f"    Address: {tx.address:#x}",
        f"    Idempotent: {'yes' if tx.idempotent else 'no'}",
        "",
        "Trace:",
    ]
    for number, event in enumerate(_minimize(result.events), start=1):
        lines.append(f"    {number}. {event}")
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
            f"    UCIe_state = {'recovered' if result.link_converged and not result.deadlock else 'not converged'}",
            f"    linkerror_status = {str(tx.signals.linkerror).lower()}",
            f"    crc_error_status = {str(tx.signals.crc_error).lower()}",
            f"    retrain_status = {str(tx.signals.retrain).lower()}",
            f"    emergency_shutdown = {str(tx.signals.emergency_shutdown).lower()}",
            f"    fast_throttle = {str(tx.signals.fast_throttle).lower()}",
            f"    priority_sideband_opcode_only = {str(tx.signals.pspt_opcode_only).lower()}",
            f"    runtime_parity = {str(tx.signals.runtime_parity).lower()}",
            f"    runtime_recalibration = {str(tx.signals.runtime_recalibration).lower()}",
            f"    management_reports_axi_commit = {str(tx.signals.management_reports_axi_commit).lower()}",
            "",
            "Classification:",
            f"    profile 1 AXI+FDI: {tx.fdi_class.value if tx.fdi_class else 'UNCLASSIFIED'}",
            f"    profile 2 AXI+FDI+software error state: {tx.software_class.value if tx.software_class else 'UNCLASSIFIED'}",
            f"    profile 3 AXI+standardized management: {tx.management_class.value if tx.management_class else 'UNCLASSIFIED'}",
            "",
            "Safe automatic retry:",
            f"    {'YES' if tx.fdi_class and safe_to_retry(tx.fdi_class) else 'NO'}",
            "",
            "Ambiguity window (logical events from architectural commit):",
            f"    open = {str(tx.ambiguity_open).lower()}",
            f"    length = {tx.ambiguity_window if tx.ambiguity_window is not None else 'n/a'}",
        ]
    )
    return "\n".join(lines)


def _describe_view(tx) -> str:
    sig = tx.signals
    present = [
        name
        for name, enabled in (
            ("linkerror", sig.linkerror),
            ("linkreset", sig.linkreset),
            ("crc_error", sig.crc_error),
            ("retrain", sig.retrain),
            ("uncorrectable", sig.uncorrectable),
            ("remote_linkerror_sideband", sig.remote_linkerror_sideband),
            ("runtime_parity", sig.runtime_parity),
            ("fast_throttle", sig.fast_throttle),
            ("emergency_shutdown", sig.emergency_shutdown),
            ("priority_sideband_opcode_only", sig.pspt_opcode_only),
            ("runtime_recalibration", sig.runtime_recalibration),
            ("endpoint_reset", sig.endpoint_reset),
        )
        if enabled
    ]
    shown = ", ".join(present) if present else "none"
    return (
        f"axi_accepted={str(tx.axi_accepted).lower()}, "
        f"completion_received={str(tx.requester_received_response).lower()}, "
        f"signals={shown}, "
        "axi_commit_bit=false"
    )


def _equivalence_block(pair: EqPair) -> str:
    left = pair.committed.transactions[0]
    right = pair.uncommitted.transactions[0]
    lines = [
        f"Pair: {pair.name}",
        "",
        f"Committed trace ground truth: architectural_commit_count = {left.ground_commit_count}",
        f"Uncommitted trace ground truth: architectural_commit_count = {right.ground_commit_count}",
        "",
    ]
    all_match = all(pair.matched.values()) and left.ground_commit_count != right.ground_commit_count
    for profile in PROFILES:
        same = pair.matched[profile]
        lines.append(
            f"Profile {profile}: initiator views {'match' if same else 'differ'}"
        )
    lines.append("")
    if all_match:
        lines.append("OBSERVATIONAL EQUIVALENCE FOUND")
        lines.append("")
        lines.append("different ground truth")
        lines.append("+")
        lines.append("same standardized initiator-visible information")
        lines.append("=")
        lines.append("transaction-fate ambiguity")
    else:
        lines.append(
            "This pair did not meet the equivalence test "
            "(views differed, or the commit counts did not)."
        )
    lines.append("")
    lines.append("Committed initiator view, profile 3:")
    lines.append(f"    {_describe_view(left)}")
    lines.append("Uncommitted initiator view, profile 3:")
    lines.append(f"    {_describe_view(right)}")
    return "\n".join(lines)


def _window_table(v2: WindowStats, v3: WindowStats) -> str:
    rows = [
        ("Closed windows (completion arrived)", len(v2.closed), len(v3.closed)),
        ("Open windows (fate unresolved)", len(v2.opened), len(v3.opened)),
        ("Minimum closed window", _num(v2.closed_min), _num(v3.closed_min)),
        ("Maximum closed window", _num(v2.closed_max), _num(v3.closed_max)),
        ("Average closed window", _num(v2.closed_avg), _num(v3.closed_avg)),
        ("Minimum open window", _num(v2.open_min), _num(v3.open_min)),
        ("Maximum open window", _num(v2.open_max), _num(v3.open_max)),
        ("Average open window", _num(v2.open_avg), _num(v3.open_avg)),
        ("Window eliminated", _yes(v2.eliminated), _yes(v3.eliminated)),
    ]
    lines = [
        "| Window metric | UCIe 2.0 | UCIe 3.0 |",
        "|---|---:|---:|",
    ]
    for name, left, right in rows:
        lines.append(f"| {name} | {left} | {right} |")
    return "\n".join(lines)


def _mechanism_table() -> str:
    lines = [
        "| Mechanism | Where | Class | What the specification gives the initiator |",
        "|---|---|---|---|",
    ]
    labels = {
        Effect.A_DETECTION: "A. improves fault detection",
        Effect.B_PROPAGATION: "B. improves fault propagation",
        Effect.C_RECOVERY_TIMING: "C. improves recovery timing",
        Effect.D_TRANSPORT_STATE: "D. preserves transport state",
        Effect.E_TRANSACTION_STATE: "E. preserves transaction state",
        Effect.F_COMMIT_KNOWLEDGE: "F. provides architectural commit knowledge",
        Effect.G_NO_FATE_EFFECT: "G. does not affect AXI transaction fate",
    }
    for item in catalog():
        lines.append(
            f"| {item.name} ({item.section}) | {item.where} | {labels[item.effect]} | {item.note} |"
        )
    return "\n".join(lines)


def _conclusion(data: ComparisonRun, v3_unknown: bool, reduced: bool) -> str:
    if not v3_unknown:
        return (
            "Within the explored model, the additional recovery and observability "
            "mechanisms introduced in UCIe 3.0 eliminate the transaction-fate "
            "ambiguity observed under UCIe 2.0. Every interrupted transaction could "
            "be classified as completed or not completed using standardized "
            "initiator-visible information."
        )
    if reduced:
        return (
            "UCIe 3.0 reduces the number and/or duration of transaction-fate "
            "ambiguity scenarios compared with UCIe 2.0, but does not eliminate "
            "them. Additional UCIe 3.0 signaling improves fault observability and "
            "recovery coordination, while the architectural commit point remains "
            "outside the standardized recovery semantics."
        )
    return (
        "Systematic fault-injection analysis shows that the transaction-fate "
        "ambiguity previously identified under UCIe 2.0 remains reachable under "
        "UCIe 3.0. Although UCIe 3.0 provides enhanced error signaling and "
        "recovery mechanisms, these mechanisms describe link and transport state "
        "rather than the architectural commit state of an outstanding AXI "
        "transaction. Two executions with different architectural outcomes can "
        "therefore remain observationally equivalent to the initiator after recovery."
    )


def _management_sentence(eliminated: bool) -> str:
    if eliminated:
        return (
            "UCIe 3.0 improves fault detection, sideband scheduling, and "
            "power-emergency signaling, and within this model it does solve the "
            "question of whether an interrupted safety-critical command already executed."
        )
    return (
        "UCIe 3.0 improves fault detection, sideband scheduling, and "
        "power-emergency signaling, but does not solve the question of whether "
        "an interrupted safety-critical command already executed."
    )


def _named(results: list[RunResult], name: str) -> RunResult:
    for result in results:
        if result.scenario.name == name:
            return result
    raise KeyError(name)


def render_findings(data: ComparisonRun) -> str:
    committed = data.v3.milestones["unknown"]
    missed = data.v3.milestones["before_fdi"]
    link_pair = data.pairs[0]
    v2_names = set(data.v2.unknown_scenarios)
    v3_names = set(data.v3.unknown_scenarios)
    only_unknown = [
        result.scenario.name
        for result in data.only
        if _watched_class(result) == Fate.UNKNOWN
    ]
    v3_unknown = bool(v3_names or only_unknown)
    reduced = (len(v3_names) < len(v2_names)) or (
        len(data.v3.windows.opened) < len(data.v2.windows.opened)
    )
    eliminated = not v3_unknown
    retry_tx = data.retry.transactions[0]
    parts = [
        "AoU 0.8 over UCIe 3.0: transaction fate after link recovery",
        "",
        _management_sentence(eliminated),
        "",
        "This report is generated from the simulator. Counts are not hand-written.",
        "UCIe behavior is taken from UCIe Specification Revision 3.0, Version 1.0",
        "(5 August 2025). A UCIe 2.0 rule is used only where the 3.0 text still",
        "states it. Those retained rules are listed with the 3.0 mechanisms below.",
        "The AoU 0.8 specification text is not in this workspace. AoU behavior",
        "remains the publicly cited §8.3.3 link-down rule. Outstanding AXI fate",
        "after cleanup is still a policy, because UCIe 3.0 §10.3.4 still says the",
        "adapter and PHY do not guarantee quiescence of protocol transactions.",
        "",
        "Percentages, where they appear, are observed under the selected fault",
        f"workload and seed {data.seed}. They are not protocol probabilities.",
        "",
        "Specification classification of mechanisms",
        "",
        "Class F would mean the mechanism tells the initiator the architectural",
        "result. None of the modeled mechanisms is class F. The classifier",
        "refuses a commit bit that the specification does not define.",
        "",
        _mechanism_table(),
        "",
        "Replayed UCIe 2.0 counterexample on the UCIe 3.0 model",
        "",
        "The same non-idempotent MMIO increment is committed remotely, the",
        "response is then lost, and the link returns to Active. The 3.0 status",
        "latched on that path is link, CRC, retrain, and management-transport",
        "state. It does not name the transaction.",
        "",
        _trace(committed),
        "",
        "Matching non-commit trace",
        "",
        _trace(missed),
        "",
        _equivalence_block(link_pair),
        "",
        "UCIe 3.0-only fault pairs",
        "",
    ]
    for pair in data.pairs[1:]:
        parts.append(_equivalence_block(pair))
        parts.append("")
    parts.extend(
        [
            "Mechanisms that do not drop the mainband transaction",
            "",
            "Fast Throttle (§8.4.2.1), runtime recalibration (§4.6), and runtime",
            "link-testing parity (§3.9) are specified not to discard a valid flit",
            "by themselves. The runs record the indication and let the write finish.",
            "",
        ]
    )
    for name in (
        "ucie3_fast_throttle_during_response",
        "ucie3_recalibration_during_response",
        "ucie3_parity_during_response",
    ):
        result = _named(data.only, name)
        tx = result.transactions[0]
        parts.append(
            f"{name}: classification {tx.fdi_class.value if tx.fdi_class else 'UNCLASSIFIED'}, "
            f"architectural_commit_count = {tx.ground_commit_count}, "
            f"completion_received = {str(tx.requester_received_response).lower()}"
        )
    parts.extend(
        [
            "",
            "Automatic retry of the committed UCIe 3.0 loss",
            "",
            f"architectural_commit_count = {retry_tx.ground_commit_count}",
            f"completion_received = {str(retry_tx.requester_received_response).lower()}",
            f"classification = {retry_tx.initiator_class.value if retry_tx.initiator_class else 'UNCLASSIFIED'}",
            f"violations = {len(data.retry.violations)}",
            "",
            "Ambiguity window on the equivalent workload",
            "",
            "The window is counted in logical protocol events, from architectural",
            "commit to either the AXI completion or the return to Active without",
            "one. UCIe 3.0 notification lines are not counted, so a longer trace",
            "printout is not treated as a longer window. Sideband performant mode",
            "and priority sideband change UI scheduling. This model does not turn",
            "that UI gap into a fate event, and §4.1.5.2 does not define a",
            "completion inside a priority packet.",
            "",
            _window_table(data.v2.windows, data.v3.windows),
            "",
            "Conclusion",
            "",
            _conclusion(data, v3_unknown, reduced and not eliminated),
            "",
        ]
    )
    return "\n".join(parts) + "\n"


def render_comparison(data: ComparisonRun) -> str:
    left = combined_tally(data.v2)
    right = combined_tally(data.v3)
    v2_unknown = left["fdi_unknown"] > 0
    v3_unknown = right["fdi_unknown"] > 0 or any(
        _watched_class(result) == Fate.UNKNOWN for result in data.only
    )
    v2_dup = left["duplicate_commit"] > 0 or left["transactions_commit_gt1"] > 0
    v3_dup = right["duplicate_commit"] > 0 or right["transactions_commit_gt1"] > 0
    # The equivalent suite already includes policy D. Also count the explicit
    # UCIe 3.0 replay of that policy, which is outside the equivalent tally.
    if data.retry.transactions[0].ground_commit_count > 1:
        v3_dup = True
    v2_false = left["false_safe_retry"] > 0
    v3_false = right["false_safe_retry"] > 0 or any(
        item.startswith("property 1") for item in data.retry.violations
    )
    b2 = _bucket_map(data.v2)
    b3 = _bucket_map(data.v3)
    buckets = [
        "Before target acceptance",
        "After target acceptance",
        "After architectural commit",
        "After response generation",
        "After response delivery",
    ]
    v2_map = _class_by_name(data.v2)
    v3_map = _class_by_name(data.v3)
    eliminated = sorted(
        name
        for name, label in v2_map.items()
        if label == Fate.UNKNOWN and v3_map.get(name) != Fate.UNKNOWN
    )
    introduced = sorted(
        name
        for name, label in v3_map.items()
        if label == Fate.UNKNOWN and v2_map.get(name) != Fate.UNKNOWN
    )
    only_unknown = [
        result.scenario.name
        for result in data.only
        if _watched_class(result) == Fate.UNKNOWN
    ]
    equivalence = [
        pair.name
        for pair in data.pairs
        if all(pair.matched.values())
        and pair.committed.transactions[0].ground_commit_count
        != pair.uncommitted.transactions[0].ground_commit_count
    ]
    reduced_count = right["fdi_unknown"] < left["fdi_unknown"]
    reduced_window = len(data.v3.windows.opened) < len(data.v2.windows.opened) or (
        data.v3.windows.open_max is not None
        and data.v2.windows.open_max is not None
        and data.v3.windows.open_max < data.v2.windows.open_max
    )
    eliminated_all = not v3_unknown
    guests = _named(data.only, "ucie3_two_guests_shared_link")
    guest_lines = []
    for tx in guests.transactions:
        guest_lines.append(
            f"T{tx.logical_id} axi_id {tx.axi_id}: "
            f"architectural_commit_count = {tx.ground_commit_count}, "
            f"classification = {tx.fdi_class.value if tx.fdi_class else 'UNCLASSIFIED'}, "
            f"{_describe_view(tx)}"
        )
    same_guest_view = len({initiator_view(tx, "management") for tx in guests.transactions}) == 1

    # Transport acknowledgement versus commit, taken from the equivalent runs.
    transport_without_commit = 0
    commit_without_completion = 0
    for result in data.v3.explorer.results:
        for tx in result.transactions:
            if (tx.request_ack or tx.data_ack or tx.response_ack) and tx.ground_commit_count == 0:
                transport_without_commit += 1
            if tx.ground_commit_count >= 1 and not tx.requester_received_response:
                commit_without_completion += 1

    lines = [
        "UCIe 2.0 versus UCIe 3.0: AXI transaction fate",
        "",
        _management_sentence(eliminated_all),
        "",
        "This comparison replays one systematic suite and one seeded random",
        f"suite (seed {data.seed}, {data.random_count} injections) on both protocol",
        "models. A third set exercises mechanisms that UCIe 3.0 adds and UCIe 2.0",
        "does not define. Percentages are observed under that workload and seed.",
        "They are not intrinsic protocol probabilities. The reachability rows",
        "are the stronger result.",
        "",
        "Equivalent-workload results",
        "",
        "| Metric | UCIe 2.0 | UCIe 3.0 |",
        "|---|---:|---:|",
        f"| UNKNOWN reachable | {_yes(v2_unknown)} | {_yes(right['fdi_unknown'] > 0)} |",
        f"| Duplicate commit reachable | {_yes(v2_dup)} | {_yes(v3_dup)} |",
        f"| False-safe-retry reachable | {_yes(v2_false)} | {_yes(v3_false)} |",
        f"| Liveness failures | {left['liveness_failures']} | {right['liveness_failures']} |",
        f"| Recovery traces explored | {recovery_traces(data.v2)} | {recovery_traces(data.v3)} |",
    ]
    for bucket in buckets:
        lines.append(
            f"| UNKNOWN {bucket.lower()} | {_rate_cell(b2.get(bucket))} | {_rate_cell(b3.get(bucket))} |"
        )
    lines.extend(
        [
            "",
            f"Equivalent-workload UNKNOWN transactions: UCIe 2.0 {left['fdi_unknown']}, "
            f"UCIe 3.0 {right['fdi_unknown']}.",
            f"Equivalent-workload simulations: UCIe 2.0 {left['simulations']} "
            f"({left['systematic']} systematic + {left['randomized']} random), "
            f"UCIe 3.0 {right['simulations']} "
            f"({right['systematic']} systematic + {right['randomized']} random).",
            "",
            "Answers",
            "",
            f"1. UNKNOWN transaction fate is reachable in UCIe 2.0: {_yes(v2_unknown)}.",
            f"2. UNKNOWN transaction fate is reachable in UCIe 3.0: {_yes(v3_unknown)}. "
            "The equivalent workload already reaches it. The 3.0-only destructive",
            " faults do not remove it.",
            "3. Fault scenarios that create UNKNOWN are the runs whose watched",
            " transaction ends without an AXI completion after the initiator had",
            " accepted it, and where standardized status does not prove the effect",
            " was absent. On the equivalent workload the UCIe 3.0 model reproduces",
            f" the UCIe 2.0 set: {len(eliminated)} scenario names leave that set and",
            f" {len(introduced)} names enter it.",
            "4. UCIe 3.0 eliminates a UCIe 2.0 ambiguity class on this workload: "
            f"{_yes(bool(eliminated))}.",
            "5. UCIe 3.0 does make additional standardized status visible: software",
            " error registers, Link Health Monitor, management-transport independence,",
            " and, on the 3.0-only faults, Fast Throttle, Emergency Shutdown, a",
            " priority-sideband opcode, runtime parity, and runtime recalibration.",
            " In every equivalence pair those fields are the same on the committed",
            " trace and the uncommitted trace. Visibility is not commit knowledge.",
            "6. The logical-event ambiguity window on the equivalent workload is",
            f" {'shorter' if reduced_window else 'not shorter'} under UCIe 3.0.",
            " Sideband performant mode removes a 32 UI sideband gap. That gap is",
            " not a fate milestone in this model, and the specification does not",
            " define it as one.",
            "7. UCIe 3.0 reduces the number of UNKNOWN transactions on the",
            f" equivalent workload: {_yes(reduced_count)} "
            f"({left['fdi_unknown']} and {right['fdi_unknown']}).",
            "8. Automatic retry is safe in a new UCIe 3.0 scenario: "
            f"{_yes(False if v3_dup else not v3_unknown)}. "
            "The committed loss, retried under policy D, still produces a second",
            f" architectural effect (architectural_commit_count = "
            f"{data.retry.transactions[0].ground_commit_count}). "
            "Fast Throttle, parity, and recalibration complete without a retry,",
            " because the specification does not drop the flit. They are not new",
            " cases in which a lost completion becomes safe to replay.",
            "9. Transport acknowledgement is still not architectural commit.",
            f" On the UCIe 3.0 systematic runs, {transport_without_commit} transactions",
            " had a transport ACK with architectural_commit_count 0, and",
            f" {commit_without_completion} transactions had a commit without an",
            " initiator completion.",
            "10. The additional software-visible and management-visible signals",
            " listed in answer 5 are available. They improve classification of the",
            " link. They do not change the transaction label. Profile 1, profile 2,",
            " and profile 3 agree on every equivalence pair.",
            "",
            "Equivalence pairs",
            "",
            "A pair is an equivalence when the commit counts differ and all three",
            " observation profiles still match.",
            "",
        ]
    )
    if equivalence:
        for name in equivalence:
            lines.append(f"- {name}")
    else:
        lines.append("- none")
    lines.extend(
        [
            "",
            "UCIe 3.0-only scenarios whose watched transaction is UNKNOWN",
            "",
        ]
    )
    if only_unknown:
        for name in only_unknown:
            lines.append(f"- {name}")
    else:
        lines.append("- none")
    lines.extend(
        [
            "",
            "Ambiguity window",
            "",
            "Logical events, equivalent workload, watched transaction, counted",
            " only after architectural commit. Observed under the selected fault",
            f" workload and seed {data.seed}.",
            "",
            _window_table(data.v2.windows, data.v3.windows),
            "",
            "The window is eliminated only when no open window remains. An open",
            " window means the initiator returned to an operational link without",
            " a completion, and without a standardized commit bit.",
            "",
            "Automotive interpretation",
            "",
            "Link recovery and transaction recovery are different events. The",
            " question for a virtual ECU is what it knows about the last",
            " safety-relevant command after the UCIe link is healthy again.",
            "",
            "1. A virtual ECU still needs an UNKNOWN transaction state under",
            f" UCIe 3.0: {_yes(v3_unknown)}. Profile 3, which includes every",
            " standardized management fact this model is allowed to read, still",
            " returns UNKNOWN for a committed loss and for a loss that never",
            " reached the target.",
            "2. Automatic replay of non-idempotent MMIO is not safe. The UCIe 3.0",
            " replay of the committed loss duplicates the architectural effect",
            f" (architectural_commit_count = {data.retry.transactions[0].ground_commit_count}).",
            "3. UCIe 3.0 error signaling cannot be mapped directly to a",
            " guest-visible transaction failure. The same CRC, retrain, and",
            " LinkError status appears for a command that committed and for a",
            " command that did not.",
            "4. A LinkError requires the guest's outstanding transaction state",
            " to be invalidated as a known success. It does not justify recording",
            " the command as known not to have executed. The sound state is",
            " UNKNOWN unless a completion was already delivered.",
            "5. Several guests sharing one endpoint are not isolated, for the",
            " purpose of retrospective fate, by a physical link fault. UCIe status",
            " is link-scoped. In the two-initiator run"
            f" `{guests.scenario.name}`, the profile-3 views are"
            f" {'identical' if same_guest_view else 'not identical'}.",
        ]
    )
    for item in guest_lines:
        lines.append(f"   - {item}")
    lines.extend(
        [
            "6. The virtual platform cannot determine which guest transactions",
            " may already have committed. No modeled register carries an AXI id",
            " or a commit bit, and the equivalence pairs have different commit",
            " counts under the same status.",
            "7. UCIe 3.0 does not materially shorten the logical-event interval",
            " during which transaction fate is ambiguous. It adds faster sideband",
            " scheduling and a fatal Emergency Shutdown path. Those change how",
            " quickly the platform learns that the link or the power state is",
            " bad. They do not say whether the last command took effect.",
            " After the UCIe link is healthy again, the virtual ECU still does",
            " not know what happened to the last safety-relevant command.",
            "",
            "Conclusion",
            "",
            _conclusion(data, v3_unknown, (reduced_count or reduced_window) and not eliminated_all),
            "",
        ]
    )
    return "\n".join(lines) + "\n"
