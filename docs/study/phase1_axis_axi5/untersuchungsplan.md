# Untersuchungsplan — Phase 1: AXI4-Stream & begrenztes AXI5

**Status:** Phase-1-Lieferung (Planung / Abgrenzung / Baseline).  
**Datum:** 2026-11-02.  
**Kein Commit/Push. Keine Website-Änderung. Keine neuen Protokollmechanismen implementiert.**

## Zweck

Systematische Vorbereitung der Fault-Injection-Studie auf:

1. **AXI4-Stream** als eigenes Interface (nicht als Synonym für UCIe-Streaming),
2. **ausgewählte optionale AXI5-Mechanismen** auf dem Memory-Mapped-Pfad,
3. klar getrennte Nachweise gegenüber dem bestehenden AXI4-MM-über-UCIe-S-Baseline.

Diese Phase legt Quellen, Profile, Forschungsfragen und Experimentmatrizen fest.  
Implementierung folgt in Phase 2; Auswertung und Gegenbeispiele in Phase 3.

## Dokumentenindex

| Datei | Inhalt |
| --- | --- |
| [`01_baseline_verification.md`](01_baseline_verification.md) | Reproduzierbarer Baseline-Bericht inkl. ausgeführter Befehle und Bedingungsprüfung |
| [`02_protocol_boundaries.md`](02_protocol_boundaries.md) | Quellenbasierte Abgrenzung a–d |
| [`03_study_profiles.md`](03_study_profiles.md) | Modellprofile AXIS-P1 und AXI5-MM-P1 |
| [`04_research_questions.md`](04_research_questions.md) | Forschungsfragen, Evidenz, Gegenbeispiele |
| [`05_sources_coverage_matrix.md`](05_sources_coverage_matrix.md) | Quellen- und Coverage-Matrix mit Status |
| [`06_experiment_matrix.md`](06_experiment_matrix.md) | Geplante Experimente und Invarianten |
| [`07_phase2_phase3_handoff.md`](07_phase2_phase3_handoff.md) | Übergabe für Implementierung und Auswertung |

## Ausgangszustand (Kurzfassung)

- Timed SystemC-Modell `ucie_sim` + Python-Kampagne `fault_handling_campaign`.
- Fault-Handling-Draft V0.1: lokale SLVERR (ERR-05), Watchdog (WDT-03/04), Master-Retry (ERR-07).
- **Erneut ausgeführt:** 19 automotive + 12 Referenzexperimente — alle PASS; kritische wissenschaftliche Bedingungen bestätigt (siehe Baseline-Bericht).
- Historische Ergebnisse und uncommittete Working-Tree-Änderungen bleiben erhalten; Snapshot unter `simulation/results/_baseline_archive_phase1/`.

## Verbindliche Unterscheidungen

| Begriff | Bedeutung in dieser Studie | Nicht synonym mit |
| --- | --- | --- |
| **(a) AXI4 Memory-Mapped** | AW/AR/W/R/B, Antworten OKAY/SLVERR/… | Stream-Transfers |
| **(b) AXI4-Stream** | Eigenes Interface TVALID/TREADY/… (IHI0051) | UCIe-Flit-Streaming |
| **(c) Streaming-Transport über UCIe** | UCIe/AoU-Flit-Pfad mit `protocol: streaming` (Referenz-Slide) | AXI4-Stream-Handshake |
| **(d) AXI5 (Teilumfang)** | Optionale MM-Features laut IHI0022 Issue L | „alles AXI5“ / Stream Issue B |

## Gewählte Modellprofile (benannt)

| Profil-ID | Kurzbeschreibung |
| --- | --- |
| **AXIS-P1-Continuous** | AXI4-Stream-Klasse nach IHI0051B; `Continuous_Packets=True`; zwei Senkenmodelle |
| **AXI5-MM-P1** | Nur AtomicStore(ADD), Poison-Beobachtung ohne Undo, Trace **nicht** in Fate-Klassifikation |

Offene Detailfragen werden innerhalb dieser Profile entschieden und als **MODEL PROFILE** gekennzeichnet, nicht als Spec-Fakt.

## Phasenplan

| Phase | Inhalt | Gate |
| --- | --- | --- |
| **1 (diese Lieferung)** | Baseline verifizieren; Quellen; Profile; Fragen; Matrizen; Handoff | Dokumente reviewfähig; Baseline reproduzierbar |
| **2** | Implementierung AXIS-P1 + AXI5-MM-P1 im timed model; Experiment-Runner; keine Website | Unit-/Kampagnentests grün; Coverage-Status aktualisiert |
| **3** | Kampagnen, Gegenbeispiele, Bericht; ggf. Draft-Abgleich | Invarianten gemessen; Claims auf Evidenzstufen |

## Nicht-Ziele dieser Phase

- Keine neuen Protokoll-FSMs im Code.
- Keine Behauptung vollständiger AXI5- oder AXI5-Stream-Unterstützung.
- Keine Vendor-Register aus undokumentierten Databooks erfinden.
- Keine Gleichsetzung von fehlendem ACK mit Nichtausführung.
- Kein Instant-Shared-State-Cancel als Wire-Protokoll verkaufen.
