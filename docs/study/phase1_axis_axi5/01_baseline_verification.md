# Baseline-Verifikation — Fault-Handling V0.1

**Re-run UTC:** 2026-11-02 (siehe `simulation/results/_baseline_archive_phase1/archived_at_utc.txt`).  
**Ergebnis:** automotive **19/19 PASS**, reference **12/12 PASS**.  
**Archiv vs. Re-Run:** Fate/Commit-Tupel identisch.

## 1. Bewahrte Artefakte

Vor dem Re-Run kopiert nach `simulation/results/_baseline_archive_phase1/`:

- `fault_handling_report.md`
- `fault_handling_summary.{md,json}`
- `fault_handling_reference_summary.{md,json}`
- `fault_handling_observer_diff.json`

Working-Tree-Änderungen (Fault-Handling-Extension, Specs unter `docs/specs/`, Draft-PDF) wurden **nicht** verworfen. Website (`docs/*.html`) unverändert gelassen.

## 2. Tatsächlich ausgeführte Befehle

```bash
cmake -S . -B build
cmake --build build -j

ctest --test-dir build --output-on-failure \
  -R 'core|fh_|baseline_unknown|precommit|blind_retry|observational'
# → 16/16 Passed (Log: simulation/results/_baseline_archive_phase1/ctest_output.txt)

python3 -m simulation.experiments.fault_handling_campaign
# → {"automotive_all_pass": true, "reference_all_pass": true, "n_auto": 19, "n_ref": 12}
# (Log: simulation/results/_baseline_archive_phase1/campaign_rerun_output.txt)
```

Configs:

- Automotive: `configs/automotive.yaml` (u. a. `single_inflight_slot: true`, außer szenariospezifischen Overrides)
- Referenz: `configs/torsten_reference_ucie2x.yaml` (`single_inflight_slot: false`, TX=32/RX=64/retry=16)

Seed überall: **1**.

## 3. Experimentlisten

### Automotive (19)

`fh_clean_success`, `fh_loss_before_accept`, `fh_commit_then_local_slverr`, `fh_no_commit_local_slverr`, `fh_watchdog_then_late_okay`, `fh_timeout_then_remote_commit`, `fh_blind_retry_after_slverr`, `fh_no_retry_after_slverr`, `fh_axi_id_reuse_late_resp`, `fh_multi_outstanding_backpressure`, `fh_read_burst_error_completion`, `fh_race_resp_before_timeout`, `fh_race_resp_simultaneous`, `fh_race_resp_after_timeout`, `fh_classifier_diag_compare`, `fh_diag_ack_loss_after_delivery`, `fh_observer_history_compare`, `fh_safe_retry_cancel_remote`, `fh_cancel_latency_race`

### Referenz (12)

`fh_clean_success`, `fh_commit_then_local_slverr`, `fh_no_commit_local_slverr`, `fh_watchdog_then_late_okay`, `fh_timeout_then_remote_commit`, `fh_blind_retry_after_slverr`, `fh_read_burst_error_completion`, `fh_multi_outstanding_backpressure`, `fh_classifier_diag_compare`, `fh_diag_ack_loss_after_delivery`, `fh_safe_retry_cancel_remote`, `fh_cancel_latency_race`

## 4. Bedingungsprüfung (nicht nur PASS)

Geprüft an `simulation/results/fault_handling_runs/<exp>/result.json` und `fault_handling_observer_diff.json`:

| ID | Bedingung | Gemessen |
| --- | --- | --- |
| 13b | `diag_missing_ack=true`, `diag_never_handed=false`, commit=1, Fate C/D = UNKNOWN | PASS |
| 13 | Tx0: commit=1, `!diag_never_handed`, C=UNKNOWN; Tx1: commit=0, `diag_never_handed`, C=NOT_COMPLETED, `fate_c_without_diag=UNKNOWN` | PASS |
| 3 | commit=1, SLVERR/BRIDGE, A=UNKNOWN | PASS |
| 4 | commit=0, A=UNKNOWN, D=NOT_COMPLETED | PASS |
| 6 | WATCHDOG, commit=1, `allow_remote_continue=true`, A=UNKNOWN | PASS |
| 7 | BlindRetry → commit=2, software_retries≥1 | PASS |
| 15 | Instant cancel: `cancel_requested`, `!allow_remote_continue`, software_retries≥1, commit=1, COMPLETED | PASS |
| 16 | Delayed cancel: `cancel_requested`, `allow_remote_continue`, commit=1, A=UNKNOWN | PASS |
| 14 | Event-sequence A/B equal; timed A/B unequal | PASS |
| GT | `commit_count` / `architectural` nicht in Observer-Traces A/B/C (Stichprobe) | PASS |

## 5. Spezielle Prüfpunkte — Code + Messung

### 5.1 Fehlendes ACK ≠ Nichtausführung

- Regel in `src/common/classify.cpp`: `diag_missing_ack` wird explizit **nicht** für NOT_COMPLETED verwendet.
- Gegenbeispiel `fh_diag_ack_loss_after_delivery`: Delivery + persistente ACK-Unterdrückung + commit=1 → C/D bleiben UNKNOWN.

### 5.2 `diag_never_handed` aus initiatorseitig verfügbarem Zustand

- Ableitung in `World::classify_now`: `diag_never_handed = !handed_to_ucie`.
- `handed_to_ucie` wird am UCIe-Adapter gesetzt (`ucie_adapter.cpp`), nicht aus Ground Truth.
- Classifier liest nur `InitiatorView` (`view_for` kopiert **kein** `GroundTruth`).

### 5.3 Observer: timed vs. event-sequence

- Kampagne schreibt beide Modi getrennt (`fault_handling_observer_diff.json`).
- Exp 14: A/B event-sequence equal; timed unequal (verschiedene Zeitstempel).
- Zusammenfassungen dürfen „A/B gleich“ nur als Event-Sequenz nach Timestamp-Entfernung lesen.

### 5.4 Instant Shared-State-Cancel = Modellannahme

- `cancel_remote_on_local_close` + `cancel_latency_ns==0` setzt lokales Flag (`world.cpp`); Kommentar: kein Wire-/Confirm-Protokoll.
- Exp 15 zeigt Sicherheit **unter dieser Annahme**; Exp 16 widerlegt „Cancel-Request allein genügt“.

### 5.5 Ground Truth nicht in Observer/Retry-Entscheidung

- `classify(InitiatorView)` liest keinen Commit-Count.
- `arm_err07_after_local` entscheidet über Fate-Labels + `remote_may_continue` / Dup-Suppress — nicht über Ground Truth als Classifier-Input.
- Ground Truth wird nur für **Invarianten-/Auswertung** (`check_transaction_invariants`) und Report-Felder genutzt.
- Unit-Test: `architectural_commit_count=99` ändert Fate bei OKAY nicht (`tests/test_core.cpp`).

## 6. Dokumentierte Modellgrenzen (unverändert gültig)

| Grenze | Typ |
| --- | --- |
| Verkürzte `txn_watchdog_ns` in Kampagne | SIMULATION CONFIGURATION |
| Bridge-Token/Generation statt reiner AXI-ID für WDT-04 | MODEL ASSUMPTION |
| Instant cancel | MODEL ASSUMPTION |
| Disposition/ACK-History für Observer D | ASSUMPTION |
| AoU 0.8-Text nicht im Workspace; Packing 1 Msg/Flit | Abstraction |
| `protocol: streaming` in Referenzconfig | REFERENCE-SLIDE Metadata, kein AXI4-Stream |
| Poison-Undo / ECC / Vendor-CSR | nicht modelliert |
| PHY nicht cycle-accurate | Abstraction |

## 7. Abweichungen zum Berichtstext

Keine materiellen Abweichungen zwischen gespeichertem `fault_handling_report.md` und dem Re-Run gefunden. PASS-Zählung und kritische Gegenbeispiele bleiben gültig. Der Scope-Hinweis im Report („PASS ≠ jede Lücke geschlossen“) bleibt maßgeblich.
