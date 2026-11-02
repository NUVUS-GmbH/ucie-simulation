# AXI5-MM-P1 campaign results (automotive)

Teilumfang AtomicStore(ADD) / Poison / Trace über UCIe-Transport.
AXIS-P1 und MM-Baseline (`fault_handling_*`) nicht vermischt.

All pass: **True**

| Experiment | Pass | atomic_value | ops | commits | Fate A/C/D | BRESP |
| --- | --- | --- | --- | --- | --- | --- |
| `a5_atomic_store_add_ok` | True | 15 | 1 | 1 | COMPLETED/COMPLETED/COMPLETED | OKAY |
| `a5_atomic_commit_then_local_slverr` | True | 15 | 1 | 1 | UNKNOWN/UNKNOWN/UNKNOWN | SLVERR |
| `a5_atomic_blind_retry` | True | 20 | 2 | 2 | COMPLETED/COMPLETED/COMPLETED | OKAY |
| `a5_atomic_never_handed` | True | 10 | 0 | 0 | UNKNOWN/NOT_COMPLETED/NOT_COMPLETED | SLVERR |
| `a5_atomic_ack_loss` | True | 15 | 1 | 1 | UNKNOWN/UNKNOWN/UNKNOWN | SLVERR |
| `a5_atomic_loss_before_exec` | True | 10 | 0 | 0 | UNKNOWN/UNKNOWN/NOT_COMPLETED | SLVERR |
| `a5_atomic_timeout_after_commit` | True | 15 | 1 | 1 | UNKNOWN/UNKNOWN/UNKNOWN | SLVERR |
| `a5_atomic_late_response` | True | 15 | 1 | 1 | UNKNOWN/UNKNOWN/UNKNOWN | SLVERR |
| `a5_atomic_concurrent` | True | 17 | 2 | 1,1 | COMPLETED/COMPLETED/COMPLETED,COMPLETED/COMPLETED/COMPLETED | OKAY,OKAY |
| `a5_atomic_dup_suppress` | True | 15 | 1 | 1 | COMPLETED/COMPLETED/COMPLETED | OKAY |
| `a5_atomic_reset_restart` | True | 20 | 2 | 2 | COMPLETED/COMPLETED/COMPLETED | OKAY |
| `a5_poison_reject_no_commit` | True | 10 | 0 | 0 | UNKNOWN/UNKNOWN/UNKNOWN | SLVERR |
| `a5_poison_reject_okay` | True | 10 | 0 | 0 | COMPLETED/COMPLETED/COMPLETED | OKAY |
| `a5_poison_with_slverr_independent` | True | 17 | 1 | 0,1 | COMPLETED/COMPLETED/COMPLETED,UNKNOWN/UNKNOWN/UNKNOWN | OKAY,SLVERR |
| `a5_poison_after_commit_no_undo` | True | 15 | 1 | 1,0 | COMPLETED/COMPLETED/COMPLETED,UNKNOWN/UNKNOWN/UNKNOWN | OKAY,SLVERR |
| `a5_poison_accept_marked` | True | 15 | 1 | 1 | COMPLETED/COMPLETED/COMPLETED | OKAY |
| `a5_poison_partial_then_timeout` | True | 10 | 0 | 0 | UNKNOWN/UNKNOWN/UNKNOWN | SLVERR |
| `a5_trace_not_in_classifier` | True | 20 | 2 | 1,1 | UNKNOWN/UNKNOWN/UNKNOWN,UNKNOWN/UNKNOWN/UNKNOWN | SLVERR,SLVERR |
| `a5_safe_retry_cancel0` | True | 15 | 1 | 1 | COMPLETED/COMPLETED/COMPLETED | OKAY |
| `a5_exclusive_smoke_out_of_scope` | True | 10 | 0 | 0 | UNKNOWN/UNKNOWN/UNKNOWN | SLVERR |

## Assumptions

- Bridge packing of AWATOP/WPOISON/Trace onto UCIe flits is MODEL PROFILE (no AoU claim)
- AtomicStore(ADD) only — AtomicLoad/Swap/Compare/Exclusive out of scope
- AtomicStore returns write response only; no fabricated R prior-value
- Poison target policies are named MODEL/TARGET choices; Spec A16.1 leaves commit reaction open
- Poison must not undo a prior commit (study invariant)
- Trace is observability-only; classify()/InitiatorView never read Trace
- Instant cancel (cancel_latency_ns==0) remains MODEL ASSUMPTION
- COMPLETED under OKAY is transfer-class fate; remote effect checked via atomic_value separately
- SINK-PKT atomic commit assumption belongs to AXIS, not this MM profile

