# AXI5-MM-P1 Coverage Matrix (measured)

Status relative to `docs/study/phase1_axis_axi5/05_sources_coverage_matrix.md` §D.

| Thema | Status | Evidence |
| --- | --- | --- |
| AtomicStore overview A6.4.1 | implementiert | a5_atomic_store_add_ok |
| Operator ADD A6.4.2 | implementiert | a5_atomic_* ; memory value checked |
| AWATOP signaling A6.4.6 | implementiert | AWATOP=0x10 Store ADD LE |
| No R prior-value (Store) | implementiert | axi5_read_response_created=false |
| Blind retry double ADD | gemessen | a5_atomic_blind_retry atomic_value=20 |
| Poison WPOISON A16.1 | implementiert | a5_poison_* named target policies |
| Poison ⊥ Error | gemessen | a5_poison_with_slverr_independent |
| No Poison undo | gemessen | a5_poison_after_commit_no_undo |
| Trace A12.3 observability | implementiert | a5_trace_not_in_classifier |
| AtomicLoad/Swap/Compare/Exclusive | nicht untersucht | a5_exclusive_smoke_out_of_scope |
| Full AXI5 | nicht behauptet | profile subset only |

## Experiment PASS/FAIL

| ID | Pass | Failures |
| --- | --- | --- |
| `a5_atomic_store_add_ok` | True | - |
| `a5_atomic_commit_then_local_slverr` | True | - |
| `a5_atomic_blind_retry` | True | - |
| `a5_atomic_never_handed` | True | - |
| `a5_atomic_ack_loss` | True | - |
| `a5_atomic_loss_before_exec` | True | - |
| `a5_atomic_timeout_after_commit` | True | - |
| `a5_atomic_late_response` | True | - |
| `a5_atomic_concurrent` | True | - |
| `a5_atomic_dup_suppress` | True | - |
| `a5_atomic_reset_restart` | True | - |
| `a5_poison_reject_no_commit` | True | - |
| `a5_poison_reject_okay` | True | - |
| `a5_poison_with_slverr_independent` | True | - |
| `a5_poison_after_commit_no_undo` | True | - |
| `a5_poison_accept_marked` | True | - |
| `a5_poison_partial_then_timeout` | True | - |
| `a5_trace_not_in_classifier` | True | - |
| `a5_safe_retry_cancel0` | True | - |
| `a5_exclusive_smoke_out_of_scope` | True | - |

