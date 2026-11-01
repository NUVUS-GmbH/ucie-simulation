# Cross-guest fault-induced temporal interference

Timed SystemC experiment: can a UCIe fault confined to Guest A raise
shared-resource occupancy enough for an otherwise deadline-compliant
Guest B transaction to miss its real-time requirement?

**FAULT_INDUCED_CROSS_GUEST_DEADLINE_VIOLATION = true**

## 1. Experiment configuration

| Parameter | Value |
| --- | --- |
| Guests | A (best-effort), B (safety-critical) |
| Guest A workload | 6 `MemoryWrite` |
| Guest B workload | 1 non-idempotent `IncrementCounter` |
| Shared resource | one UCIe link, one in-flight payload slot (baseline) |
| Arbitration | round-robin (baseline) |
| Credits / queues | unchanged across compared runs |
| Guest A fault schedule | attempt 1 CRC error, attempt 2 CRC error, attempt 3 accept (per selected flit role) |
| Guest B faults in main comparison | none |
| Clock | 1.0 ns |

Compared runs differ only in fault injection applied to Guest A.

## 2. Deadline derivation

### Option B — relative baseline budget (primary)

No external automotive timing requirement is bound into this model, so
the campaign uses a transparent **experimental timing budget**:

```text
deadline = shared_quiet_latency × 1.20
         = 61.0 ns × 1.20
         = 73.20 ns
```

This is an experimental timing budget, not an automotive-standard deadline.

### Option A — derived timing budget (documented)

```text
isolated Guest B service time     = 39.0 ns
normal sharing overhead           = 22.0 ns
allowed interference margin       = deadline − isolated
                                  = 34.20 ns
deadline                          = 73.20 ns
```

The margin equals normal sharing plus the Option B headroom above the
quiet baseline.

## 3. Isolated baseline (Run 1)

- Guest B intrinsic latency: **39.0 ns**
- CRC failures: 0
- Retries / replays: 0 / 0
- Commit count: 1

## 4. Shared quiet baseline (Run 2)

- Guest B latency: **61.0 ns**
- Normal sharing overhead vs isolated: **22.0 ns**
- Deadline (73.20 ns): **PASS** (slack 12.20 ns)
- Guest B CRC / replay / commit: 0 / 0 / 1

## 5. Guest-A-only faulted result (Run 3)

- Guest B latency: **106.0 ns**
- Deadline (73.20 ns): **FAIL** (slack n/a ns)
- Guest B CRC failures (transaction retries): 0
- Guest B replays: 0
- Guest B commit count: 1
- Guest B completion: True
- Run CRC failures (Guest A recovery traffic): 36
- Guest A any retry: True

## 6. Guest-B-only faulted result (Run 4)

- Guest B latency: **117.0 ns**
- Guest B retries / replays: 6 / 6
- Guest A any retry: False

This separates self-induced retry delay from cross-guest replay interference.

## 7. Timing decomposition

```text
Guest B total latency (faulted) = 106.0 ns
  intrinsic service time        = 39.0 ns
  + normal shared interference  = 22.0 ns
  + fault-induced interference  = 45.0 ns
```

Guest B wait attribution (faulted run, simulator instrumentation):

| Cause | ns |
| --- | ---: |
| WAIT_ARBITRATION | 1.0 |
| WAIT_GUEST_A_NORMAL_TRAFFIC | 6.0 |
| WAIT_GUEST_A_REPLAY | 16.0 |
| WAIT_OWN_REPLAY | 6.0 |
| WAIT_CREDIT | 0.0 |

## 8. Retry / resource occupancy

| Metric | shared_quiet | guest_a_storm |
| --- | ---: | ---: |
| retry buffer peak | 1 | 1 |
| TX slot hold by Guest A (ns) | 120.0 | 336.0 |
| TX slot hold by Guest A that is replay (ns) | 0.0 | 336.0 |
| Guest B wait attributable to Guest A replay (ns) | 0.0 | 16.0 |
| Guest A replay attempts | 0 | 24 |

The retry-buffer peak remains 1 in both runs, matching the model’s
single in-flight payload slot. Interference is explained by extended
TX-slot occupancy under Guest A replay, not by retry-buffer saturation.

## 9. Deadline sweep (Option C)

Exact fault-induced deadline-violation interval:

```text
61.0 ns <= deadline < 106.0 ns
```

Sampled on [60.0, 110.0] ns with step 1.0 ns:
first=61.0, last=105.0.

Full sweep table: `simulation/results/cross_guest_deadline_sweep.csv`.

## 10. Mitigation comparison

Each mitigation is tested one at a time against the Guest-A-faulted workload.

| Mitigation | Guest B latency (ns) | Deadline | Guest A max latency (ns) | Notes |
| --- | ---: | --- | ---: | --- |
| A. Current design (single slot, RR) | 106.0 | FAIL | 393.0 | baseline interference |
| B. Safety-priority arbitration | 39.0 | PASS | 393.0 | strict priority for safety-critical Guest B at the shared AXI port |
| C. Per-guest transmission slots | 71.0 | PASS | 369.0 | per-guest outstanding TX slots with per-guest sequence spaces |
| D. Replay budget yield | 59.0 | PASS | 432.0 | per-guest TX slot + yield Guest A replay after first recovery attempt |
| E. Reserved safety bandwidth | 71.0 | PASS | 369.0 | weighted RR (B weight 4) plus per-guest TX slots |
| F. Virtual channels / separated queues | 71.0 | PASS | 369.0 | separated per-guest resource state / VC sequence spaces |

Throughput / fairness: Guest A completion latency rises or falls with
how aggressively the mitigation steals the shared TX opportunity.
Safety-priority and reserved bandwidth improve Guest B at the expense
of Guest A fairness. Per-guest slots / virtual channels bound Guest B
without requiring Guest A to fail.

## 11. Minimized event trace

Guest-A-faulted run (truncated):

```text
2000.000 ns  g0  create  Guest A creates T1 MEMORY_WRITE
2000.000 ns  g0  create  Guest A creates T2 MEMORY_WRITE
2000.000 ns  g0  create  Guest A creates T3 MEMORY_WRITE
2000.000 ns  g0  create  Guest A creates T4 MEMORY_WRITE
2000.000 ns  g0  create  Guest A creates T5 MEMORY_WRITE
2000.000 ns  g0  create  Guest A creates T6 MEMORY_WRITE
2000.000 ns  g1  create  Guest B creates T7 INCREMENT_COUNTER
2002.000 ns  g0  phase  AXI accepts T1
2003.000 ns  g1  phase  AXI accepts T7
2004.000 ns  g0  phase  AoU packet ready
2004.000 ns  g0  phase  AXI accepts T2
2004.000 ns  g0  phase  queued for the link
2004.000 ns  g0  phase  UCIe TX begins
2004.000 ns  g0  fault  fault 3 at TRANSMITTING
2005.000 ns  g0  phase  AXI accepts T3
2006.000 ns  g1  phase  AoU packet ready
2006.000 ns  g0  phase  AXI accepts T4
2007.000 ns  g0  phase  AXI accepts T5
2008.000 ns  g0  phase  AoU packet ready
2008.000 ns  g0  phase  AXI accepts T6
2010.000 ns  g0  phase  AoU packet ready
2012.000 ns  g0  phase  AoU packet ready
2013.000 ns  g0  crc  UCIe CRC failure on the request flit (1-bit corruption); replay requested, streak 1
2014.000 ns  g0  phase  AoU packet ready
2016.000 ns  g0  phase  AoU packet ready
2022.000 ns  g0  crc  UCIe CRC failure on the request flit (1-bit corruption); replay requested, streak 2
2029.000 ns  g0  phase  remote endpoint receives the request
2032.000 ns  g0  phase  queued for the link
2032.000 ns  g0  wdata_tx  write-data TX begins
2041.000 ns  g0  crc  UCIe CRC failure on the wdata flit (1-bit corruption); replay requested, streak 1
2050.000 ns  g0  crc  UCIe CRC failure on the wdata flit (1-bit corruption); replay requested, streak 2
2059.000 ns  g0  phase  target accepts T1
2060.000 ns  g1  phase  queued for the link
2060.000 ns  g1  phase  UCIe TX begins
2063.000 ns  g0  phase  ARCHITECTURAL COMMIT
2065.000 ns  g0  phase  response created
2067.000 ns  g0  phase  response queued
2067.000 ns  g1  phase  remote endpoint receives the request
2069.000 ns  g0  phase  queued for the link
2069.000 ns  g0  phase  response TX
```

## 12. Scientific interpretation

A fault confined to Guest A caused an otherwise deadline-compliant Guest B transaction to violate its real-time requirement through contention for a shared UCIe transmission resource. Guest B experienced no CRC errors and no replay, committed exactly once, and completed successfully, but exceeded its experimental timing budget because Guest A’s replay traffic retained the shared transmit resource. Functional isolation did not imply temporal isolation in this shared UCIe-based virtualized configuration.

Preferred wording for a positive result is **cross-guest temporal
interference** or **fault-induced temporal interference through a
shared resource**. This report does **not** claim an ISO 26262
freedom-from-interference violation; that would require an explicit
ISO 26262 analysis beyond this experiment.

## 13. Limitations

- Timing numbers are a SystemC [MODEL], not a PHY-accurate UCIe implementation.
- The Option B deadline is an experimental budget, not an OEM requirement.
- Single in-flight payload slot is a model policy that makes TX-slot contention visible.
- Per-guest / VC mitigations introduce per-guest sequence spaces; that is an architectural counterfactual, not a claim that UCIe mandates those VCs.
- Wait attribution covers arbitration, credit, and TX-slot blocking; target/response service time remains in the residual latency.
- Only one mitigation is enabled per run; combinations are out of scope.

## Counterfactual summary

```text
shared_quiet:
  latency  = 61.0 ns
  deadline = 73.20 ns
  slack    = 12.20 ns

guest_a_faulted:
  latency  = 106.0 ns
  deadline = 73.20 ns
  slack    = n/a ns

fault-induced latency increase = 45.0 ns (73.8%)
```
