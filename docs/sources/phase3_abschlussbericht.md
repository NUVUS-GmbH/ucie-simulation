# Phase-3 Abschlussbericht — AXI4-MM / AXI4-Stream / AXI5-MM-P1 über UCIe

**Datum:** 2026-11-02  
**Website:** nicht geändert. **Commit/Push:** nicht durchgeführt.  
**Codezustand:** `HEAD=cd9feee` plus Working-Tree (AXIS/AXI5/Fault-Handling); siehe `phase3_audit_code_state.txt`.

## Reproduzierbarkeit (frisch gemessen)

| Schritt | Befehl / Ort |
| --- | --- |
| Build | `cmake -S . -B build && cmake --build build -j` |
| CTest | `ctest --test-dir build --output-on-failure -R 'core\|fh_\|ax_\|a5_\|baseline_unknown\|precommit\|blind_retry\|observational'` → 30/30 PASS |
| AXI4-MM | `python3 -m simulation.experiments.fault_handling_campaign` → auto 19/19, ref 12/12 |
| AXIS-P1 | `python3 -m simulation.experiments.axis_campaign` → auto 24/24, ref 7/7 |
| AXI5-MM-P1 | `python3 -m simulation.experiments.axi5_mm_p1_campaign` → auto 20/20, ref 7/7 |
| Seed | `1` für alle Kampagnenläufe |
| Ergebnisbäume | frisch geleert vor Lauf; `run_one` löscht `timed_results/.../<exp>` vor dem Prozess (kein Erben alter `result.json`) |

Logs: `phase3_audit_fh_campaign.log`, `phase3_audit_axis_campaign.log`, `phase3_audit_axi5_campaign.log`.

**Hinweis:** Eine endliche PASS-Kampagne belegt Erreichbarkeit und gemessene Gegenbeispiele — **keine** universelle Sicherheit.

---

## 1. Begriffstrennung (alle drei Profile)

| Dimension | Bedeutung | Typische Signale |
| --- | --- | --- |
| **Protokollabschluss** | AXI-/AXIS-Handshake geschlossen | `AxiProtoState`, `axi_completed_ok/err`, lokales TLAST |
| **Tatsächliche Wirkung** | Ground-Truth-Commit am Target | `architectural_commit_count`, `atomic_value` / AXIS `app_effects` |
| **Observer-Wissen** | Was der Initiator ableiten darf | `Fate`, `EffectKnowledge` aus `classify(InitiatorView)` |

### Ableitungen COMPLETED / SURE_EFFECT / COMPLETION_INCONSISTENT

1. `axi_completed_ok` → `Fate::Completed` bezeichnet erfolgreichen Transferabschluss.
2. `classify_effect(InitiatorView)` leitet Wirkungswissen separat ab. Bei lokal
   bekannter Poison-Kennzeichnung bleibt OKAY für A–D `INDETERMINATE`, unabhängig
   von der tatsächlich ausgeführten Target-Policy. Ohne Poison gilt der explizite
   Modellvertrag: erfolgreiche MM-Antwort bzw. AXIS-App-ACK bestätigt die Wirkung.
3. `SURE_EFFECT` bedeutet, dass die beabsichtigte Wirkung stattgefunden hat, nicht
   Exactly-once. Wiederholte Wirkung verletzt separat `AT_MOST_ONCE_VIOLATION`.
4. `FALSE_SURE_EFFECT` prüft tatsächliche Wissensaussagen aller Observer A–D gegen
   Ground Truth. `COMPLETION_INCONSISTENT` meldet widersprüchliche Protokollabschlüsse
   (gleichzeitig erfolgreicher und fehlerhafter Abschluss), nicht Poison-Rejection.
5. SLVERR allein beweist weiterhin weder Wirkung noch Nichtausführung.

Nach Bereinigung verlangt `a5_poison_reject_okay`: Proto `COMPLETED_OK`, Fate
`COMPLETED`, Knowledge A–D `INDETERMINATE`, GT `commit=0`, `atomic_value=10`,
keine `COMPLETION_INCONSISTENT`- oder `FALSE_SURE_EFFECT`-Violation.
Negative Unit-Tests erzwingen weiterhin die Erkennung falscher Wissensaussagen,
auch bei Observer D. Target-Policy und Ground Truth gehen nicht in die
Wissensableitung ein.

---

## 2. RejectWithoutCommitOkay

| Frage | Antwort | Stufe |
| --- | --- | --- |
| Fachlich zulässig im Poison-Profil? | **Ja als benannte TARGET POLICY.** Spec A16.1: Poison ⊥ Error-Response; Spec erzwingt weder Commit noch Verbot von OKAY. | Spec + MODEL |
| Was garantiert OKAY hier? | Im untersuchten Poison-Profil den erfolgreichen Antwortabschluss, aber weder die konkrete Target-Policy noch einen ADD-Wirkungsbeweis. | gemessen |
| Ausbleibende Wirkung? | **Erwartetes Policy-Verhalten**, kein Simulationsfehler. Die Wirkung bleibt für den Observer unbestimmt; dies ist keine Protokoll-Inkonsistenz. | gemessen |
| Initiator-Unterscheidbarkeit RejectOkay vs AcceptMarkedOkay? | **Nein** allein über BRESP (beide OKAY). | gemessen (`a5_poison_reject_okay` vs `a5_poison_accept_marked`) |

Poison-Policies bleiben ausdrücklich **kein** standardisiertes AXI-/UCIe-Muss.

---

## 3. Rekonstruktion `a5_atomic_blind_retry`

Quelle: `axi5_mm_p1_runs/a5_atomic_blind_retry/{result.json,trace.txt}` (frischer Lauf).

| Größe | Wert |
| --- | --- |
| Anfangswert | `atomic_value = 10` |
| Operand | `+5` (AWATOP Store-ADD LE `0x10`) |
| Versuchindex erster Versuch | `transfer_attempt = 0` |
| Versuchindex nach Retry | `transfer_attempt = 1` |
| Versuchsanzahl | `1 + software_retries = 2` |
| Ausführungsanzahl | `atomic_ops_applied = 2` (= doppeltes ADD) |
| Endwert | `20` |

### Ausführungszeitpunkte (GT-Trace)

| t (ns) | Ereignis | Speicher danach |
| --- | --- | --- |
| 2027 | 1. Architectural ADD `10+5→15` | 15 |
| 2031–2058 | Response-CRC-Streak → Retrain | 15 |
| 7058 | LinkError | 15 |
| 16007058/59 | Flush; Bridge-SLVERR; BlindRetry armed | 15 (Antwortverlust; Wirkung bleibt) |
| 16009059 | Software-Retry derselben logischen Tx | 15 |
| 16009086 | 2. Architectural ADD `15+5→20` | 20 |
| 16009098 | Target-OKAY; Fate COMPLETED / SURE_EFFECT | 20 |

Violations (korrekt, nicht unterdrückt): `FALSE_SAFE_RETRY`, `AT_MOST_ONCE_VIOLATION`.

### Identischer Ablauf ohne Retry (`a5_atomic_commit_then_local_slverr`)

Gleicher Fault bis Bridge-SLVERR; `MasterRetryPolicy::Abort`.

| Größe | Mit BlindRetry | Ohne Retry |
| --- | --- | --- |
| ops / atomic_value | 2 / 20 | 1 / 15 |
| software_retries / transfer_attempt | 1 / 1 | 0 / 0 |
| final Fate A | COMPLETED | UNKNOWN |
| Violations | AT_MOST_ONCE + … | keine |

**Existenznachweis:** Blind-Retry nach Commit+Antwortverlust erzeugt sichtbare Doppelwirkung am Speicher — nicht nur am Commit-Zähler.

---

## 4. Trace / Classifier / Retry-Policy — Wissensisolation

| Kanal | In `classify()` / Retry? | Befund |
| --- | --- | --- |
| AWTRACE/BTRACE | nein (`axi5_use_trace_in_classify` muss false; `InitiatorView` ohne Trace) | gemessen (`a5_trace_not_in_classifier`) |
| GT-Wirkung (`ATOMIC_ADD`, AXIS effects) | zuvor fälschlich als `DISP:*` in Observer **D** | **behoben:** Prefix `GT:` wird nicht mehr in A–D projiziert |
| RetryOnlyIfNotCompleted | nur Fate + `remote_may_continue` / Dup-Suppress | Code audit |
| BlindRetry | nur lokales SLVERR | Code audit |

**Offene Grenze (ehrlich):** Ohne App-ACK nutzt `axis_wait_settle` weiterhin `app_effects` (GT) als Early-Exit für die Wartezeit — beeinflusst Settlement-Timing, nicht `classify()`. MODEL-Convenience, kein Wire-Signal.

---

## 5. Vergleichsmatrix

Legende Evidenz: **M** = direkt gemessen · **S** = Spec · **A** = Modellannahme · **O** = noch offen

| Profil / Variante | Protokollabschluss | Mögliche Wirkung | Sichtbare Evidenz | Sicherer Retry (Bedingungen) | Offene Grenzen |
| --- | --- | --- | --- | --- | --- |
| **AXI4-MM** | B/R Handshake OKAY/SLVERR **(M/S)** | 0/1 Commit; BlindRetry → 2 **(M)** | A: AXI; C/D: Status/Diag/Disp; SLVERR≠Commit-Bit **(M)** | Nur `NOT_COMPLETED` **und** Cancel/Dup-Suppress **(A/M)** | Wire-Cancel; Vendor-CSR; ACK≠Nichtausführung **(M)** |
| **AXIS SINK-IMM** | lokales TVALID∧TREADY; TLAST lokal; kein BRESP **(M/S)** | Teilwirkung pro remote akzeptiertem Beat **(M)** `ax_loss_mid_packet_imm` commit=1 bei Abbruch | ohne App-ACK kein COMPLETED **(M)**; missing UCIe-ACK ≠ NOT_COMPLETED **(M)** | nach Teilaccept i.d.R. **unsicher** **(M)** | AoU-Flit-Mapping **(O)**; settle peeks `app_effects` **(A)** |
| **AXIS SINK-PKT** | wie IMM lokal | Wirkung erst nach Process+Effect; Mid-Loss → commit=0 **(M)** `ax_loss_mid_packet_pkt` | TLAST lokal ≠ Remote-PKT-Commit **(M)** `ax_tlast_local_remote_loss` | sicherer nur mit Cancel/Dup-ID + kein remote TLAST/Effect **(A)** | packet_op_id Lifetime/Reset **(A/M)** |
| **+ App-ACK / Dup-Suppress** | optional zusätzlicher Abschluss **(A)** | Dup-Suppress hält Einfachwirkung **(M)** | App-ACK kann COMPLETED liefern **(M)** | verbessert Evidenz; kein Spec-Muss **(S/A)** | App-ACK-Verlust; ID-Reuse nach Expiry **(M)** |
| **AXI5-MM-P1 AtomicStore(ADD)** | nur B-Response, kein R-Vorwert **(S/M)** | ADD am `atomic_value`; BlindRetry 10→20 **(M)** | wie MM; Trace nur Observability **(M)** | wie MM; Atomic macht Fate **nicht** sicherer **(M)** `a5_atomic_commit_then_local_slverr` | AtomicLoad/Swap/Compare/Exclusive **(O)**; Bridge-AWATOP-Packung **(A)** |
| **+ Poison-Policies** | OKAY oder SLVERR je Policy **(A)** | Reject: keine Wirkung; AcceptMarked: Wirkung **(M)** | OKAY allein unterscheidet Policies nicht **(M)** | Poison ist kein Commit-Orakel **(S/M)** | welche Policy echte IP hat **(O)** |
| **+ Instant Cancel** | — | verhindert Folgewirkung alter Instanz **(A/M)** | Cancel nicht auf Wire spezifiziert | „sicher“ nur unter `cancel_latency_ns==0` Ideal **(A)** | reales Cancel-Protokoll **(O)** |

---

## 6. Existenznachweise (Gegenbeispiele)

| Claim widerlegt / begrenzt | Experiment | Beobachtung |
| --- | --- | --- |
| SLVERR ⇒ nicht ausgeführt | `fh_commit_then_local_slverr`, `a5_atomic_commit_then_local_slverr` | commit=1, Fate UNKNOWN |
| missing ACK ⇒ NOT_COMPLETED | `fh_diag_ack_loss_after_delivery`, `a5_atomic_ack_loss`, `ax_ack_loss_after_delivery` | Wirkung da; Fate nicht NOT_COMPLETED allein wegen ACK |
| BlindRetry at-most-once | `fh_blind_retry_after_slverr`, `a5_atomic_blind_retry`, `ax_imm_partial_then_blind_retry` | Doppelwirkung / AT_MOST_ONCE |
| TLAST lokal ⇒ Remote-PKT-Wirkung | `ax_tlast_local_remote_loss` | lokal TLAST-Pfad, commit=0 |
| Stream „weniger Ambiguität“ | IMM Teilwirkung vs PKT Nullwirkung bei Mid-Loss | Ambiguität verschiebt sich |
| Poison verhindert Wirkung (immer) | `a5_poison_accept_marked` | Poison∧Commit∧OKAY |
| OKAY ⇒ SureEffect (epistemisch) | `a5_poison_reject_okay` | OKAY∧commit=0, Knowledge INDETERMINATE |
| Trace beweist Ausführung | `a5_trace_not_in_classifier` | Fate mit/ohne AWTRACE gleich; Trace nicht in classify |

---

## 7. Beweiskraft der Tests (kritisch)

| Kriterium | Bewertung |
| --- | --- |
| PASS unabhängig von Impl.? | Teils: viele Requires prüfen GT+Fate+Violations; transfer-class COMPLETED ist bewusste Modellwahl und braucht Invarianten |
| Echte Handshakes/Wirkungen? | Ja für VALID/READY, BRESP, Atomic ADD, AXIS beats; UCIe-PHY weiterhin abstrahiert |
| Ereignisreihenfolgen? | Race-/Watchdog-/Cancel-Varianten vorhanden; nicht exhaustiv |
| Fehlerdiagnose bis Endzustand? | Watchdog/Late-Discard modelliert; vorzeitiges Sim-Ende durch settle-Grace möglich (AXIS) |
| Idealisierungen sichtbar? | Instant-Cancel, Bridge-Packung, verkürzte WDT-ns, Poison-Policies, SINK-Modelle — als MODEL/PROJECT gekennzeichnet |
| Observer ohne Simulatorwissen? | Nach Fix: GT-Effects nicht mehr in A–D. Verbleibend: AXIS settle Early-Exit auf `app_effects` |

---

## 8. In diesem Audit behobene Fehler

1. **Stale `result.json`:** Kampagnen löschen Output-Verzeichnis vor dem Lauf.
2. **GT-Leak in Observer D:** Architectural/AXIS-Effekt-Events → `GT:` (keine A–D-Projektion).
3. **Violations unvollständig:** `FALSE_SURE_EFFECT` / `FALSE_SURE_NO_EFFECT` / `SECOND_LOCAL_RESPONSE` werden emittiert.
4. **Poison-OKAY-Require zu schwach:** verlangt jetzt unbestimmtes Wirkungswissen ohne falsche Invariantenverletzung.

---

## 9. Quellen / Resultat-Links

- Baseline MM-Bericht: [`fault_handling_report.md`](fault_handling_report.md)
- AXIS-Bericht: [`axis_p1_report.md`](axis_p1_report.md)
- AXI5-Bericht: [`axi5_mm_p1_report.md`](axi5_mm_p1_report.md)
- Study-Plan: [`docs/study/phase1_axis_axi5/`](../../docs/study/phase1_axis_axi5/)
- Runs: `fault_handling_runs/`, `axis_p1_runs/`, `axi5_mm_p1_runs/` (+ `*_reference_runs/`)
- Specs: ARM IHI 0022 (AXI5 Issue L Teilumfang), IHI 0051B (AXI4-Stream), UCIe 2.0; Draft `UCIe-S_AXI_Fehlerspezifikation.pdf`

---

## 10. Antwortentwurf an provided

Siehe Abschnitt unten (nicht versendet).

---

## 11. Höchstens fünf Bridge-/IP-Fragen

1. Welche **Poison-Reaktion** hat euer Target/Bridge bei WPOISON: Reject+SLVERR, Reject+OKAY, Accept-marked, oder undocumented?
2. Existiert ein **Wire-Cancel / Abort** der Remote-Instanz mit bekannter Latenz, oder nur lokales SLVERR?
3. Bei Stream-Pfad: Senke **sofort pro Beat** oder **erst nach TLAST/Process** — und gibt es App-ACK?
4. Werden **AtomicStore**-Attribute (AWATOP) über eure UCIe/AoU-Packung durchgereicht, und welche Atomics sind freigeschaltet?
5. Nach LinkError/Reset: bleibt eine **Duplikat-Unterdrückung** (Token / packet_op_id) erhalten, und wie wird AXI-ID-Wiederverwendung mit Late-Response gehandhabt?
