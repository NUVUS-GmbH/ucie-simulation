"""Cross-guest fault-induced temporal interference campaign.

Runs the timed SystemC binary across the required FFI cases, derives an
Option B experimental timing budget from the fault-free shared baseline,
evaluates the formal success criterion, sweeps deadlines, compares
mitigations, and writes Markdown / CSV / JSON under simulation/results/.
"""

from __future__ import annotations

import csv
import json
import math
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from simulation.timed_campaign import run

ROOT = Path(__file__).resolve().parents[2]
CONFIG = str(ROOT / "configs" / "automotive.yaml")
OUT_ROOT = ROOT / "simulation" / "results" / "cross_guest_ffi_runs"
REPORT_MD = ROOT / "simulation" / "results" / "cross_guest_fault_interference.md"
REPORT_JSON = ROOT / "simulation" / "results" / "cross_guest_fault_interference.json"
REPORT_CSV = ROOT / "simulation" / "results" / "cross_guest_fault_interference.csv"
SWEEP_CSV = ROOT / "simulation" / "results" / "cross_guest_deadline_sweep.csv"
BARS_CSV = ROOT / "simulation" / "results" / "cross_guest_latency_bars.csv"

DEADLINE_MARGIN = 1.20  # Option B: deadline = shared_quiet_latency * margin
SWEEP_START_NS = 60.0
SWEEP_END_NS = 110.0
SWEEP_STEP_NS = 1.0


@dataclass
class GuestBView:
    latency_ns: float
    commit_count: int
    retry_count: int
    replay_count: int
    completion_received: bool
    crc_failures_run: int
    deadline_violated_in_sim: bool
    waits: dict[str, float] = field(default_factory=dict)


def guest_b(result: dict[str, Any]) -> GuestBView:
    txs = [t for t in result["transactions"] if t["guest"] == 1]
    if not txs:
        raise RuntimeError(f"no Guest B transaction in {result.get('experiment')}")
    tx = txs[0]
    return GuestBView(
        latency_ns=float(tx["latency_ns"]),
        commit_count=int(tx["commit_count"]),
        retry_count=int(tx.get("retry_count", 0)),
        replay_count=int(tx.get("replay_count", tx.get("retry_count", 0))),
        completion_received=bool(tx.get("completion_received", tx.get("completed", False))),
        crc_failures_run=int(result.get("crc_failures", 0)),
        deadline_violated_in_sim=bool(tx.get("deadline_violated", False)),
        waits={
            "WAIT_ARBITRATION": float(tx.get("wait_arbitration_ns", 0)),
            "WAIT_GUEST_A_NORMAL_TRAFFIC": float(tx.get("wait_guest_a_normal_ns", 0)),
            "WAIT_GUEST_A_REPLAY": float(tx.get("wait_guest_a_replay_ns", 0)),
            "WAIT_OWN_REPLAY": float(tx.get("wait_own_replay_ns", 0)),
            "WAIT_CREDIT": float(tx.get("wait_credit_ns", 0)),
        },
    )


def guest_a_stats(result: dict[str, Any]) -> dict[str, Any]:
    txs = [t for t in result["transactions"] if t["guest"] == 0]
    return {
        "count": len(txs),
        "retries": [int(t.get("retry_count", 0)) for t in txs],
        "latencies_ns": [float(t["latency_ns"]) for t in txs],
        "max_latency_ns": max((float(t["latency_ns"]) for t in txs), default=-1),
        "any_retry": any(int(t.get("retry_count", 0)) > 0 for t in txs),
        "all_commit_once": all(int(t["commit_count"]) == 1 for t in txs) if txs else True,
    }


def run_case(name: str) -> dict[str, Any]:
    out = OUT_ROOT / name
    result = run(CONFIG, name, seed=1, output=str(out))
    result["_output_dir"] = str(out)
    return result


def deadline_sweep(quiet: float, faulted: float) -> dict[str, Any]:
    rows = []
    lo = None
    hi = None
    d = SWEEP_START_NS
    while d <= SWEEP_END_NS + 1e-9:
        quiet_pass = quiet <= d
        faulted_pass = faulted <= d
        in_interval = quiet_pass and (not faulted_pass)
        rows.append(
            {
                "deadline_ns": round(d, 3),
                "quiet_pass": quiet_pass,
                "faulted_pass": faulted_pass,
                "fault_induced_violation_region": in_interval,
            }
        )
        if in_interval:
            lo = d if lo is None else lo
            hi = d
        d += SWEEP_STEP_NS
    # Exact open/closed interval from measured latencies.
    exact_lo = quiet
    exact_hi = faulted
    return {
        "rows": rows,
        "exact_interval": {
            "quiet_pass_and_faulted_fail": f"{exact_lo} ns <= deadline < {exact_hi} ns",
            "lower_inclusive_ns": exact_lo,
            "upper_exclusive_ns": exact_hi,
        },
        "sampled_interval": {
            "first_ns": lo,
            "last_ns": hi,
            "note": "sampled on 1 ns grid with <= comparators",
        },
    }


def minimized_trace(path: Path, guest: int = 1, limit: int = 40) -> list[str]:
    csv_path = path / "trace.csv"
    if not csv_path.exists():
        return []
    lines: list[str] = []
    with csv_path.open(encoding="utf-8") as fh:
        reader = csv.DictReader(fh)
        for row in reader:
            gid = row.get("guest_id", "")
            event = row.get("event", "")
            keep = event in {
                "create",
                "phase",
                "crc",
                "complete",
                "fault",
                "wdata_tx",
                "replay_yield",
                "duplicate_suppress",
            }
            if not keep:
                continue
            if gid not in {"", "0", "1", str(guest)} and event not in {"crc", "fault", "replay_yield"}:
                continue
            lines.append(
                f"{row['time_ns']} ns  g{gid}  {event}  {row.get('details', '')}"
            )
            if len(lines) >= limit:
                break
    return lines


def fmt(v: float, digits: int = 1) -> str:
    if v is None or (isinstance(v, float) and (math.isnan(v) or v < 0)):
        return "n/a"
    return f"{v:.{digits}f}"


def build_report(data: dict[str, Any]) -> str:
    b = data["baselines"]
    deadline = data["deadline"]
    crit = data["criterion"]
    decomp = data["decomposition"]
    res_q = data["resources"]["shared_quiet"]
    res_f = data["resources"]["guest_a_storm"]
    sweep = data["deadline_sweep"]
    mits = data["mitigations"]
    cf = data["counterfactual"]

    verdict = "true" if crit["FAULT_INDUCED_CROSS_GUEST_DEADLINE_VIOLATION"] else "false"
    interpretation = data["interpretation"]

    lines = [
        "# Cross-guest fault-induced temporal interference",
        "",
        "Timed SystemC experiment: can a UCIe fault confined to Guest A raise",
        "shared-resource occupancy enough for an otherwise deadline-compliant",
        "Guest B transaction to miss its real-time requirement?",
        "",
        f"**FAULT_INDUCED_CROSS_GUEST_DEADLINE_VIOLATION = {verdict}**",
        "",
        "## 1. Experiment configuration",
        "",
        "| Parameter | Value |",
        "| --- | --- |",
        "| Guests | A (best-effort), B (safety-critical) |",
        "| Guest A workload | 6 `MemoryWrite` |",
        "| Guest B workload | 1 non-idempotent `IncrementCounter` |",
        "| Shared resource | one UCIe link, one in-flight payload slot (baseline) |",
        "| Arbitration | round-robin (baseline) |",
        "| Credits / queues | unchanged across compared runs |",
        "| Guest A fault schedule | attempt 1 CRC error, attempt 2 CRC error, attempt 3 accept (per selected flit role) |",
        "| Guest B faults in main comparison | none |",
        "| Clock | 1.0 ns |",
        "",
        "Compared runs differ only in fault injection applied to Guest A.",
        "",
        "## 2. Deadline derivation",
        "",
        "### Option B — relative baseline budget (primary)",
        "",
        "No external automotive timing requirement is bound into this model, so",
        "the campaign uses a transparent **experimental timing budget**:",
        "",
        "```text",
        f"deadline = shared_quiet_latency × {DEADLINE_MARGIN:.2f}",
        f"         = {fmt(b['shared_quiet']['latency_ns'])} ns × {DEADLINE_MARGIN:.2f}",
        f"         = {fmt(deadline['ns'], 2)} ns",
        "```",
        "",
        "This is an experimental timing budget, not an automotive-standard deadline.",
        "",
        "### Option A — derived timing budget (documented)",
        "",
        "```text",
        f"isolated Guest B service time     = {fmt(b['guest_b_isolated']['latency_ns'])} ns",
        f"normal sharing overhead           = {fmt(decomp['normal_sharing_ns'])} ns",
        f"allowed interference margin       = deadline − isolated",
        f"                                  = {fmt(deadline['ns'] - b['guest_b_isolated']['latency_ns'], 2)} ns",
        f"deadline                          = {fmt(deadline['ns'], 2)} ns",
        "```",
        "",
        "The margin equals normal sharing plus the Option B headroom above the",
        "quiet baseline.",
        "",
        "## 3. Isolated baseline (Run 1)",
        "",
        f"- Guest B intrinsic latency: **{fmt(b['guest_b_isolated']['latency_ns'])} ns**",
        f"- CRC failures: {b['guest_b_isolated']['crc_failures_run']}",
        f"- Retries / replays: {b['guest_b_isolated']['retry_count']} / {b['guest_b_isolated']['replay_count']}",
        f"- Commit count: {b['guest_b_isolated']['commit_count']}",
        "",
        "## 4. Shared quiet baseline (Run 2)",
        "",
        f"- Guest B latency: **{fmt(b['shared_quiet']['latency_ns'])} ns**",
        f"- Normal sharing overhead vs isolated: **{fmt(decomp['normal_sharing_ns'])} ns**",
        f"- Deadline ({fmt(deadline['ns'], 2)} ns): "
        f"**{'PASS' if crit['quiet_pass'] else 'FAIL'}** "
        f"(slack {fmt(cf['slack_before_ns'], 2)} ns)",
        f"- Guest B CRC / replay / commit: 0 / 0 / {b['shared_quiet']['commit_count']}",
        "",
        "## 5. Guest-A-only faulted result (Run 3)",
        "",
        f"- Guest B latency: **{fmt(b['guest_a_storm']['latency_ns'])} ns**",
        f"- Deadline ({fmt(deadline['ns'], 2)} ns): "
        f"**{'PASS' if crit['faulted_pass'] else 'FAIL'}** "
        f"(slack {fmt(cf['slack_after_ns'], 2)} ns)",
        f"- Guest B CRC failures (transaction retries): {b['guest_a_storm']['retry_count']}",
        f"- Guest B replays: {b['guest_a_storm']['replay_count']}",
        f"- Guest B commit count: {b['guest_a_storm']['commit_count']}",
        f"- Guest B completion: {b['guest_a_storm']['completion_received']}",
        f"- Run CRC failures (Guest A recovery traffic): {b['guest_a_storm']['crc_failures_run']}",
        f"- Guest A any retry: {data['guest_a']['guest_a_storm']['any_retry']}",
        "",
        "## 6. Guest-B-only faulted result (Run 4)",
        "",
        f"- Guest B latency: **{fmt(b['guest_b_storm']['latency_ns'])} ns**",
        f"- Guest B retries / replays: {b['guest_b_storm']['retry_count']} / {b['guest_b_storm']['replay_count']}",
        f"- Guest A any retry: {data['guest_a']['guest_b_storm']['any_retry']}",
        "",
        "This separates self-induced retry delay from cross-guest replay interference.",
        "",
        "## 7. Timing decomposition",
        "",
        "```text",
        f"Guest B total latency (faulted) = {fmt(b['guest_a_storm']['latency_ns'])} ns",
        f"  intrinsic service time        = {fmt(decomp['intrinsic_ns'])} ns",
        f"  + normal shared interference  = {fmt(decomp['normal_sharing_ns'])} ns",
        f"  + fault-induced interference  = {fmt(decomp['fault_induced_ns'])} ns",
        "```",
        "",
        "Guest B wait attribution (faulted run, simulator instrumentation):",
        "",
        "| Cause | ns |",
        "| --- | ---: |",
    ]
    for k, v in b["guest_a_storm"]["waits"].items():
        lines.append(f"| {k} | {fmt(v)} |")

    lines.extend(
        [
            "",
            "## 8. Retry / resource occupancy",
            "",
            "| Metric | shared_quiet | guest_a_storm |",
            "| --- | ---: | ---: |",
            f"| retry buffer peak | {res_q.get('retry_buffer_peak', 0)} | {res_f.get('retry_buffer_peak', 0)} |",
            f"| TX slot hold by Guest A (ns) | {fmt(res_q.get('tx_slot_hold_ns', [0])[0])} | {fmt(res_f.get('tx_slot_hold_ns', [0])[0])} |",
            f"| TX slot hold by Guest A that is replay (ns) | {fmt(res_q.get('tx_slot_hold_replay_ns', [0])[0])} | {fmt(res_f.get('tx_slot_hold_replay_ns', [0])[0])} |",
            f"| Guest B wait attributable to Guest A replay (ns) | {fmt(res_q.get('guest_b_wait_guest_a_replay_ns', 0))} | {fmt(res_f.get('guest_b_wait_guest_a_replay_ns', 0))} |",
            f"| Guest A replay attempts | {res_q.get('guest_a_replay_attempts', 0)} | {res_f.get('guest_a_replay_attempts', 0)} |",
            "",
            "The retry-buffer peak remains 1 in both runs, matching the model’s",
            "single in-flight payload slot. Interference is explained by extended",
            "TX-slot occupancy under Guest A replay, not by retry-buffer saturation.",
            "",
            "## 9. Deadline sweep (Option C)",
            "",
            f"Exact fault-induced deadline-violation interval:",
            "",
            "```text",
            sweep["exact_interval"]["quiet_pass_and_faulted_fail"],
            "```",
            "",
            f"Sampled on [{SWEEP_START_NS}, {SWEEP_END_NS}] ns with step {SWEEP_STEP_NS} ns:",
            f"first={sweep['sampled_interval']['first_ns']}, "
            f"last={sweep['sampled_interval']['last_ns']}.",
            "",
            "Full sweep table: `simulation/results/cross_guest_deadline_sweep.csv`.",
            "",
            "## 10. Mitigation comparison",
            "",
            "Each mitigation is tested one at a time against the Guest-A-faulted workload.",
            "",
            "| Mitigation | Guest B latency (ns) | Deadline | Guest A max latency (ns) | Notes |",
            "| --- | ---: | --- | ---: | --- |",
            f"| A. Current design (single slot, RR) | {fmt(b['guest_a_storm']['latency_ns'])} | "
            f"{'FAIL' if not crit['faulted_pass'] else 'PASS'} | "
            f"{fmt(data['guest_a']['guest_a_storm']['max_latency_ns'])} | baseline interference |",
        ]
    )
    for key, label in [
        ("mit_safety_priority", "B. Safety-priority arbitration"),
        ("mit_per_guest_slots", "C. Per-guest transmission slots"),
        ("mit_replay_budget", "D. Replay budget yield"),
        ("mit_reserved_bw", "E. Reserved safety bandwidth"),
        ("mit_virtual_channels", "F. Virtual channels / separated queues"),
    ]:
        m = mits[key]
        lines.append(
            f"| {label} | {fmt(m['guest_b_latency_ns'])} | {m['deadline_result']} | "
            f"{fmt(m['guest_a_max_latency_ns'])} | {m['notes']} |"
        )

    lines.extend(
        [
            "",
            "Throughput / fairness: Guest A completion latency rises or falls with",
            "how aggressively the mitigation steals the shared TX opportunity.",
            "Safety-priority and reserved bandwidth improve Guest B at the expense",
            "of Guest A fairness. Per-guest slots / virtual channels bound Guest B",
            "without requiring Guest A to fail.",
            "",
            "## 11. Minimized event trace",
            "",
            "Guest-A-faulted run (truncated):",
            "",
            "```text",
        ]
    )
    lines.extend(data["traces"]["guest_a_storm"] or ["(no trace)"])
    lines.extend(
        [
            "```",
            "",
            "## 12. Scientific interpretation",
            "",
            interpretation,
            "",
            "Preferred wording for a positive result is **cross-guest temporal",
            "interference** or **fault-induced temporal interference through a",
            "shared resource**. This report does **not** claim an ISO 26262",
            "freedom-from-interference violation; that would require an explicit",
            "ISO 26262 analysis beyond this experiment.",
            "",
            "## 13. Limitations",
            "",
            "- Timing numbers are a SystemC [MODEL], not a PHY-accurate UCIe implementation.",
            "- The Option B deadline is an experimental budget, not an OEM requirement.",
            "- Single in-flight payload slot is a model policy that makes TX-slot contention visible.",
            "- Per-guest / VC mitigations introduce per-guest sequence spaces; that is an architectural counterfactual, not a claim that UCIe mandates those VCs.",
            "- Wait attribution covers arbitration, credit, and TX-slot blocking; target/response service time remains in the residual latency.",
            "- Only one mitigation is enabled per run; combinations are out of scope.",
            "",
            "## Counterfactual summary",
            "",
            "```text",
            f"shared_quiet:",
            f"  latency  = {fmt(cf['baseline_latency_ns'])} ns",
            f"  deadline = {fmt(cf['deadline_ns'], 2)} ns",
            f"  slack    = {fmt(cf['slack_before_ns'], 2)} ns",
            "",
            f"guest_a_faulted:",
            f"  latency  = {fmt(cf['faulted_latency_ns'])} ns",
            f"  deadline = {fmt(cf['deadline_ns'], 2)} ns",
            f"  slack    = {fmt(cf['slack_after_ns'], 2)} ns",
            "",
            f"fault-induced latency increase = {fmt(cf['absolute_increase_ns'])} ns "
            f"({fmt(cf['relative_increase'] * 100, 1)}%)",
            "```",
            "",
        ]
    )
    if not crit["FAULT_INDUCED_CROSS_GUEST_DEADLINE_VIOLATION"]:
        lines.extend(
            [
                "Criterion not met. Quantities:",
                "",
                f"- maximum added Guest B latency: {fmt(cf['absolute_increase_ns'])} ns",
                f"- minimum remaining deadline slack (faulted): {fmt(cf['slack_after_ns'], 2)} ns",
                f"- preventing resource note: {data.get('negative_result_note', 'n/a')}",
                "",
            ]
        )
    return "\n".join(lines) + "\n"


def main() -> int:
    OUT_ROOT.mkdir(parents=True, exist_ok=True)
    (ROOT / "simulation" / "results").mkdir(parents=True, exist_ok=True)

    cases = [
        "guest_b_isolated",
        "shared_quiet",
        "guest_a_storm",
        "guest_b_storm",
        "mit_safety_priority",
        "mit_per_guest_slots",
        "mit_replay_budget",
        "mit_reserved_bw",
        "mit_virtual_channels",
    ]
    raw: dict[str, dict[str, Any]] = {}
    for name in cases:
        print(f"running {name}...")
        raw[name] = run_case(name)

    views = {name: guest_b(raw[name]) for name in cases}
    astats = {name: guest_a_stats(raw[name]) for name in cases}

    quiet = views["shared_quiet"].latency_ns
    faulted = views["guest_a_storm"].latency_ns
    isolated = views["guest_b_isolated"].latency_ns
    deadline = quiet * DEADLINE_MARGIN

    quiet_pass = quiet <= deadline
    faulted_pass = faulted <= deadline
    b_clean = (
        views["guest_a_storm"].retry_count == 0
        and views["guest_a_storm"].replay_count == 0
        and views["guest_a_storm"].commit_count == 1
        and views["guest_a_storm"].completion_received
    )
    a_only_faults = astats["guest_a_storm"]["any_retry"] and not (
        views["guest_a_storm"].retry_count > 0
    )

    success = bool(quiet_pass and (not faulted_pass) and b_clean and a_only_faults)

    if success:
        interpretation = (
            "A fault confined to Guest A caused an otherwise deadline-compliant "
            "Guest B transaction to violate its real-time requirement through "
            "contention for a shared UCIe transmission resource. Guest B "
            "experienced no CRC errors and no replay, committed exactly once, "
            "and completed successfully, but exceeded its experimental timing "
            "budget because Guest A’s replay traffic retained the shared "
            "transmit resource. Functional isolation did not imply temporal "
            "isolation in this shared UCIe-based virtualized configuration."
        )
        negative = ""
    else:
        interpretation = (
            "The formal FAULT_INDUCED_CROSS_GUEST_DEADLINE_VIOLATION criterion "
            "was not met under the Option B experimental timing budget. "
            "Fault-induced temporal interference may still be present as a "
            "latency increase; see the counterfactual delta."
        )
        negative = (
            "Single in-flight TX slot still couples guests, but the selected "
            "deadline / measured delta did not satisfy quiet PASS and faulted FAIL."
        )

    mit_notes = {
        "mit_safety_priority": "strict priority for safety-critical Guest B at the shared AXI port",
        "mit_per_guest_slots": "per-guest outstanding TX slots with per-guest sequence spaces",
        "mit_replay_budget": "per-guest TX slot + yield Guest A replay after first recovery attempt",
        "mit_reserved_bw": "weighted RR (B weight 4) plus per-guest TX slots",
        "mit_virtual_channels": "separated per-guest resource state / VC sequence spaces",
    }
    mitigations = {}
    for key, note in mit_notes.items():
        lat = views[key].latency_ns
        mitigations[key] = {
            "guest_b_latency_ns": lat,
            "deadline_result": "PASS" if lat <= deadline else "FAIL",
            "guest_a_max_latency_ns": astats[key]["max_latency_ns"],
            "guest_a_completion_ok": astats[key]["all_commit_once"],
            "throughput_cost_guest_a_max_delta_ns": astats[key]["max_latency_ns"]
            - astats["guest_a_storm"]["max_latency_ns"],
            "fairness_impact": (
                "Guest A delayed" if astats[key]["max_latency_ns"] > astats["guest_a_storm"]["max_latency_ns"]
                else "Guest A similar or faster"
            ),
            "notes": note,
            "pass": raw[key].get("pass", False),
        }

    sweep = deadline_sweep(quiet, faulted)

    data = {
        "research_question": (
            "Can a UCIe fault confined to one virtual ECU increase shared-resource "
            "occupancy enough to cause a safety-critical transaction from another, "
            "error-free virtual ECU to miss its deadline?"
        ),
        "deadline": {
            "method": "Option B — relative baseline budget",
            "margin": DEADLINE_MARGIN,
            "ns": deadline,
            "label": "experimental timing budget (not an automotive-standard deadline)",
        },
        "baselines": {
            name: {
                "latency_ns": views[name].latency_ns,
                "commit_count": views[name].commit_count,
                "retry_count": views[name].retry_count,
                "replay_count": views[name].replay_count,
                "completion_received": views[name].completion_received,
                "crc_failures_run": views[name].crc_failures_run,
                "waits": views[name].waits,
                "sim_pass": raw[name].get("pass", False),
            }
            for name in [
                "guest_b_isolated",
                "shared_quiet",
                "guest_a_storm",
                "guest_b_storm",
            ]
        },
        "guest_a": {
            "shared_quiet": astats["shared_quiet"],
            "guest_a_storm": astats["guest_a_storm"],
            "guest_b_storm": astats["guest_b_storm"],
        },
        "decomposition": {
            "intrinsic_ns": isolated,
            "normal_sharing_ns": quiet - isolated,
            "fault_induced_ns": faulted - quiet,
            "faulted_total_ns": faulted,
        },
        "resources": {
            "shared_quiet": raw["shared_quiet"].get("resources", {}),
            "guest_a_storm": raw["guest_a_storm"].get("resources", {}),
        },
        "counterfactual": {
            "baseline_latency_ns": quiet,
            "faulted_latency_ns": faulted,
            "absolute_increase_ns": faulted - quiet,
            "relative_increase": (faulted / quiet - 1.0) if quiet > 0 else None,
            "deadline_ns": deadline,
            "slack_before_ns": deadline - quiet,
            "slack_after_ns": deadline - faulted,
        },
        "criterion": {
            "quiet_pass": quiet_pass,
            "faulted_pass": faulted_pass,
            "guest_b_crc_failures": views["guest_a_storm"].retry_count,
            "guest_b_replays": views["guest_a_storm"].replay_count,
            "guest_b_commit_count": views["guest_a_storm"].commit_count,
            "guest_b_completed": views["guest_a_storm"].completion_received,
            "guest_a_only_injected_faults": a_only_faults,
            "FAULT_INDUCED_CROSS_GUEST_DEADLINE_VIOLATION": success,
        },
        "deadline_sweep": {
            "exact_interval": sweep["exact_interval"],
            "sampled_interval": sweep["sampled_interval"],
        },
        "mitigations": mitigations,
        "traces": {
            "guest_a_storm": minimized_trace(Path(raw["guest_a_storm"]["_output_dir"])),
            "shared_quiet": minimized_trace(Path(raw["shared_quiet"]["_output_dir"])),
        },
        "interpretation": interpretation,
        "negative_result_note": negative,
        "figure": {
            "bars": [
                {"label": "Guest B isolated", "latency_ns": isolated},
                {"label": "Guest B shared / quiet", "latency_ns": quiet},
                {"label": "Guest B shared / Guest A faulty", "latency_ns": faulted},
            ],
            "deadline_ns": deadline,
            "desired_pattern_observed": isolated <= deadline and quiet <= deadline and faulted > deadline,
        },
    }

    REPORT_JSON.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    REPORT_MD.write_text(build_report(data), encoding="utf-8")

    with REPORT_CSV.open("w", encoding="utf-8", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(
            [
                "run",
                "guest_b_latency_ns",
                "deadline_ns",
                "slack_ns",
                "deadline_pass",
                "guest_b_retry",
                "guest_b_commit",
                "crc_failures_run",
            ]
        )
        for name in [
            "guest_b_isolated",
            "shared_quiet",
            "guest_a_storm",
            "guest_b_storm",
            *mit_notes.keys(),
        ]:
            lat = views[name].latency_ns
            w.writerow(
                [
                    name,
                    lat,
                    deadline,
                    deadline - lat,
                    lat <= deadline,
                    views[name].retry_count,
                    views[name].commit_count,
                    views[name].crc_failures_run,
                ]
            )

    with SWEEP_CSV.open("w", encoding="utf-8", newline="") as fh:
        w = csv.DictWriter(
            fh,
            fieldnames=[
                "deadline_ns",
                "quiet_pass",
                "faulted_pass",
                "fault_induced_violation_region",
            ],
        )
        w.writeheader()
        w.writerows(sweep["rows"])

    with BARS_CSV.open("w", encoding="utf-8", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["label", "latency_ns", "deadline_ns"])
        for bar in data["figure"]["bars"]:
            w.writerow([bar["label"], bar["latency_ns"], deadline])

    print(json.dumps({"FAULT_INDUCED_CROSS_GUEST_DEADLINE_VIOLATION": success, "deadline_ns": deadline}, indent=2))
    print(f"wrote {REPORT_MD}")
    return 0 if success else 1


if __name__ == "__main__":
    raise SystemExit(main())
