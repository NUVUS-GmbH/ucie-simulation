# AXI5-MM-P1 Investigation Report

**Profil:** AXI5-MM-P1 (ARM IHI 0022 Issue L Teilumfang).
**Umgesetzt:** AtomicStore(ADD), Poison (WPOISON) mit benannten Target-Policies, Trace (Observability).
**Nicht umgesetzt:** AtomicLoad/Swap/Compare, Exclusive, AXI5-Stream, Exactly-once-Garantie, volle AXI5-Konformität.

## Spezifikationsbindung

| Feature | Spec | Modell | Explizit nicht |
| --- | --- | --- | --- |
| AtomicStore | A6.4.1: Address+Data+Op; Resultat speichern; **eine Write-Response ohne Daten** | `counter/atomic_value += operand` atomar bzgl. Modellzeit | AtomicLoad (gibt Vorwert zurück) |
| ADD | A6.4.2 / Table A6.9 AWATOP[2:0]=000 | little-endian Store ADD AWATOP=`0b010000` | CLR/EOR/SET/SMAX/… |
| Poison | A16.1: Markierung korrupter Daten; **unabhängig** von Error-Response | Transport + Target-Policy; kein Undo | Spec-Zwang „Poison verhindert Commit“ |
| Trace | A12.3: IMPLEMENTATION DEFINED; Empfehlung AWTRACE→BTRACE | Export in DISP/Debug; **nicht** in `classify()` | Trace als Commit-Beweis |

## Zentraler Atomic-Nachweis (Blind Retry)

Nach Ausführung mit verlorener Antwort und Blind-Retry ist die doppelte ADD
am **Speicherwert** sichtbar (`atomic_value`: 10→20 bei Operand 5), nicht nur am
Commit-Zähler. Getrennt geprüft: `atomic_ops_applied`, `architectural_commit_count`,
BRESP/Fate, Observer-Klassifikation.

Automotive all_pass=True (n=20); Reference all_pass=True (n=7).

## Poison vs Atomics

Zuerst getrennt untersucht. Kombination nur unter benannter Policy:
`RejectWithoutCommit{Slverr,Okay}`, `AcceptMarked{Okay,Slverr}`.
Spec A16.1 belegt Unabhängigkeit Poison∧OKAY und !Poison∧SLVERR
(Experiment `a5_poison_with_slverr_independent`).

## COMPLETED / Bridge / Cancel Kennzeichnung

- **Bridge-Mapping** AWATOP/WPOISON/Trace auf UCIe-Flits: MODEL PROFILE, keine AoU-Konformität.
- **SINK-PKT-Commit:** AXIS-Senkenannahme; dieses MM-Profil nutzt AtomicStore-Wirkung am Target.
- **COMPLETED:** bei OKAY-BRESP profilabhängige Transfer-Klassifikation; bei
  `RejectWithoutCommitOkay` ausdrücklich **kein** Remote-Wirkungsbeweis
  (`knowledge_a..d=INDETERMINATE`; kein falscher Wirkungsbeweis).
- **Instant Shared-State-Cancel:** weiterhin MODEL ASSUMPTION (`a5_safe_retry_cancel0`).

## Was nicht nachgewiesen ist

- Exactly-once über Retry-Versuche hinweg (Gegenbeispiel: Blind-Retry Doppel-ADD).
- Dass AtomicStore Fate bestimmbarer macht (Gegenbeispiel: Commit + lokales SLVERR).
- Dass Poison ein Commit-Orakel ist (Gegenbeispiele: Poison∧OKAY / AcceptMarked).
- Dass Trace Ausführung beweist (Fate identisch mit/ohne AWTRACE).
- Wire-Cancel-Protokoll (nur Instant-Cancel-Idealmodell).

## Annahmen

- Bridge packing of AWATOP/WPOISON/Trace onto UCIe flits is MODEL PROFILE (no AoU claim)
- AtomicStore(ADD) only — AtomicLoad/Swap/Compare/Exclusive out of scope
- AtomicStore returns write response only; no fabricated R prior-value
- Poison target policies are named MODEL/TARGET choices; Spec A16.1 leaves commit reaction open
- Poison must not undo a prior commit (study invariant)
- Trace is observability-only; classify()/InitiatorView never read Trace
- Instant cancel (cancel_latency_ns==0) remains MODEL ASSUMPTION
- COMPLETED under OKAY is transfer-class fate; remote effect checked via atomic_value separately
- SINK-PKT atomic commit assumption belongs to AXIS, not this MM profile

