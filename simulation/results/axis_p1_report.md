# AXIS-P1 Fault-Injection Report

**Profil:** AXIS-P1-Continuous (IHI 0051B / Continuous_Packets).
**Transport:** bestehender UCIe-Pfad; Bridge-Policy `one_ucie_segment_per_transfer`.
**Nicht überschrieben:** `fault_handling_*`, `_baseline_archive_phase1/`.

## Was zeigt das Experiment?

1. AXI4-Stream-Transfers zählen nur bei TVALID∧TREADY; lokale Handshakes sind keine entfernte Anwendungswirkung.
2. SINK-IMM erzeugt Teilwirkungen pro remote akzeptiertem Beat; SINK-PKT puffert bis TLAST und trennt ausdrücklich: Paketempfang → Process-Start → atomare Wirkung.
3. Ohne App-ACK liefert das Basisinterface kein COMPLETED für die Wirkung (kein Stream-BRESP). Fehlendes UCIe-ACK allein ist nicht NOT_COMPLETED.
4. Blind-Retry nach Teilaccept (IMM) bzw. App-Wiederholung kann Doppelwirkungen erzeugen; optionale Dup-Unterdrückung ist ein Zusatzmechanismus.
5. Observer-Vergleiche müssen event-sequence und timed getrennt ausweisen.

Automotive all_pass=True (n=24); Reference all_pass=True (n=7).

## Welche Annahmen benötigt es?

- Bridge-AXIS-P1: one UCIe segment per AXIS transfer (MODEL PROFILE)
- AoU normative AXIS↔Flit mapping ungeklärt (AoU PDF absent)
- Instant cancel (cancel_latency_ns==0) is MODEL ASSUMPTION
- No AXI-Stream BRESP; local abort is bridge lifecycle only
- SINK-PKT: packet_complete ≠ process_start ≠ atomic effect
- App-ACK / packet_op_id / dup-suppress are ADDITIONAL MECHANISMS

## Welche stärkere Aussage ist nicht nachgewiesen?

- AoU-normative Abbildung AXIS↔UCIe-Flit (Quellenlücke).
- Wire-Cancel-Protokoll (Instant-Cancel bleibt MODEL ASSUMPTION).
- Dass Stream „weniger Ambiguität“ als MM hat — Gegenbeispiele (TLAST lokal ≠ Remote-PKT-Commit; IMM-Teilwirkungen) widerlegen das.
- AXI5-Stream TWAKEUP/Parity (außerhalb Profil).
- Vendor-CSR / Exactly-once als Spec-Fakt.

## Unterschiede gegenüber AXI4 Memory-Mapped

| Dimension | AXI4-MM (Baseline) | AXIS-P1 |
| --- | --- | --- |
| Protokollabschluss | B/R Handshake (OKAY/SLVERR) | lokales TLAST; kein BRESP |
| Wirkung | Target-Commit nach Request+Data | Senkenmodell IMM/PKT |
| Teilwirkung | typisch eine Commit-Aktion/Txn | IMM: ja; PKT: nein bis Process+Effect |
| Lokaler Fehlerabschluss | SLVERR auf Response-Kanal | Bridge STREAM_LOCAL_ABORT |
| Classifier COMPLETED | OKAY gesehen | nur mit optionalem App-ACK |
| Transport | gleicher UCIe CRC/Replay/LinkError-Pfad | gleicher Pfad, StreamSegment-Flits |

## Observer (ax_observer_timed_vs_seq)

Mode: `single_run_two_txs`

- A: classification_equal=True, event_sequence_equal=True, timed_equal=False, reordered=False
- B: classification_equal=True, event_sequence_equal=True, timed_equal=False, reordered=False
- C: classification_equal=False, event_sequence_equal=False, timed_equal=False, reordered=False
- D: classification_equal=False, event_sequence_equal=False, timed_equal=False, reordered=False

## Evidenzstufen

- **directly measured:** Kampagnen-JSON unter `axis_p1_runs/`
- **under explicit model assumptions:** Bridge-Policy, Instant-Cancel, App-ACK
- **not yet established:** AoU-Volltext-Konformität

