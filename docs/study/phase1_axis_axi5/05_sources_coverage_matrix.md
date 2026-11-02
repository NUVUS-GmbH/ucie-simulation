# Quellen- und Coverage-Matrix — Phase 1 Extension

**Statuswerte:**  
`implementiert` | `abstrahiert` | `geplant` | `nicht untersucht` | `ungeklärt`

Bezug: Baseline-Matrix `simulation/results/fault_handling_coverage_matrix.md` bleibt für Draft V0.1 AXI4↔AXI4 gültig. Diese Matrix deckt die **Erweiterung** ab.

## A. AXI4 Memory-Mapped (Baseline, Referenz)

| Thema | Quelle | Status | Notes |
| --- | --- | --- | --- |
| VALID/READY Address/Data/Resp | IHI0022L Response/Handshake chapters | implementiert / abstrahiert | timed delays |
| SLVERR/OKAY/DECERR enum | IHI0022L | implementiert | DECERR decode map: nicht untersucht |
| Local SLVERR on LinkError (ERR-05) | Draft + Policy | implementiert | |
| WDT-03/04/06 | Draft | implementiert | WDT timing shortened in campaign |
| Classifier A–D, diag_never_handed | Model + Draft | implementiert | ACK≠non-exec measured |
| Instant cancel | — | abstrahiert | MODEL ASSUMPTION |
| Ground-truth isolation | Model rule | implementiert | |

## B. AXI4-Stream (AXIS-P1)

| Thema | Quelle | Status | Notes |
| --- | --- | --- | --- |
| TVALID/TREADY Transfer | IHI0051B §2.2 | implementiert | axis_p1 campaign; beat nur bei Handshake |
| TDATA Nutzdaten | §2.1/2.4 | implementiert | width MODEL PROFILE 128b |
| TKEEP | §2.5; Continuous §3.3 | implementiert | last-beat partial keep |
| TSTRB / Position bytes | §2.5 | nicht untersucht | Continuous_Packets → abwesend |
| TLAST Paketgrenze | §2.6 | implementiert | lokal ≠ remote commit |
| TID/TDEST | §2.7 | implementiert | 4b/4b; Continuous negativtest |
| TUSER | §2.1 | implementiert (minimal) | default 0; App-ACK optional |
| Continuous_Packets | §3.3 | implementiert | Profilzwang + TID mid-packet FAIL |
| Interleaving / sparse streams | §1.2, Kap.4 | nicht untersucht | |
| Upsize/Downsize interconnect | §2.4 notes | nicht untersucht | |
| TWAKEUP (AXI5-Stream) | §2.3 | nicht untersucht | bewusst außerhalb AXIS-P1 |
| Stream parity (AXI5-Stream) | Kap.5 | nicht untersucht | |
| SINK-IMM / SINK-PKT | — | implementiert | PKT: Empfang≠Process≠Effect |
| App-ACK Rückkanal | — | implementiert (separat) | ADDITIONAL MECHANISM |
| Packet-ID / Dup-suppress | — | implementiert (separat) | lifetime/capacity/reset policy |
| AXIS Bridge über UCIe | AoU/Databook | ungeklärt / abstrahiert (Policy) | Bridge-AXIS-P1; AoU-PDF fehlt |

## C. Streaming-Transport über UCIe

| Thema | Quelle | Status | Notes |
| --- | --- | --- | --- |
| CRC detect / replay | UCIe 2.0 §3.7/§3.8 | implementiert | LFSR nicht ausgeführt |
| LinkError residency 16 ms | §10.3.3.7 | implementiert | |
| Sideband timeout 8 ms | §10.3.3.8 | implementiert | |
| No protocol quiescence guarantee | §10.3.4 | abstrahiert | |
| `protocol: streaming` metadata | Reference slide | abstrahiert | ≠ AXI4-Stream |
| Normative AXIS↔UCIe mapping | AoU 0.8 / IP | ungeklärt | AoU PDF fehlt |
| Format 6 negotiation | Draft D2D-01 | abstrahiert | |

## D. AXI5 MM Teilumfang (AXI5-MM-P1)

| Thema | Quelle | Status | Notes |
| --- | --- | --- | --- |
| AtomicStore overview | IHI0022L A6.4.1 | implementiert | a5_atomic_*; B only, no R prior-value |
| Operator ADD | A6.4.2 | implementiert | only; AWATOP[2:0]=000 |
| AWATOP signaling | A6.4.6 | implementiert | Store ADD LE = 0x10 (Tables A6.8/A6.9) |
| AtomicLoad/Swap/Compare | A6.4.1 | nicht untersucht | P1 ausgeschlossen |
| Exclusive accesses | A6.3 | nicht untersucht | a5_exclusive_smoke_out_of_scope |
| Atomic dependencies / ID rules | A6.4.4–A6.4.9 | abstrahiert (Minimal) | concurrent different AXI IDs |
| Support property | A6.4.10 | abstrahiert | axi5_mm_p1_enabled config |
| Poison WPOISON/RPOISON | A16.1 | implementiert | named target policies; no undo |
| Poison ∧ error independence | A16.1 | gemessen | a5_poison_with_slverr_independent |
| Poison + MTE | A12.2.10 | nicht untersucht | |
| Trace signals | A12.3 | implementiert (observe only) | a5_trace_not_in_classifier |
| Parity A16.2 | A16.2 | nicht untersucht | |
| Full AXI5 interface class | Kap. B2 | nicht untersucht | |

## E. Projekt-Draft / IP

| Thema | Quelle | Status |
| --- | --- | --- |
| ERR-01 Poison transport USER vs AXI5 | Draft §9 | AXI5-Pfad gemessen (Studienvariante); USER-Pfad weiter offen |
| Vendor CSR maps | Draft §9 | ungeklärt |
| Exactly-once token | Model optional | abstrahiert |

## Änderungsregel

Phase 2 aktualisiert Statuszellen nur bei Merge der Implementierung.  
Phase 3 setzt „geplant“→„implementiert“ erst nach gemessener Kampagne.
