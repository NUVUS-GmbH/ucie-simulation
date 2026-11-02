# Phase-1-Dokumente — Widerspruchsprüfung (vor Phase-2-Umsetzung)

Geprüft: `profiles.yaml`, `03_study_profiles.md`, `06_experiment_matrix.md`,
`07_phase2_phase3_handoff.md`, User-Übergabe Phase 2.

## Behobene / dokumentierte Spannungen

| Thema | Dokumente | Auflösung in der Implementierung |
| --- | --- | --- |
| SINK-PKT Commit | `profiles.yaml` / `03`: `after_tlast_accept` als ein Schritt; User-Übergabe verlangt Trennung Empfang / Process-Start / Wirkung | **Verfeinerung:** drei Stufen (1) `remote_packet_complete`, (2) `processing_started` nach `axis_sink_process_start_cycles`, (3) atomare Wirkung via `axis_commit_pkt_effect`. Vollständiger Empfang allein erzeugt keine Wirkung. |
| App-ACK | Handoff: optional nachziehen; Matrix: `ax_appack_loss` | Implementiert als **ADDITIONAL MECHANISM** (`AxisAppAckMode::SeparateReturn`), nicht Basisinterface. |
| AoU-Abbildung | Coverage: ungeklärt; Profile: Bridge-Policy | Bridge-AXIS-P1 explizit als MODEL PROFILE; **keine AoU-Konformitätsbehauptung**. |
| Stream vs MM Response | Matrix: kein Stream-BRESP | Lokaler Abort = `STREAM_LOCAL_ABORT` / Bridge-Diagnostik; kein erfundenes BRESP. |
| AXI5-MM-P1 | Phase-1-Plan enthält Atomic/Poison/Trace | **Implementiert** als AXI5-MM-P1 (AtomicStore(ADD), Poison-Policies, Trace observability-only). AXIS und MM-Baseline bleiben getrennt; nach Infrastrukturänderungen beide Kampagnen erneut grün. |

## Keine echten Spec-Widersprüche

Continuous_Packets (kein TSTRB, TID/TDEST stabil, TKEEP-Regeln) ist intern konsistent zwischen `profiles.yaml` und `03_study_profiles.md`.
