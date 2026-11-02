# AXIS-P1 campaign results (axis_p1_automotive)

Eigenständiges AXI4-Stream-Untersuchungsprofil über UCIe-Transport.
Baseline `fault_handling_*` und `_baseline_archive_phase1/` unverändert.

All pass: **True**

| Experiment | Pass | Sink | Local/Remote beats | Effects | GT commit | Fate A/C/D |
| --- | --- | --- | --- | --- | --- | --- |
| `ax_clean_packet_imm` | True | IMM | 4/4 | 4 | 4 | UNKNOWN/UNKNOWN/UNKNOWN |
| `ax_clean_packet_pkt` | True | PKT | 4/4 | 1 | 1 | UNKNOWN/UNKNOWN/UNKNOWN |
| `ax_loss_mid_packet_imm` | True | IMM | 4/2 | 1 | 1 | UNKNOWN/UNKNOWN/UNKNOWN |
| `ax_loss_mid_packet_pkt` | True | PKT | 4/1 | 0 | 0 | UNKNOWN/UNKNOWN/UNKNOWN |
| `ax_loss_before_first_imm` | True | IMM | 0/0 | 0 | 0 | UNKNOWN/NOT_COMPLETED/NOT_COMPLETED |
| `ax_loss_before_first_pkt` | True | PKT | 0/0 | 0 | 0 | UNKNOWN/NOT_COMPLETED/NOT_COMPLETED |
| `ax_tlast_local_remote_loss` | True | PKT | 4/0 | 0 | 0 | UNKNOWN/UNKNOWN/UNKNOWN |
| `ax_imm_partial_then_blind_retry` | True | IMM | 4/4 | 4 | 4 | UNKNOWN/UNKNOWN/UNKNOWN |
| `ax_pkt_commit_then_local_timeout` | True | PKT | 2/2 | 1 | 1 | UNKNOWN/UNKNOWN/UNKNOWN |
| `ax_ack_loss_after_delivery` | True | IMM | 2/2 | 2 | 2 | UNKNOWN/UNKNOWN/UNKNOWN |
| `ax_observer_timed_vs_seq` | True | IMM,IMM | 2/2,2/0 | 1,0 | 1,0 | UNKNOWN/UNKNOWN/UNKNOWN,UNKNOWN/NOT_COMPLETED/NOT_COMPLETED |
| `ax_safe_retry_cancel0` | True | PKT | 2/2 | 1 | 1 | UNKNOWN/UNKNOWN/UNKNOWN |
| `ax_cancel_latency_race` | True | PKT | 2/2 | 1 | 1 | UNKNOWN/UNKNOWN/UNKNOWN |
| `ax_backpressure_tready` | True | IMM | 4/4 | 4 | 4 | UNKNOWN/UNKNOWN/UNKNOWN |
| `ax_continuous_tid_stable` | True | IMM | 1/0 | 0 | 0 | UNKNOWN/NOT_COMPLETED/NOT_COMPLETED |
| `ax_appack_loss` | True | PKT | 2/2 | 1 | 1 | UNKNOWN/UNKNOWN/UNKNOWN |
| `ax_reset_partial_packet` | True | IMM | 4/1 | 0 | 0 | UNKNOWN/UNKNOWN/UNKNOWN |
| `ax_late_data_after_restart` | True | IMM | 0/0 | 0 | 0 | UNKNOWN/NOT_COMPLETED/NOT_COMPLETED |
| `ax_multi_packet_inflight` | True | IMM,PKT,IMM | 2/2,2/2,1/1 | 2,1,1 | 2,1,1 | UNKNOWN/UNKNOWN/UNKNOWN,UNKNOWN/UNKNOWN/UNKNOWN,UNKNOWN/UNKNOWN/UNKNOWN |
| `ax_packet_len_variants` | True | IMM,IMM,PKT | 1/1,4/4,16/16 | 1,4,1 | 1,4,1 | UNKNOWN/UNKNOWN/UNKNOWN,UNKNOWN/UNKNOWN/UNKNOWN,UNKNOWN/UNKNOWN/UNKNOWN |
| `ax_replay_no_dup_effect` | True | IMM | 2/2 | 2 | 2 | UNKNOWN/UNKNOWN/UNKNOWN |
| `ax_app_level_repeat_dup` | True | PKT | 2/2 | 1 | 2 | UNKNOWN/UNKNOWN/UNKNOWN |
| `ax_dup_suppress` | True | PKT | 2/2 | 0 | 1 | UNKNOWN/UNKNOWN/UNKNOWN |
| `ax_packet_id_reuse` | True | PKT,PKT | 1/1,1/1 | 1,1 | 1,1 | UNKNOWN/UNKNOWN/UNKNOWN,UNKNOWN/UNKNOWN/UNKNOWN |

## Observer compare

Mode: `single_run_two_txs`

- Profile A: class_eq=True seq_eq=True timed_eq=False reordered=False
- Profile B: class_eq=True seq_eq=True timed_eq=False reordered=False
- Profile C: class_eq=False seq_eq=False timed_eq=False reordered=False
- Profile D: class_eq=False seq_eq=False timed_eq=False reordered=False

## Assumptions

- Bridge-AXIS-P1: one UCIe segment per AXIS transfer (MODEL PROFILE)
- AoU normative AXIS↔Flit mapping ungeklärt (AoU PDF absent)
- Instant cancel (cancel_latency_ns==0) is MODEL ASSUMPTION
- No AXI-Stream BRESP; local abort is bridge lifecycle only
- SINK-PKT: packet_complete ≠ process_start ≠ atomic effect
- App-ACK / packet_op_id / dup-suppress are ADDITIONAL MECHANISMS

