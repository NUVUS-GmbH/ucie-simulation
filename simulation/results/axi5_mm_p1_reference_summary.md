# AXI5-MM-P1 campaign results (reference)

Teilumfang AtomicStore(ADD) / Poison / Trace über UCIe-Transport.
AXIS-P1 und MM-Baseline (`fault_handling_*`) nicht vermischt.

All pass: **True**

| Experiment | Pass | atomic_value | ops | commits | Fate A/C/D | BRESP |
| --- | --- | --- | --- | --- | --- | --- |
| `a5_atomic_store_add_ok` | True | 15 | 1 | 1 | COMPLETED/COMPLETED/COMPLETED | OKAY |
| `a5_atomic_blind_retry` | True | 20 | 2 | 2 | COMPLETED/COMPLETED/COMPLETED | OKAY |
| `a5_atomic_never_handed` | True | 10 | 0 | 0 | UNKNOWN/NOT_COMPLETED/NOT_COMPLETED | SLVERR |
| `a5_poison_reject_no_commit` | True | 10 | 0 | 0 | UNKNOWN/UNKNOWN/UNKNOWN | SLVERR |
| `a5_poison_with_slverr_independent` | True | 17 | 1 | 0,1 | COMPLETED/COMPLETED/COMPLETED,UNKNOWN/UNKNOWN/UNKNOWN | OKAY,SLVERR |
| `a5_trace_not_in_classifier` | True | 20 | 2 | 1,1 | UNKNOWN/UNKNOWN/UNKNOWN,UNKNOWN/UNKNOWN/UNKNOWN | SLVERR,SLVERR |
| `a5_safe_retry_cancel0` | True | 15 | 1 | 1 | COMPLETED/COMPLETED/COMPLETED | OKAY |

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

