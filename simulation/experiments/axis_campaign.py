#!/usr/bin/env python3
"""Run the AXIS-P1-Continuous experiment matrix over UCIe transport.

Writes results under simulation/results/axis_p1_*/ without overwriting the
Phase-1 MM baseline archive or fault_handling_* trees.
"""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

ROOT = Path(__file__).resolve().parents[2]
BIN = ROOT / "build" / "ucie_sim"
CFG_AUTO = ROOT / "configs" / "automotive.yaml"
CFG_REF = ROOT / "configs" / "torsten_reference_ucie2x.yaml"
OUT = ROOT / "simulation" / "results" / "axis_p1_runs"
OUT_REF = ROOT / "simulation" / "results" / "axis_p1_reference_runs"
TIMED = ROOT / "timed_results" / "axis_p1"
TIMED_REF = ROOT / "timed_results" / "axis_p1_reference"
CFG_DUMP = ROOT / "simulation" / "results" / "axis_p1_effective_configs"

# Full automotive matrix (Phase-1 06_experiment_matrix.md + extensions).
EXPERIMENTS = [
    "ax_clean_packet_imm",
    "ax_clean_packet_pkt",
    "ax_loss_mid_packet_imm",
    "ax_loss_mid_packet_pkt",
    "ax_loss_before_first_imm",
    "ax_loss_before_first_pkt",
    "ax_tlast_local_remote_loss",
    "ax_imm_partial_then_blind_retry",
    "ax_pkt_commit_then_local_timeout",
    "ax_ack_loss_after_delivery",
    "ax_observer_timed_vs_seq",
    "ax_safe_retry_cancel0",
    "ax_cancel_latency_race",
    "ax_backpressure_tready",
    "ax_continuous_tid_stable",
    "ax_appack_loss",
    "ax_reset_partial_packet",
    "ax_late_data_after_restart",
    "ax_multi_packet_inflight",
    "ax_packet_len_variants",
    "ax_replay_no_dup_effect",
    "ax_app_level_repeat_dup",
    "ax_dup_suppress",
    "ax_packet_id_reuse",
]

# Reference-architecture subset (timing differs; fate classes remain reachable).
REFERENCE_EXPERIMENTS = [
    "ax_clean_packet_imm",
    "ax_clean_packet_pkt",
    "ax_loss_mid_packet_imm",
    "ax_loss_before_first_imm",
    "ax_ack_loss_after_delivery",
    "ax_backpressure_tready",
    "ax_safe_retry_cancel0",
]


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
                (proc.stdout or "")[-500:],
                (proc.stderr or "")[-500:],
            ],
            "effective_config": {},
        }
    data = json.loads(result_path.read_text())
    data["stdout_tail"] = "\n".join((proc.stdout or "").strip().splitlines()[-12:])
    data["config"] = str(cfg.relative_to(ROOT))
    # Scenario overrides are embedded in the binary scenario; capture run knobs.
    data["effective_config"] = {
        "config_file": data["config"],
        "experiment": name,
        "seed": 1,
        "axis_profile": "AXIS-P1-Continuous",
        "bridge_policy": "one_ucie_segment_per_transfer",
        "aou_mapping_claim": "MODEL PROFILE only; AoU full text absent",
        "single_inflight_slot": data.get("single_inflight_slot"),
        "retry_buffer_entries": data.get("retry_buffer_entries"),
        "tx_buffer_entries": data.get("tx_buffer_entries"),
        "rx_buffer_entries": data.get("rx_buffer_entries"),
        "crc_enabled": data.get("crc_enabled"),
        "retry_enabled": data.get("retry_enabled"),
        "ucie_revision": data.get("ucie_revision"),
        "transactions_axis": [
            {
                "id": t.get("id"),
                "sink": t.get("axis_sink"),
                "local_beats": t.get("axis_local_beats"),
                "remote_beats": t.get("axis_remote_beats"),
                "remote_packet_complete": t.get("axis_remote_packet_complete"),
                "processing_started": t.get("axis_processing_started"),
                "app_effects": t.get("axis_app_effects"),
                "bytes_committed": t.get("axis_bytes_committed"),
                "commit_count": t.get("commit_count"),
                "fate_a": t.get("fate_a"),
                "fate_c": t.get("fate_c"),
                "fate_d": t.get("fate_d"),
            }
            for t in data.get("transactions", [])
            if t.get("axis_enabled")
        ],
    }
    archive = archive_root / name
    if archive.exists():
        shutil.rmtree(archive)
    shutil.copytree(dest, archive)
    CFG_DUMP.mkdir(parents=True, exist_ok=True)
    tag = cfg.stem + "__" + name
    (CFG_DUMP / f"{tag}.json").write_text(
        json.dumps(data["effective_config"], indent=2) + "\n"
    )
    return data


def events_only(rows: list[str]) -> list[str]:
    out = []
    for row in rows:
        if "|" in row:
            out.append(row.split("|", 1)[1])
        else:
            out.append(row)
    return out


def diff_event_sequences(left: list[str], right: list[str]) -> dict:
    a = events_only(left)
    b = events_only(right)
    return {
        "equal": a == b,
        "left_len": len(a),
        "right_len": len(b),
        "only_left": [e for e in a if e not in b][:20],
        "only_right": [e for e in b if e not in a][:20],
        "missing_in_right": [e for e in a if e not in b][:20],
        "extra_in_right": [e for e in b if e not in a][:20],
        "reordered": a != b and sorted(a) == sorted(b),
    }


def diff_timed_traces(left: list[str], right: list[str]) -> dict:
    return {
        "equal": left == right,
        "left_len": len(left),
        "right_len": len(right),
        "only_left": [e for e in left if e not in right][:20],
        "only_right": [e for e in right if e not in left][:20],
    }


def observer_compare(rows: list[dict]) -> dict:
    by_name = {r.get("experiment"): r for r in rows}
    obs = by_name.get("ax_observer_timed_vs_seq")
    if not obs or len(obs.get("transactions", [])) < 2:
        return {"mode": "unavailable"}
    t0, t1 = obs["transactions"][0], obs["transactions"][1]
    report: Dict[str, Any] = {
        "mode": "single_run_two_txs",
        "experiment": "ax_observer_timed_vs_seq",
        "ground_truth": {
            "tx0_commit": t0.get("commit_count"),
            "tx1_commit": t1.get("commit_count"),
        },
        "projection": {
            "event_sequence": "strip timestamp before '|'; compare event tokens",
            "timed": "full 'time_ns|event' rows",
            "normalization_allowed": [
                "strip absolute timestamps for event_sequence",
                "retain order",
                "no classifier-output as independent HW diagnostic",
            ],
        },
        "profiles": {},
    }
    for prof in ("a", "b", "c", "d"):
        key = f"observer_trace_{prof}"
        left, right = t0.get(key, []), t1.get(key, [])
        report["profiles"][prof] = {
            "event_sequence": diff_event_sequences(left, right),
            "timed": diff_timed_traces(left, right),
            "classification_equal": t0.get(f"fate_{prof}") == t1.get(f"fate_{prof}")
            if prof in "abcd"
            else None,
        }
    # Fate fields use fate_a..d keys.
    report["profiles"]["a"]["classification_equal"] = t0.get("fate_a") == t1.get("fate_a")
    report["profiles"]["b"]["classification_equal"] = t0.get("fate_b") == t1.get("fate_b")
    report["profiles"]["c"]["classification_equal"] = t0.get("fate_c") == t1.get("fate_c")
    report["profiles"]["d"]["classification_equal"] = t0.get("fate_d") == t1.get("fate_d")
    return report


def summarize(rows: list[dict], campaign: str) -> dict:
    summary: Dict[str, Any] = {
        "campaign": campaign,
        "profile": "AXIS-P1-Continuous",
        "experiments": [],
        "all_pass": True,
        "assumptions": [
            "Bridge-AXIS-P1: one UCIe segment per AXIS transfer (MODEL PROFILE)",
            "AoU normative AXIS↔Flit mapping ungeklärt (AoU PDF absent)",
            "Instant cancel (cancel_latency_ns==0) is MODEL ASSUMPTION",
            "No AXI-Stream BRESP; local abort is bridge lifecycle only",
            "SINK-PKT: packet_complete ≠ process_start ≠ atomic effect",
            "App-ACK / packet_op_id / dup-suppress are ADDITIONAL MECHANISMS",
        ],
    }
    for row in rows:
        txs = row.get("transactions", [])
        entry = {
            "experiment": row.get("experiment"),
            "pass": bool(row.get("pass")),
            "config": row.get("config"),
            "failures": row.get("failures", []),
            "counter": row.get("counter"),
            "effective_config": row.get("effective_config"),
            "transactions": [
                {
                    "id": t.get("id"),
                    "commit_count": t.get("commit_count"),
                    "fate_a": t.get("fate_a"),
                    "fate_c": t.get("fate_c"),
                    "fate_d": t.get("fate_d"),
                    "diag_never_handed": t.get("diag_never_handed"),
                    "diag_missing_ack": t.get("diag_missing_ack"),
                    "software_retries": t.get("software_retries"),
                    "axis_sink": t.get("axis_sink"),
                    "axis_local_beats": t.get("axis_local_beats"),
                    "axis_remote_beats": t.get("axis_remote_beats"),
                    "axis_remote_packet_complete": t.get("axis_remote_packet_complete"),
                    "axis_processing_started": t.get("axis_processing_started"),
                    "axis_app_effects": t.get("axis_app_effects"),
                    "axis_bytes_committed": t.get("axis_bytes_committed"),
                    "axis_local_tlast": t.get("axis_local_tlast"),
                    "axis_protocol_error": t.get("axis_protocol_error"),
                    "axis_app_ack_received": t.get("axis_app_ack_received"),
                    "axis_dup_suppress_hit": t.get("axis_dup_suppress_hit"),
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
    summary["observer_history_compare"] = observer_compare(rows)
    return summary


def write_md(summary: dict, path: Path) -> None:
    lines = [
        f"# AXIS-P1 campaign results ({summary['campaign']})",
        "",
        "Eigenständiges AXI4-Stream-Untersuchungsprofil über UCIe-Transport.",
        "Baseline `fault_handling_*` und `_baseline_archive_phase1/` unverändert.",
        "",
        f"All pass: **{summary['all_pass']}**",
        "",
        "| Experiment | Pass | Sink | Local/Remote beats | Effects | GT commit | Fate A/C/D |",
        "| --- | --- | --- | --- | --- | --- | --- |",
    ]
    for e in summary["experiments"]:
        txs = e["transactions"]
        sink = ",".join(str(t.get("axis_sink")) for t in txs) or "-"
        beats = ",".join(
            f"{t.get('axis_local_beats')}/{t.get('axis_remote_beats')}" for t in txs
        ) or "-"
        effects = ",".join(str(t.get("axis_app_effects")) for t in txs) or "-"
        commits = ",".join(str(t.get("commit_count")) for t in txs) or "-"
        fate = ",".join(
            f"{t.get('fate_a')}/{t.get('fate_c')}/{t.get('fate_d')}" for t in txs
        ) or "-"
        lines.append(
            f"| `{e['experiment']}` | {e['pass']} | {sink} | {beats} | {effects} | {commits} | {fate} |"
        )
    oh = summary.get("observer_history_compare") or {}
    lines += ["", "## Observer compare", "", f"Mode: `{oh.get('mode')}`", ""]
    for prof, diff in (oh.get("profiles") or {}).items():
        es = diff.get("event_sequence") or {}
        td = diff.get("timed") or {}
        lines.append(
            f"- Profile {prof.upper()}: class_eq={diff.get('classification_equal')} "
            f"seq_eq={es.get('equal')} timed_eq={td.get('equal')} "
            f"reordered={es.get('reordered')}"
        )
    lines += ["", "## Assumptions", ""]
    for a in summary.get("assumptions") or []:
        lines.append(f"- {a}")
    lines.append("")
    path.write_text("\n".join(lines) + "\n")


def write_coverage(summary: dict, path: Path) -> None:
    lines = [
        "# AXIS-P1 Coverage Matrix (Phase 2/3 measured)",
        "",
        "Status relative to `docs/study/phase1_axis_axi5/05_sources_coverage_matrix.md`.",
        "Does not overwrite the Phase-1 baseline MM coverage matrix.",
        "",
        "| Thema | Status | Evidence |",
        "| --- | --- | --- |",
        "| TVALID/TREADY Transfer | implementiert | ax_clean_*, ax_backpressure_tready |",
        "| TKEEP / Continuous_Packets | implementiert | ax_packet_len_variants, ax_continuous_tid_stable |",
        "| TLAST Paketgrenze | implementiert | ax_clean_packet_*, ax_tlast_local_remote_loss |",
        "| SINK-IMM | implementiert | ax_clean_packet_imm, ax_loss_mid_packet_imm |",
        "| SINK-PKT (reception≠process≠effect) | implementiert | ax_clean_packet_pkt |",
        "| App-ACK (additional) | implementiert | ax_appack_loss |",
        "| Packet-ID / Dup-suppress (additional) | implementiert | ax_dup_suppress, ax_packet_id_reuse |",
        "| Bridge one-segment-per-transfer | abstrahiert (MODEL) | all ax_*; AoU ungeklärt |",
        "| AXI-Stream BRESP | nicht vorhanden | verified: no completed_ok without App-ACK |",
        "",
        "## Experiment PASS/FAIL",
        "",
        "| ID | Pass | Failures |",
        "| --- | --- | --- |",
    ]
    for e in summary["experiments"]:
        fails = "; ".join(e.get("failures") or []) or "-"
        lines.append(f"| `{e['experiment']}` | {e['pass']} | {fails} |")
    lines.append("")
    path.write_text("\n".join(lines) + "\n")


def write_report(auto: dict, ref: dict, path: Path) -> None:
    oh = auto.get("observer_history_compare") or {}
    lines = [
        "# AXIS-P1 Fault-Injection Report",
        "",
        "**Profil:** AXIS-P1-Continuous (IHI 0051B / Continuous_Packets).",
        "**Transport:** bestehender UCIe-Pfad; Bridge-Policy `one_ucie_segment_per_transfer`.",
        "**Nicht überschrieben:** `fault_handling_*`, `_baseline_archive_phase1/`.",
        "",
        "## Was zeigt das Experiment?",
        "",
        "1. AXI4-Stream-Transfers zählen nur bei TVALID∧TREADY; lokale Handshakes "
        "sind keine entfernte Anwendungswirkung.",
        "2. SINK-IMM erzeugt Teilwirkungen pro remote akzeptiertem Beat; SINK-PKT "
        "puffert bis TLAST und trennt ausdrücklich: Paketempfang → Process-Start → "
        "atomare Wirkung.",
        "3. Ohne App-ACK liefert das Basisinterface kein COMPLETED für die Wirkung "
        "(kein Stream-BRESP). Fehlendes UCIe-ACK allein ist nicht NOT_COMPLETED.",
        "4. Blind-Retry nach Teilaccept (IMM) bzw. App-Wiederholung kann "
        "Doppelwirkungen erzeugen; optionale Dup-Unterdrückung ist ein Zusatzmechanismus.",
        "5. Observer-Vergleiche müssen event-sequence und timed getrennt ausweisen.",
        "",
        f"Automotive all_pass={auto.get('all_pass')} (n={len(auto.get('experiments') or [])}); "
        f"Reference all_pass={ref.get('all_pass')} (n={len(ref.get('experiments') or [])}).",
        "",
        "## Welche Annahmen benötigt es?",
        "",
    ]
    for a in auto.get("assumptions") or []:
        lines.append(f"- {a}")
    lines += [
        "",
        "## Welche stärkere Aussage ist nicht nachgewiesen?",
        "",
        "- AoU-normative Abbildung AXIS↔UCIe-Flit (Quellenlücke).",
        "- Wire-Cancel-Protokoll (Instant-Cancel bleibt MODEL ASSUMPTION).",
        "- Dass Stream „weniger Ambiguität“ als MM hat — Gegenbeispiele "
        "(TLAST lokal ≠ Remote-PKT-Commit; IMM-Teilwirkungen) widerlegen das.",
        "- AXI5-Stream TWAKEUP/Parity (außerhalb Profil).",
        "- Vendor-CSR / Exactly-once als Spec-Fakt.",
        "",
        "## Unterschiede gegenüber AXI4 Memory-Mapped",
        "",
        "| Dimension | AXI4-MM (Baseline) | AXIS-P1 |",
        "| --- | --- | --- |",
        "| Protokollabschluss | B/R Handshake (OKAY/SLVERR) | lokales TLAST; kein BRESP |",
        "| Wirkung | Target-Commit nach Request+Data | Senkenmodell IMM/PKT |",
        "| Teilwirkung | typisch eine Commit-Aktion/Txn | IMM: ja; PKT: nein bis Process+Effect |",
        "| Lokaler Fehlerabschluss | SLVERR auf Response-Kanal | Bridge STREAM_LOCAL_ABORT |",
        "| Classifier COMPLETED | OKAY gesehen | nur mit optionalem App-ACK |",
        "| Transport | gleicher UCIe CRC/Replay/LinkError-Pfad | gleicher Pfad, StreamSegment-Flits |",
        "",
        "## Observer (ax_observer_timed_vs_seq)",
        "",
        f"Mode: `{oh.get('mode')}`",
        "",
    ]
    for prof, diff in (oh.get("profiles") or {}).items():
        es = diff.get("event_sequence") or {}
        td = diff.get("timed") or {}
        lines.append(
            f"- {prof.upper()}: classification_equal={diff.get('classification_equal')}, "
            f"event_sequence_equal={es.get('equal')}, timed_equal={td.get('equal')}, "
            f"reordered={es.get('reordered')}"
        )
    lines += [
        "",
        "## Evidenzstufen",
        "",
        "- **directly measured:** Kampagnen-JSON unter `axis_p1_runs/`",
        "- **under explicit model assumptions:** Bridge-Policy, Instant-Cancel, App-ACK",
        "- **not yet established:** AoU-Volltext-Konformität",
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
    summary = summarize(rows, "axis_p1_automotive")
    (ROOT / "simulation/results/axis_p1_summary.json").write_text(
        json.dumps(summary, indent=2) + "\n"
    )
    write_md(summary, ROOT / "simulation/results/axis_p1_summary.md")
    write_coverage(summary, ROOT / "simulation/results/axis_p1_coverage_matrix.md")
    (
        ROOT / "simulation/results/axis_p1_observer_diff.json"
    ).write_text(json.dumps(summary.get("observer_history_compare"), indent=2) + "\n")

    ref_rows = [run_one(name, CFG_REF, TIMED_REF, OUT_REF) for name in REFERENCE_EXPERIMENTS]
    ref_summary = summarize(ref_rows, "axis_p1_reference_ucie2x")
    (ROOT / "simulation/results/axis_p1_reference_summary.json").write_text(
        json.dumps(ref_summary, indent=2) + "\n"
    )
    write_md(ref_summary, ROOT / "simulation/results/axis_p1_reference_summary.md")
    write_report(
        summary, ref_summary, ROOT / "simulation/results/axis_p1_report.md"
    )

    print(
        json.dumps(
            {
                "automotive_all_pass": summary["all_pass"],
                "reference_all_pass": ref_summary["all_pass"],
                "n_auto": len(rows),
                "n_ref": len(ref_rows),
                "failed_auto": [e["experiment"] for e in summary["experiments"] if not e["pass"]],
                "failed_ref": [e["experiment"] for e in ref_summary["experiments"] if not e["pass"]],
            },
            indent=2,
        )
    )
    return 0 if summary["all_pass"] and ref_summary["all_pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
