# Experimentmatrix — Phase 2/3 (geplant)

Bestehende 19+12 MM-Experimente bleiben Regression.  
Neue IDs: Präfix `ax` = AXIS-P1, `a5` = AXI5-MM-P1.

**Invarianten-Legende:**  
GT = Ground Truth nur Auswertung; Obs = Observer; Cls = Classifier; Ret = Retry-Politik.

---

## 0. Regression (unverändert)

Alle `fh_*` auf `configs/automotive.yaml` und Kerngruppe auf `torsten_reference_ucie2x.yaml`.  
Erwartet: weiter 19/19 und 12/12 PASS inkl. Bedingungen aus `01_baseline_verification.md`.

---

## 1. AXIS-P1 — Kernmatrix

Config-Basis: automotive + `axis_profile: AXIS-P1-Continuous` (Phase 2).  
Seed=1. Shortened WDT analog bestehender Kampagne (SIMULATION CONFIG).

| ID | Experiment | Senke | Fault / Setup | Erwartete Invarianten |
| --- | --- | --- | --- | --- |
| ax_clean_packet | Sauberes 4-Beat-Paket | IMM & PKT (2 Läufe) | none | GT commit = (IMM:4 / PKT:1); Obs A sieht 4 Transfers + TLAST; kein FDI |
| ax_loss_mid_packet | CRC/Drop nach Beat 2/4 | IMM | corrupt/drop after transfer 2 | IMM: GT∈{2}; PKT-Lauf separat: GT=0; Cls A≠COMPLETED_EFFECT; missing UCIe ACK allein ⇒ nicht NOT_COMPLETED |
| ax_loss_before_first | Loss vor erstem UCIe-Handoff | beide | LinkError pre-hand | `diag_never_handed` ⇒ Cls C NOT_COMPLETED (wenn diag on); GT=0 |
| ax_tlast_local_remote_loss | TLAST lokal, Payload vor Remote verloren | PKT | drop last flit after local TLAST hs | Lokales Paketende ≠ GT commit; Fate UNKNOWN/INDETERMINATE |
| ax_imm_partial_then_blind_retry | Blind retry nach Teilaccept | IMM | mid-loss + BlindRetry | GT > erste Wirkung (Doppelbytes); flag AT_MOST_ONCE / FALSE_SAFE_RETRY |
| ax_pkt_commit_then_local_timeout | Remote PKT-Commit, lokaler Timeout | PKT | delay response path + WDT/bridge close | GT=1; Initiator ohne App-ACK unbestimmt; analog Hypothese Exp3 |
| ax_ack_loss_after_delivery | Segment delivered, ACK lost | IMM | persistent AckNakLoss | `diag_missing_ack`; Cls C/D **nicht** NOT_COMPLETED wegen ACK; GT>0 |
| ax_observer_timed_vs_seq | Zwei Läufe commit vs no-commit | IMM | analog fh_observer | Event-seq A/B equal möglich; timed getrennt unequal ausweisen |
| ax_safe_retry_cancel0 | RetryOnlyIfNotCompleted + cancel_lat=0 | PKT | never-handed + instant cancel | unter MODEL ASSUMPTION: genau ein späterer Commit |
| ax_cancel_latency_race | cancel_lat groß | PKT | timeout then remote TLAST commit | Cancel-Request verhindert Commit nicht |
| ax_backpressure_tready | Receiver TREADY delay | IMM | ready delay >0 | Transfer erst nach READY; Zähler stimmen; kein Spec-Verstoß TVALID hold |
| ax_continuous_tid_stable | TID/TDEST change mid-packet | — | Negativtest Stimulus | Modell flaggt ProtocolError / verwirft als unsupported (Continuous_Packets) |
| ax_appack_loss | Separater App-ACK verloren | PKT+AppACK | appack drop after sink commit | Basisinterface „fertig“, App-Wissen unbestimmt; Retry-Policy muss App-Kanal lesen — getrennt dokumentieren |

### AXIS-Invarianten (kampagnenweit)

1. GT nie Input von Obs A–C / Cls / Ret.
2. Observer-Diff immer dual: `event_sequence` und `timed`.
3. Kein Experiment behauptet AXI-Stream-BRESP.
4. Claims zu Cancel immer als MODEL ASSUMPTION labeln, wenn `cancel_latency_ns==0`.

---

## 2. AXI5-MM-P1 — Kernmatrix

| ID | Experiment | Setup | Erwartete Invarianten |
| --- | --- | --- | --- |
| a5_atomic_store_add_ok | AtomicStore ADD, clean | AWATOP=ADD Store | GT counter += operand einmal; BRESP OKAY; kein R-Kanalereignis |
| a5_atomic_commit_then_local_slverr | Commit then LinkError SLVERR | analog fh_commit_then_local_slverr | GT=1; A=UNKNOWN; SLVERR≠NOT_COMPLETED |
| a5_atomic_blind_retry | BlindRetry after ambiguous SLVERR | post-commit loss + BlindRetry | GT doppeltes ADD (z. B. +x+x); AT_MOST_ONCE |
| a5_atomic_never_handed | LinkError before UCIe | diag on | GT=0; C NOT_COMPLETED via never_handed |
| a5_atomic_ack_loss | ACK loss after delivery | analog 13b | missing_ack; C/D UNKNOWN; GT=1 |
| a5_poison_reject_no_commit | WPOISON=1, SINK-REJECT | poison on write data | GT=0; Response-Variante A: SLVERR / B: OKAY — beide dokumentieren; Poison≠Commit-Orakel |
| a5_poison_with_slverr_independent | poison∧OKAY und !poison∧SLVERR Paar | zwei Tx | Spec A16.1 Unabhängigkeit gemessen |
| a5_poison_after_commit_no_undo | Normal commit, later poison signal attempt | post-commit poison inject | GT bleibt 1; kein Undo |
| a5_trace_not_in_classifier | AWTRACE=1, ambiguous fate | Trace asserted | Fate identisch mit Trace=0; Trace nur in Obs D/debug |
| a5_safe_retry_cancel0 | NOT_COMPLETED + cancel0 | never handed | ein Commit unter Instant-Cancel-Annahme |
| a5_exclusive_smoke_out_of_scope | Exclusive read/write | — | **nicht** in PASS-Matrix; Runner lehnt oder markiert skipped |

### AXI5-Invarianten (kampagnenweit)

1. Kein Claim „full AXI5“.
2. AtomicStore-Pfad erzeugt keine Fake-Read-Completions.
3. Poison-Undo-Invariante: `commit_count` sinkt nie.
4. Trace-Bits erscheinen nicht in `InitiatorView`-Feldern, die `classify()` liest.

---

## 3. Kreuzvergleich / Transport

| ID | Zweck | Erwartung |
| --- | --- | --- |
| x_mm_vs_axis_same_ucie_fault | Gleiches UCIe-Fault-Timing, MM Write vs AXIS Paket | Qualitativ gleiche Ambiguitätsklasse, andere Abschlusszeugnisse (BRESP vs TLAST) |
| x_ref_arch_axis_subset | 3–5 AXIS-Fälle auf Torsten-Referenzconfig | multi-outstanding ändert Timing; Fate-Klassen bleiben erreichbar |

---

## 4. PASS-Kriterien (Phase 3)

Ein neues Experiment „PASS“ nur wenn:

1. `result.json` existieren und Szenario-`require()`-Checks halten,
2. gelistete wissenschaftliche Bedingungen (nicht nur `pass:true`) im Report zitiert sind,
3. Evidenzstufe am Claim steht,
4. Regression `fh_*` unverändert grün.
