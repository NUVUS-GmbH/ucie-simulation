# AXIS-P1 Coverage Matrix (Phase 2/3 measured)

Status relative to `docs/study/phase1_axis_axi5/05_sources_coverage_matrix.md`.
Does not overwrite the Phase-1 baseline MM coverage matrix.

| Thema | Status | Evidence |
| --- | --- | --- |
| TVALID/TREADY Transfer | implementiert | ax_clean_*, ax_backpressure_tready |
| TKEEP / Continuous_Packets | implementiert | ax_packet_len_variants, ax_continuous_tid_stable |
| TLAST Paketgrenze | implementiert | ax_clean_packet_*, ax_tlast_local_remote_loss |
| SINK-IMM | implementiert | ax_clean_packet_imm, ax_loss_mid_packet_imm |
| SINK-PKT (reception≠process≠effect) | implementiert | ax_clean_packet_pkt |
| App-ACK (additional) | implementiert | ax_appack_loss |
| Packet-ID / Dup-suppress (additional) | implementiert | ax_dup_suppress, ax_packet_id_reuse |
| Bridge one-segment-per-transfer | abstrahiert (MODEL) | all ax_*; AoU ungeklärt |
| AXI-Stream BRESP | nicht vorhanden | verified: no completed_ok without App-ACK |

## Experiment PASS/FAIL

| ID | Pass | Failures |
| --- | --- | --- |
| `ax_clean_packet_imm` | True | - |
| `ax_clean_packet_pkt` | True | - |
| `ax_loss_mid_packet_imm` | True | - |
| `ax_loss_mid_packet_pkt` | True | - |
| `ax_loss_before_first_imm` | True | - |
| `ax_loss_before_first_pkt` | True | - |
| `ax_tlast_local_remote_loss` | True | - |
| `ax_imm_partial_then_blind_retry` | True | - |
| `ax_pkt_commit_then_local_timeout` | True | - |
| `ax_ack_loss_after_delivery` | True | - |
| `ax_observer_timed_vs_seq` | True | - |
| `ax_safe_retry_cancel0` | True | - |
| `ax_cancel_latency_race` | True | - |
| `ax_backpressure_tready` | True | - |
| `ax_continuous_tid_stable` | True | - |
| `ax_appack_loss` | True | - |
| `ax_reset_partial_packet` | True | - |
| `ax_late_data_after_restart` | True | - |
| `ax_multi_packet_inflight` | True | - |
| `ax_packet_len_variants` | True | - |
| `ax_replay_no_dup_effect` | True | - |
| `ax_app_level_repeat_dup` | True | - |
| `ax_dup_suppress` | True | - |
| `ax_packet_id_reuse` | True | - |

