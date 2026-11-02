# AXIS-P1 campaign results (axis_p1_reference_ucie2x)

Eigenständiges AXI4-Stream-Untersuchungsprofil über UCIe-Transport.
Baseline `fault_handling_*` und `_baseline_archive_phase1/` unverändert.

All pass: **True**

| Experiment | Pass | Sink | Local/Remote beats | Effects | GT commit | Fate A/C/D |
| --- | --- | --- | --- | --- | --- | --- |
| `ax_clean_packet_imm` | True | IMM | 4/4 | 4 | 4 | UNKNOWN/UNKNOWN/UNKNOWN |
| `ax_clean_packet_pkt` | True | PKT | 4/4 | 1 | 1 | UNKNOWN/UNKNOWN/UNKNOWN |
| `ax_loss_mid_packet_imm` | True | IMM | 4/2 | 1 | 1 | UNKNOWN/UNKNOWN/UNKNOWN |
| `ax_loss_before_first_imm` | True | IMM | 0/0 | 0 | 0 | UNKNOWN/NOT_COMPLETED/NOT_COMPLETED |
| `ax_ack_loss_after_delivery` | True | IMM | 2/2 | 2 | 2 | UNKNOWN/UNKNOWN/UNKNOWN |
| `ax_backpressure_tready` | True | IMM | 4/4 | 4 | 4 | UNKNOWN/UNKNOWN/UNKNOWN |
| `ax_safe_retry_cancel0` | True | PKT | 2/2 | 1 | 1 | UNKNOWN/UNKNOWN/UNKNOWN |

## Observer compare

Mode: `unavailable`


## Assumptions

- Bridge-AXIS-P1: one UCIe segment per AXIS transfer (MODEL PROFILE)
- AoU normative AXIS↔Flit mapping ungeklärt (AoU PDF absent)
- Instant cancel (cancel_latency_ns==0) is MODEL ASSUMPTION
- No AXI-Stream BRESP; local abort is bridge lifecycle only
- SINK-PKT: packet_complete ≠ process_start ≠ atomic effect
- App-ACK / packet_op_id / dup-suppress are ADDITIONAL MECHANISMS

