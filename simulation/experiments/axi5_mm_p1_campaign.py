#!/usr/bin/env python3
"""Run the AXI5-MM-P1 experiment matrix (AtomicStore ADD / Poison / Trace).

Orthogonal to AXIS-P1. Does not overwrite fault_handling_* or axis_p1_* trees.
"""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, List

ROOT = Path(__file__).resolve().parents[2]
BIN = ROOT / "build" / "ucie_sim"
CFG_AUTO = ROOT / "configs" / "automotive.yaml"
CFG_REF = ROOT / "configs" / "torsten_reference_ucie2x.yaml"
OUT = ROOT / "simulation" / "results" / "axi5_mm_p1_runs"
OUT_REF = ROOT / "simulation" / "results" / "axi5_mm_p1_reference_runs"
TIMED = ROOT / "timed_results" / "axi5_mm_p1"
TIMED_REF = ROOT / "timed_results" / "axi5_mm_p1_reference"
CFG_DUMP = ROOT / "simulation" / "results" / "axi5_mm_p1_effective_configs"

EXPERIMENTS = [
    "a5_atomic_store_add_ok",
    "a5_atomic_commit_then_local_slverr",
    "a5_atomic_blind_retry",
    "a5_atomic_never_handed",
    "a5_atomic_ack_loss",
    "a5_atomic_loss_before_exec",
    "a5_atomic_timeout_after_commit",
    "a5_atomic_late_response",
    "a5_atomic_concurrent",
    "a5_atomic_dup_suppress",
    "a5_atomic_reset_restart",
    "a5_poison_reject_no_commit",
    "a5_poison_reject_okay",
    "a5_poison_with_slverr_independent",
    "a5_poison_after_commit_no_undo",
    "a5_poison_accept_marked",
    "a5_poison_partial_then_timeout",
    "a5_trace_not_in_classifier",
    "a5_safe_retry_cancel0",
    "a5_exclusive_smoke_out_of_scope",
]

REFERENCE_EXPERIMENTS = [
    "a5_atomic_store_add_ok",
    "a5_atomic_blind_retry",
    "a5_atomic_never_handed",
    "a5_poison_reject_no_commit",
    "a5_poison_with_slverr_independent",
    "a5_trace_not_in_classifier",
    "a5_safe_retry_cancel0",
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
    data["effective_config"] = {
        "config_file": data["config"],
        "experiment": name,
        "seed": 1,
        "profile": "AXI5-MM-P1",
        "spec": "ARM IHI 0022 Issue L",
        "atomic": "AtomicStore(ADD) only; AWATOP=0x10; no R-channel prior-value return",
        "poison_default_policy": "reject_without_commit_slverr",
        "trace": "signals present; never used in classify()/InitiatorView",
        "bridge_mapping": "MODEL PROFILE only; no AoU conformity claim",
        "sink_pkt_commit": "N/A (MM path); atomic SINK-PKT remains AXIS assumption",
        "completed_means": "profile-dependent: OKAY BRESP = transfer classification COMPLETED, not remote-effect proof when Poison reject + OKAY",
        "atomic_value": data.get("atomic_value"),
        "atomic_ops_applied": data.get("atomic_ops_applied"),
        "axi5_commit_watermark": data.get("axi5_commit_watermark"),
        "ucie_revision": data.get("ucie_revision"),
        "transactions_axi5": [
            {
                "id": t.get("id"),
                "form": t.get("axi5_form"),
                "awatop": t.get("axi5_awatop"),
                "operand": t.get("axi5_operand"),
                "value_before": t.get("axi5_value_before"),
                "value_after": t.get("axi5_value_after"),
                "atomic_effects": t.get("axi5_atomic_effects"),
                "transfer_attempt": t.get("axi5_transfer_attempt"),
                "wpoison": t.get("axi5_wpoison"),
                "poison_rejected": t.get("axi5_poison_rejected"),
                "awtrace": t.get("axi5_awtrace"),
                "btrace": t.get("axi5_btrace"),
                "trace_in_classify": t.get("axi5_trace_used_in_classify"),
                "read_resp_created": t.get("axi5_read_response_created"),
                "commit_count": t.get("commit_count"),
                "fate_a": t.get("fate_a"),
                "fate_c": t.get("fate_c"),
                "fate_d": t.get("fate_d"),
                "axi_resp": t.get("axi_resp"),
                "resp_source": t.get("resp_source"),
            }
            for t in data.get("transactions", [])
            if t.get("axi5_enabled")
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


def summarize(rows: list[dict], campaign: str) -> dict:
    summary: Dict[str, Any] = {
        "campaign": campaign,
        "profile": "AXI5-MM-P1",
        "spec": "ARM IHI 0022 Issue L",
        "experiments": [],
        "all_pass": True,
        "assumptions": [
            "Bridge packing of AWATOP/WPOISON/Trace onto UCIe flits is MODEL PROFILE (no AoU claim)",
            "AtomicStore(ADD) only — AtomicLoad/Swap/Compare/Exclusive out of scope",
            "AtomicStore returns write response only; no fabricated R prior-value",
            "Poison target policies are named MODEL/TARGET choices; Spec A16.1 leaves commit reaction open",
            "Poison must not undo a prior commit (study invariant)",
            "Trace is observability-only; classify()/InitiatorView never read Trace",
            "Instant cancel (cancel_latency_ns==0) remains MODEL ASSUMPTION",
            "COMPLETED under OKAY is transfer-class fate; remote effect checked via atomic_value separately",
            "SINK-PKT atomic commit assumption belongs to AXIS, not this MM profile",
        ],
    }
    for row in rows:
        txs = row.get("transactions", [])
        entry = {
            "experiment": row.get("experiment"),
            "pass": bool(row.get("pass")),
            "config": row.get("config"),
            "failures": row.get("failures", []),
            "violations": row.get("violations", []),
            "atomic_value": row.get("atomic_value"),
            "atomic_ops_applied": row.get("atomic_ops_applied"),
            "axi5_commit_watermark": row.get("axi5_commit_watermark"),
            "effective_config": row.get("effective_config"),
            "transactions": [
                {
                    "id": t.get("id"),
                    "commit_count": t.get("commit_count"),
                    "axi5_form": t.get("axi5_form"),
                    "axi5_operand": t.get("axi5_operand"),
                    "axi5_value_before": t.get("axi5_value_before"),
                    "axi5_value_after": t.get("axi5_value_after"),
                    "axi5_atomic_effects": t.get("axi5_atomic_effects"),
                    "axi5_transfer_attempt": t.get("axi5_transfer_attempt"),
                    "axi5_wpoison": t.get("axi5_wpoison"),
                    "axi5_poison_rejected": t.get("axi5_poison_rejected"),
                    "axi5_awtrace": t.get("axi5_awtrace"),
                    "axi5_btrace": t.get("axi5_btrace"),
                    "axi5_read_response_created": t.get("axi5_read_response_created"),
                    "fate_a": t.get("fate_a"),
                    "fate_c": t.get("fate_c"),
                    "fate_d": t.get("fate_d"),
                    "axi_resp": t.get("axi_resp"),
                    "resp_source": t.get("resp_source"),
                    "software_retries": t.get("software_retries"),
                    "diag_never_handed": t.get("diag_never_handed"),
                    "diag_missing_ack": t.get("diag_missing_ack"),
                }
                for t in txs
            ],
        }
        if not entry["pass"]:
            summary["all_pass"] = False
        summary["experiments"].append(entry)
    return summary


def write_md(summary: dict, path: Path) -> None:
    lines = [
        f"# AXI5-MM-P1 campaign results ({summary['campaign']})",
        "",
        "Teilumfang AtomicStore(ADD) / Poison / Trace über UCIe-Transport.",
        "AXIS-P1 und MM-Baseline (`fault_handling_*`) nicht vermischt.",
        "",
        f"All pass: **{summary['all_pass']}**",
        "",
        "| Experiment | Pass | atomic_value | ops | commits | Fate A/C/D | BRESP |",
        "| --- | --- | --- | --- | --- | --- | --- |",
    ]
    for e in summary["experiments"]:
        txs = e["transactions"]
        commits = ",".join(str(t.get("commit_count")) for t in txs) or "-"
        fate = ",".join(
            f"{t.get('fate_a')}/{t.get('fate_c')}/{t.get('fate_d')}" for t in txs
        ) or "-"
        resp = ",".join(str(t.get("axi_resp")) for t in txs) or "-"
        lines.append(
            f"| `{e['experiment']}` | {e['pass']} | {e.get('atomic_value')} | "
            f"{e.get('atomic_ops_applied')} | {commits} | {fate} | {resp} |"
        )
    lines += ["", "## Assumptions", ""]
    for a in summary.get("assumptions") or []:
        lines.append(f"- {a}")
    lines.append("")
    path.write_text("\n".join(lines) + "\n")


def write_coverage(summary: dict, path: Path) -> None:
    lines = [
        "# AXI5-MM-P1 Coverage Matrix (measured)",
        "",
        "Status relative to `docs/study/phase1_axis_axi5/05_sources_coverage_matrix.md` §D.",
        "",
        "| Thema | Status | Evidence |",
        "| --- | --- | --- |",
        "| AtomicStore overview A6.4.1 | implementiert | a5_atomic_store_add_ok |",
        "| Operator ADD A6.4.2 | implementiert | a5_atomic_* ; memory value checked |",
        "| AWATOP signaling A6.4.6 | implementiert | AWATOP=0x10 Store ADD LE |",
        "| No R prior-value (Store) | implementiert | axi5_read_response_created=false |",
        "| Blind retry double ADD | gemessen | a5_atomic_blind_retry atomic_value=20 |",
        "| Poison WPOISON A16.1 | implementiert | a5_poison_* named target policies |",
        "| Poison ⊥ Error | gemessen | a5_poison_with_slverr_independent |",
        "| No Poison undo | gemessen | a5_poison_after_commit_no_undo |",
        "| Trace A12.3 observability | implementiert | a5_trace_not_in_classifier |",
        "| AtomicLoad/Swap/Compare/Exclusive | nicht untersucht | a5_exclusive_smoke_out_of_scope |",
        "| Full AXI5 | nicht behauptet | profile subset only |",
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
    lines = [
        "# AXI5-MM-P1 Investigation Report",
        "",
        "**Profil:** AXI5-MM-P1 (ARM IHI 0022 Issue L Teilumfang).",
        "**Umgesetzt:** AtomicStore(ADD), Poison (WPOISON) mit benannten Target-Policies, Trace (Observability).",
        "**Nicht umgesetzt:** AtomicLoad/Swap/Compare, Exclusive, AXI5-Stream, Exactly-once-Garantie, volle AXI5-Konformität.",
        "",
        "## Spezifikationsbindung",
        "",
        "| Feature | Spec | Modell | Explizit nicht |",
        "| --- | --- | --- | --- |",
        "| AtomicStore | A6.4.1: Address+Data+Op; Resultat speichern; **eine Write-Response ohne Daten** | `counter/atomic_value += operand` atomar bzgl. Modellzeit | AtomicLoad (gibt Vorwert zurück) |",
        "| ADD | A6.4.2 / Table A6.9 AWATOP[2:0]=000 | little-endian Store ADD AWATOP=`0b010000` | CLR/EOR/SET/SMAX/… |",
        "| Poison | A16.1: Markierung korrupter Daten; **unabhängig** von Error-Response | Transport + Target-Policy; kein Undo | Spec-Zwang „Poison verhindert Commit“ |",
        "| Trace | A12.3: IMPLEMENTATION DEFINED; Empfehlung AWTRACE→BTRACE | Export in DISP/Debug; **nicht** in `classify()` | Trace als Commit-Beweis |",
        "",
        "## Zentraler Atomic-Nachweis (Blind Retry)",
        "",
        "Nach Ausführung mit verlorener Antwort und Blind-Retry ist die doppelte ADD",
        "am **Speicherwert** sichtbar (`atomic_value`: 10→20 bei Operand 5), nicht nur am",
        "Commit-Zähler. Getrennt geprüft: `atomic_ops_applied`, `architectural_commit_count`,",
        "BRESP/Fate, Observer-Klassifikation.",
        "",
        f"Automotive all_pass={auto.get('all_pass')} (n={len(auto.get('experiments') or [])}); "
        f"Reference all_pass={ref.get('all_pass')} (n={len(ref.get('experiments') or [])}).",
        "",
        "## Poison vs Atomics",
        "",
        "Zuerst getrennt untersucht. Kombination nur unter benannter Policy:",
        "`RejectWithoutCommit{Slverr,Okay}`, `AcceptMarked{Okay,Slverr}`.",
        "Spec A16.1 belegt Unabhängigkeit Poison∧OKAY und !Poison∧SLVERR",
        "(Experiment `a5_poison_with_slverr_independent`).",
        "",
        "## COMPLETED / Bridge / Cancel Kennzeichnung",
        "",
        "- **Bridge-Mapping** AWATOP/WPOISON/Trace auf UCIe-Flits: MODEL PROFILE, keine AoU-Konformität.",
        "- **SINK-PKT-Commit:** AXIS-Senkenannahme; dieses MM-Profil nutzt AtomicStore-Wirkung am Target.",
        "- **COMPLETED:** bei OKAY-BRESP profilabhängige Transfer-Klassifikation; bei",
        "  `RejectWithoutCommitOkay` ausdrücklich **kein** Remote-Wirkungsbeweis",
        "  (`knowledge_a..d=INDETERMINATE`; kein falscher Wirkungsbeweis).",
        "- **Instant Shared-State-Cancel:** weiterhin MODEL ASSUMPTION (`a5_safe_retry_cancel0`).",
        "",
        "## Was nicht nachgewiesen ist",
        "",
        "- Exactly-once über Retry-Versuche hinweg (Gegenbeispiel: Blind-Retry Doppel-ADD).",
        "- Dass AtomicStore Fate bestimmbarer macht (Gegenbeispiel: Commit + lokales SLVERR).",
        "- Dass Poison ein Commit-Orakel ist (Gegenbeispiele: Poison∧OKAY / AcceptMarked).",
        "- Dass Trace Ausführung beweist (Fate identisch mit/ohne AWTRACE).",
        "- Wire-Cancel-Protokoll (nur Instant-Cancel-Idealmodell).",
        "",
        "## Annahmen",
        "",
    ]
    for a in auto.get("assumptions") or []:
        lines.append(f"- {a}")
    lines.append("")
    path.write_text("\n".join(lines) + "\n")


def main() -> int:
    if not BIN.exists():
        print("build/ucie_sim missing; build first", file=sys.stderr)
        return 2
    OUT.mkdir(parents=True, exist_ok=True)
    OUT_REF.mkdir(parents=True, exist_ok=True)
    auto_rows = [run_one(e, CFG_AUTO, TIMED, OUT) for e in EXPERIMENTS]
    ref_rows = [run_one(e, CFG_REF, TIMED_REF, OUT_REF) for e in REFERENCE_EXPERIMENTS]
    auto = summarize(auto_rows, "automotive")
    ref = summarize(ref_rows, "reference")
    res = ROOT / "simulation" / "results"
    (res / "axi5_mm_p1_summary.json").write_text(json.dumps(auto, indent=2) + "\n")
    (res / "axi5_mm_p1_reference_summary.json").write_text(json.dumps(ref, indent=2) + "\n")
    write_md(auto, res / "axi5_mm_p1_summary.md")
    write_md(ref, res / "axi5_mm_p1_reference_summary.md")
    write_coverage(auto, res / "axi5_mm_p1_coverage_matrix.md")
    write_report(auto, ref, res / "axi5_mm_p1_report.md")
    print(f"automotive all_pass={auto['all_pass']} n={len(auto['experiments'])}")
    print(f"reference all_pass={ref['all_pass']} n={len(ref['experiments'])}")
    return 0 if auto["all_pass"] and ref["all_pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
