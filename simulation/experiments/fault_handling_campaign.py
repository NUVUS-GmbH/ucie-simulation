#!/usr/bin/env python3
"""Run the AXI-over-UCIe-S fault-handling experiment matrix (draft V0.1).

Writes results under simulation/results/fault_handling_runs/ and a summary
JSON/Markdown under simulation/results/fault_handling_*. Historical result
trees are not overwritten.

Also repeats central fault cases on the FIFO/credit/multi-outstanding
reference config and documents shortened watchdog vs retry/recovery times.
"""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, Optional, Tuple

ROOT = Path(__file__).resolve().parents[2]
BIN = ROOT / "build" / "ucie_sim"
CFG_AUTO = ROOT / "configs" / "automotive.yaml"
CFG_REF = ROOT / "configs" / "torsten_reference_ucie2x.yaml"
OUT = ROOT / "simulation" / "results" / "fault_handling_runs"
OUT_REF = ROOT / "simulation" / "results" / "fault_handling_reference_runs"
TIMED = ROOT / "timed_results" / "fault_handling"
TIMED_REF = ROOT / "timed_results" / "fault_handling_reference"

EXPERIMENTS = [
    "fh_clean_success",
    "fh_loss_before_accept",
    "fh_commit_then_local_slverr",
    "fh_no_commit_local_slverr",
    "fh_watchdog_then_late_okay",
    "fh_timeout_then_remote_commit",
    "fh_blind_retry_after_slverr",
    "fh_no_retry_after_slverr",
    "fh_axi_id_reuse_late_resp",
    "fh_multi_outstanding_backpressure",
    "fh_read_burst_error_completion",
    "fh_race_resp_before_timeout",
    "fh_race_resp_simultaneous",
    "fh_race_resp_after_timeout",
    "fh_classifier_diag_compare",
    "fh_diag_ack_loss_after_delivery",
    "fh_observer_history_compare",
    "fh_safe_retry_cancel_remote",
    "fh_cancel_latency_race",
]

# Central error cases repeated on the reference architecture config.
REFERENCE_EXPERIMENTS = [
    "fh_clean_success",
    "fh_commit_then_local_slverr",
    "fh_no_commit_local_slverr",
    "fh_watchdog_then_late_okay",
    "fh_timeout_then_remote_commit",
    "fh_blind_retry_after_slverr",
    "fh_read_burst_error_completion",
    "fh_multi_outstanding_backpressure",
    "fh_classifier_diag_compare",
    "fh_diag_ack_loss_after_delivery",
    "fh_safe_retry_cancel_remote",
    "fh_cancel_latency_race",
]

# Watchdog / recovery timing notes (ns). Campaign shortens WDT for discrete-event
# runs; UCIe residency/replay remain at spec/config values unless overridden.
WATCHDOG_DOC = {
    "draft_wdt_timeout_start_ns": 1_000_000,  # ~1 ms REFERENCE CONFIGURATION
    "campaign_shortened_examples_ns": {
        "fh_watchdog_then_late_okay": 200,
        "fh_timeout_then_remote_commit": 100,
        "fh_race_resp_after_timeout": 100,
    },
    "ucie_link_error_residency_ns_spec": 16_000_000,
    "ucie_replay_timeout_flits_spec": 375,
    "automotive_retrain_ns": 5000,
    "automotive_link_reset_ns": 10000,
    "note": (
        "Shortened txn_watchdog_ns values are SIMULATION CONFIGURATION so the "
        "watchdog can fire inside a discrete-event run. They are not claims that "
        "1 ms draft WDT is shorter than UCIe retry/recovery. In a real system, "
        "WDT_TIMEOUT must exceed max latency including retry + retrain (draft WDT-02)."
    ),
}


def run_one(name: str, cfg: Path, timed_root: Path, archive_root: Path) -> dict:
    dest = timed_root / name
    # Fresh directory: a failed process must not inherit a prior result.json PASS.
    if dest.exists():
        shutil.rmtree(dest)
    dest.mkdir(parents=True, exist_ok=True)
    cmd = [
        str(BIN),
        "--config",
        str(cfg),
        "--experiment",
        name,
        "--seed",
        "1",
        "--output",
        str(dest),
    ]
    proc = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    result_path = dest / "result.json"
    if not result_path.exists():
        return {
            "experiment": name,
            "pass": False,
            "config": str(cfg.relative_to(ROOT)),
            "failures": [
                f"no result.json; exit={proc.returncode}",
                proc.stdout[-500:],
                proc.stderr[-500:],
            ],
        }
    data = json.loads(result_path.read_text())
    data["stdout_tail"] = "\n".join(proc.stdout.strip().splitlines()[-12:])
    data["config"] = str(cfg.relative_to(ROOT))
    archive = archive_root / name
    if archive.exists():
        shutil.rmtree(archive)
    shutil.copytree(dest, archive)
    return data


def events_only(rows: list[str]) -> list[str]:
    """Strip timestamps: keep only the event token after '|'."""
    out = []
    for row in rows:
        if "|" in row:
            out.append(row.split("|", 1)[1])
        else:
            out.append(row)
    return out


def diff_event_sequences(left: list[str], right: list[str]) -> dict:
    """Compare projected event *names* after removing timestamps."""
    a = events_only(left)
    b = events_only(right)
    only_left = [e for e in a if e not in b]
    only_right = [e for e in b if e not in a]
    return {
        "equal": a == b,
        "left_len": len(a),
        "right_len": len(b),
        "only_left": only_left,
        "only_right": only_right,
    }


def diff_timed_traces(left: list[str], right: list[str]) -> dict:
    """Compare full rows including timestamps (time-observing observers)."""
    only_left = [e for e in left if e not in right]
    only_right = [e for e in right if e not in left]
    return {
        "equal": left == right,
        "left_len": len(left),
        "right_len": len(right),
        "only_left": only_left[:20],
        "only_right": only_right[:20],
        "note": "Full 'time_ns|event' rows; equality requires identical timing.",
    }


def _pair_txs(rows: list[dict]) -> Optional[Tuple[Dict[str, Any], dict, dict]]:
    by_name = {r.get("experiment"): r for r in rows}
    paired = by_name.get("fh_observer_history_compare")
    if paired and len(paired.get("transactions", [])) >= 2:
        return paired, paired["transactions"][0], paired["transactions"][1]
    commit = by_name.get("fh_commit_then_local_slverr")
    no_commit = by_name.get("fh_no_commit_local_slverr")
    if not commit or not no_commit:
        return None
    t0 = (commit.get("transactions") or [{}])[0]
    t1 = (no_commit.get("transactions") or [{}])[0]
    return {"mode": "cross_run"}, t0, t1


def observer_history_report(rows: list[dict]) -> dict:
    """Commit vs no-commit observer-history comparison.

    Reports two independent findings:
      - event_sequence: timestamps stripped (event names only)
      - timed: full rows including timestamps
    """
    paired = _pair_txs(rows)
    if paired is None:
        return {"mode": "unavailable", "reason": "missing commit/no-commit runs"}
    meta, t0, t1 = paired
    mode = "single_run_two_txs" if meta.get("experiment") == "fh_observer_history_compare" else "cross_run"
    report = {
        "mode": mode,
        "experiment": meta.get("experiment", "cross_run"),
        "allowed_observations": {
            "A": "AXI handshakes + response codes (no internal RESP source)",
            "B": "A + FDI link-down",
            "C": "B + standardized status + optional DIAG when exposed",
            "D": "C + disposition / late-discard / RESP source",
        },
        "definitions": {
            "event_sequence_equal": (
                "Same projected event *names* after removing timestamps "
                "(time_ns|event → event). Valid for observers that ignore timing."
            ),
            "timed_equal": (
                "Identical full history rows including timestamps. Required for "
                "time-observing observers; usually false across independent txs."
            ),
        },
        "profiles": {},
    }
    for prof in ("a", "b", "c", "d"):
        key = f"observer_trace_{prof}"
        left, right = t0.get(key, []), t1.get(key, [])
        report["profiles"][prof] = {
            "event_sequence": diff_event_sequences(left, right),
            "timed": diff_timed_traces(left, right),
        }
    if mode == "single_run_two_txs":
        report["fate_equal_abc"] = meta.get("observational_equivalence_abc")
        report["fate_equal_d"] = meta.get("observational_equivalence_d")
    return report


def summarize(rows: list[dict], campaign: str) -> dict:
    summary = {
        "campaign": campaign,
        "experiments": [],
        "all_pass": True,
        "watchdog_timing": WATCHDOG_DOC,
    }
    for row in rows:
        txs = row.get("transactions", [])
        entry = {
            "experiment": row.get("experiment"),
            "pass": bool(row.get("pass")),
            "config": row.get("config"),
            "failures": row.get("failures", []),
            "counter": row.get("counter"),
            "txn_watchdog_fires": row.get("txn_watchdog_fires"),
            "late_responses_discarded": row.get("late_responses_discarded"),
            "single_inflight_slot": row.get("single_inflight_slot"),
            "transactions": [
                {
                    "id": t.get("id"),
                    "commit_count": t.get("commit_count"),
                    "fate_a": t.get("fate_a"),
                    "fate_b": t.get("fate_b"),
                    "fate_c": t.get("fate_c"),
                    "fate_d": t.get("fate_d"),
                    "fate_c_without_diag": t.get("fate_c_without_diag"),
                    "fate_d_without_diag": t.get("fate_d_without_diag"),
                    "axi_resp": t.get("axi_resp"),
                    "resp_source": t.get("resp_source"),
                    "proto_state": t.get("proto_state"),
                    "knowledge_a": t.get("knowledge_a"),
                    "late_responses_discarded": t.get("late_responses_discarded"),
                    "software_retries": t.get("software_retries"),
                    "axi_id": t.get("axi_id"),
                    "bridge_token": t.get("bridge_token"),
                    "completing_token": t.get("completing_token"),
                    "allow_remote_continue": t.get("allow_remote_continue"),
                    "beats_responded": t.get("beats_responded"),
                    "beat_handshake_count": t.get("beat_handshake_count"),
                    "resp_backpressured": t.get("resp_backpressured"),
                    "burst_len": t.get("burst_len"),
                    "observer_trace_a": t.get("observer_trace_a", []),
                    "observer_trace_b": t.get("observer_trace_b", []),
                    "observer_trace_c": t.get("observer_trace_c", []),
                    "observer_trace_d": t.get("observer_trace_d", []),
                }
                for t in txs
            ],
        }
        if not entry["pass"]:
            summary["all_pass"] = False
        summary["experiments"].append(entry)
    summary["observer_history_compare"] = observer_history_report(rows)
    return summary


def write_md(summary: dict, path: Path) -> None:
    lines = [
        f"# Fault-handling campaign results ({summary['campaign']})",
        "",
        "Separate result area for the AXI4↔AXI4-over-UCIe-S fault-handling extension.",
        "Historical `reference_architecture_runs/` and prior findings are unchanged.",
        "",
        f"All pass: **{summary['all_pass']}**",
        "",
        "| Experiment | Pass | Commit(s) | AXI resp/source | Fate A/C/D | Late discard |",
        "| --- | --- | --- | --- | --- | --- |",
    ]
    for e in summary["experiments"]:
        txs = e["transactions"]
        commits = ",".join(str(t["commit_count"]) for t in txs) or "-"
        resp = ",".join(f"{t.get('axi_resp')}/{t.get('resp_source')}" for t in txs) or "-"
        fate = ",".join(
            f"{t.get('fate_a')}/{t.get('fate_c')}/{t.get('fate_d')}" for t in txs
        ) or "-"
        late = e.get("late_responses_discarded")
        lines.append(
            f"| `{e['experiment']}` | {e['pass']} | {commits} | {resp} | {fate} | {late} |"
        )

    oh = summary.get("observer_history_compare") or {}
    lines += [
        "",
        "## Observer-history compare (commit vs no-commit)",
        "",
        f"Mode: `{oh.get('mode')}`",
        "",
        "Allowed observations:",
    ]
    for k, v in (oh.get("allowed_observations") or {}).items():
        lines.append(f"- **{k}**: {v}")
    lines.append("")
    lines.append("")
    lines.append("### Event-sequence equality (timestamps stripped)")
    lines.append("")
    for prof, diff in (oh.get("profiles") or {}).items():
        es = diff.get("event_sequence") or diff
        lines.append(
            f"- Profile {prof.upper()}: equal={es.get('equal')} "
            f"only_left={es.get('only_left')} only_right={es.get('only_right')}"
        )
    lines.append("")
    lines.append("### Timed equality (timestamps retained)")
    lines.append("")
    for prof, diff in (oh.get("profiles") or {}).items():
        td = diff.get("timed")
        if not td:
            lines.append(f"- Profile {prof.upper()}: *(not reported in this summary)*")
            continue
        lines.append(
            f"- Profile {prof.upper()}: equal={td.get('equal')} "
            f"left_len={td.get('left_len')} right_len={td.get('right_len')}"
        )
    lines.append("")
    lines.append(
        "Note: „A/B equal“ in the event-sequence sense does **not** imply "
        "timed observational equivalence."
    )

    wd = summary.get("watchdog_timing") or {}
    lines += [
        "",
        "## Watchdog vs retry/recovery timing",
        "",
        wd.get("note", ""),
        "",
        f"- Draft WDT start: {wd.get('draft_wdt_timeout_start_ns')} ns",
        f"- UCIe LinkError residency (spec): {wd.get('ucie_link_error_residency_ns_spec')} ns",
        f"- UCIe replay timeout flits (spec): {wd.get('ucie_replay_timeout_flits_spec')}",
        f"- Automotive retrain_ns: {wd.get('automotive_retrain_ns')}",
        f"- Campaign shortened WDT examples: {wd.get('campaign_shortened_examples_ns')}",
        "",
    ]
    path.write_text("\n".join(lines) + "\n")


def main() -> int:
    if not BIN.exists():
        print(f"missing binary {BIN}; build with cmake --build build", file=sys.stderr)
        return 2
    OUT.mkdir(parents=True, exist_ok=True)
    OUT_REF.mkdir(parents=True, exist_ok=True)
    TIMED.mkdir(parents=True, exist_ok=True)
    TIMED_REF.mkdir(parents=True, exist_ok=True)

    rows = [run_one(name, CFG_AUTO, TIMED, OUT) for name in EXPERIMENTS]
    summary = summarize(rows, "fault_handling_v0.1")
    (ROOT / "simulation/results/fault_handling_summary.json").write_text(
        json.dumps(summary, indent=2) + "\n"
    )
    write_md(summary, ROOT / "simulation/results/fault_handling_summary.md")
    (
        ROOT / "simulation/results/fault_handling_observer_diff.json"
    ).write_text(json.dumps(summary.get("observer_history_compare"), indent=2) + "\n")

    ref_rows = [run_one(name, CFG_REF, TIMED_REF, OUT_REF) for name in REFERENCE_EXPERIMENTS]
    ref_summary = summarize(ref_rows, "fault_handling_reference_ucie2x")
    (ROOT / "simulation/results/fault_handling_reference_summary.json").write_text(
        json.dumps(ref_summary, indent=2) + "\n"
    )
    write_md(ref_summary, ROOT / "simulation/results/fault_handling_reference_summary.md")

    print(
        json.dumps(
            {
                "automotive_all_pass": summary["all_pass"],
                "reference_all_pass": ref_summary["all_pass"],
                "n_auto": len(rows),
                "n_ref": len(ref_rows),
            },
            indent=2,
        )
    )
    return 0 if summary["all_pass"] and ref_summary["all_pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
