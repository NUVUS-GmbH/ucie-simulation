# Reference-architecture validation (Torsten / Swissbit-style)

This report re-runs the timed SystemC UCIe/AoU/AXI fault-injection study against the concrete D2D + PHY reference configuration. Negative results are treated as valuable.

## 1. Architecture mapping

```text
Chiplet A
AXI Streaming → Streaming Interface / FDI → UCIe D2D Controller → RDI → UCIe PHY
                                    | Mainband + Sideband |
UCIe PHY → RDI → UCIe D2D Controller → Streaming Interface / FDI → AXI Streaming
Chiplet B
```

Layers are retained separately in the model: AXI/streaming, FDI/AoU, UCIe D2D adapter,
RDI/PHY abstraction, mainband, and sideband.

## 2. Reference parameters

| Parameter | Value | Provenance |
| --- | --- | --- |
| Link width x16 | (see config) | REFERENCE-SLIDE VALUE |
| FDI width 512 bit | (see config) | REFERENCE-SLIDE VALUE |
| Flit Format 6 / Streaming / CRC / Retry | (see config) | REFERENCE-SLIDE VALUE |
| TX Mainband FIFO 32 | (see config) | REFERENCE-SLIDE VALUE |
| RX Mainband FIFO 64 | (see config) | REFERENCE-SLIDE VALUE |
| Retry Buffer 16 | (see config) | REFERENCE-SLIDE VALUE |
| Sideband FIFO 8 | (see config) | REFERENCE-SLIDE VALUE |
| Early credit return / threshold 2 | (see config) | REFERENCE-SLIDE VALUE |
| L1 enabled / Error logging enabled | (see config) | REFERENCE-SLIDE VALUE |
| LinkError residency 16 ms | (see config) | UCIe SPEC VALUE |
| Sideband timeout 8 ms | (see config) | UCIe SPEC VALUE |
| REPLAY_TIMEOUT_FLIT_COUNT 375 | (see config) | UCIe SPEC VALUE |
| Retrain = 5 us | (see config) | SIMULATION CONFIGURATION |
| LinkReset = 10 us | (see config) | SIMULATION CONFIGURATION |
| serialization_cycles = 4 | (see config) | SIMULATION CONFIGURATION |
| AoU credit_depth = 32 | (see config) | ASSUMPTION |
| L1 entry/exit/hold dwells | (see config) | ASSUMPTION |
| Outstanding AXI fate after LinkError = Discard | (see config) | POLICY |

Configs: `configs/torsten_reference_ucie2x.yaml`, `configs/torsten_reference_ucie3.yaml`.

## 3. Assumptions

- AoU credit depth = 32 (not on slide; sized to exercise retry/TX without artificial starvation).
- Serialization/propagation/buffer cycles are simulation configuration, not Swissbit timing.
- Retrain = 5 us, LinkReset = 10 us, reactivation = 2 us are simulation configuration.
- L1 entry/exit/hold dwells are simulation configuration.
- Early credit return frees a credit when RX accepts a flit; updates batch at threshold=2.
- Retrain preserves retry buffer contents (policy); LinkReset/LinkError flush (spec).
- Error log fields are implementation diagnostics, not standardized UCIe register layouts.
- Streaming/Format-6/x16/FDI-512 are recorded as reference metadata; PHY bit timing is not modeled.
- Single-inflight slot is an optional experiment mode; reference configs disable it.

## 4. Deviations from the slide

- PHY electrical timing, Format-6 byte packing, and FDI cycle accuracy are not modeled.
- Highest Common Speed is represented only as the configured serialization/propagation abstraction.
- AoU credit depth is assumed (32), not taken from the slide.
- Error-log fields are study abstractions, not vendor register maps.

## 5. Transaction-fate results

- Post-commit loss: ground commit=1, A=UNKNOWN, B=UNKNOWN, C=UNKNOWN, D=UNKNOWN, completed=False.
- Pre-target loss: ground commit=0, A=UNKNOWN, D=NOT_COMPLETED.
- OBSERVATIONAL_EQUIVALENCE (A/B/C) = true

## 6. Retry-buffer results

- Configured capacity: 16 flits (REFERENCE-SLIDE).
- Measured peak occupancy: **3**.
- Previous peak=1 under the automotive config is attributed to `single_inflight_slot: true`.

## 7. FIFO occupancy

- TX peak=11 (cap 32), RX peak=0 (cap 64) under sustained traffic.
- Transaction-fate UNKNOWN after commit remains reachable independent of FIFO fill in the baseline_unknown scenario.

## 8. Credit behavior

- Mode: Early credit return, update threshold = 2.
- CREDIT_CONSERVATION: held.
- Consumed/returned counters (throughput run): 120/120.

## 9. Diagnostic observability

- Error log entries (diagnostics experiment): 8.
- Observer A fate with diagnostics enabled: UNKNOWN.
- Additional diagnostics did **not** turn UNKNOWN into COMPLETED or NOT_COMPLETED for AXI-only.
- Observer D (ACK/NAK history assumption) still separates pre-target NOT_COMPLETED.

## 10. Recovery behavior

```text
State element        Retrain              LinkReset            LinkError
--------------------------------------------------------------------------
TX FIFO              0 (POLICY)           0 (SPEC)             0 (SPEC)
RX FIFO              0 (POLICY)           0 (SPEC)             0 (SPEC)
Retry Buffer         0 (POLICY)           0 (SPEC)             0 (SPEC)
Credits available    64 (POLICY)          63 (SPEC)            64 (SPEC)
Outstanding AXI      1 (POLICY)           1 (SPEC)             1 (SPEC)
Outstanding responses 1 (POLICY)           1 (SPEC)             1 (SPEC)
```

Post-transition: Retrain may preserve/resend retry (POLICY); LinkReset/LinkError flush (SPEC).

## 11. L1 behavior

- L1 events observed: 1.
- Model assumption: buffers/credits preserved across L1 (no flush).
- No evidence in this run that L1 alone creates transaction-fate ambiguity beyond normal Active traffic.

## 12. Cross-client behavior

- Quiet Client B latency: 46.0 ns; faulted: 90.0 ns; delta: 44.0 ns.
- Experimental deadline miss: True.
- Prior ~45 ns interference / deadline miss under single-inflight mode was partially/fully visible under reference multi-outstanding FIFOs.

## 13. Comparison with previous model

```text
Finding                              Old model      Reference model
-------------------------------------------------------------------
UNKNOWN after architectural commit   Yes            Yes
Duplicate commit after blind retry   Yes            Yes
Pre-target UNKNOWN (observer A)      Yes            Yes
ACK history reduces UNKNOWN (observer D) Yes            Yes
OBSERVATIONAL_EQUIVALENCE (A/B/C)    true           true
Retry buffer peak                    1              3
Cross-client / cross-guest delay     45 ns          44.0 ns
Deadline miss under Client-A replay  Yes            Yes
transport delivery knowledge != architectural commit Yes            Yes
Diagnostics turn UNKNOWN into determined fate (A) n/a (hypothesis) No
CREDIT_CONSERVATION under early return + recovery Yes (old model) Yes
```

### UCIe 2.x vs UCIe 3.0

- Baseline fate A identical across modes: True.
- UCIe 3 mode enables priority-sideband metadata only; it does not add an AXI commit bit, so transaction-fate labels are unchanged unless sideband latency metrics differ.

### Throughput vs reliability (overhead only)

- Mean latency CRC/retry on: 77.92 ns; off: 77.92 ns (fault-free sustained traffic).
- Not a recommendation to disable protection.

## 14. Limitations

- Not a PHY or vendor IP model.
- Format-6 optional bytes and electrical training are out of scope.
- Diagnostic fields are abstractions.
- L1 nesting with concurrent recovery commands uses a single command slot.

## 15. Recommendations for the next PoC

Validate against real UCIe IP: (1) post-commit response loss → UNKNOWN, (2) multi-flit retry occupancy under CRC bursts, (3) early-credit + retry interaction, (4) LinkError flush vs Retrain preserve policy, (5) whether vendor diagnostics expose enough state to reduce UNKNOWN without inventing a commit bit.

---

## Research questions

### Question 1 — Which previous findings survive?

UNKNOWN after architectural commit; Duplicate commit after blind retry; Pre-target UNKNOWN (observer A); ACK history reduces UNKNOWN (observer D); OBSERVATIONAL_EQUIVALENCE (A/B/C); Cross-client / cross-guest delay; Deadline miss under Client-A replay; transport delivery knowledge != architectural commit; CREDIT_CONSERVATION under early return + recovery

### Question 2 — Which were simulator assumptions?

Retry buffer peak

### Question 3 — Which internal UCIe states matter most?

Retry-buffer occupancy and ACK/NAK disposition before flush; FDI destructive vs Retrain; architectural commit vs response delivery

### Question 4 — Does error logging reduce fate uncertainty?

No for observer A (still UNKNOWN). Diagnostics record CRC/retry/recovery and buffer snapshots but do not bind to AXI completion or commit.

### Question 5 — What to validate on real UCIe IP next?

Validate against real UCIe IP: (1) post-commit response loss → UNKNOWN, (2) multi-flit retry occupancy under CRC bursts, (3) early-credit + retry interaction, (4) LinkError flush vs Retrain preserve policy, (5) whether vendor diagnostics expose enough state to reduce UNKNOWN without inventing a commit bit.

## Final summary

We configured the existing timed simulator to the provided UCIe D2D reference (FIFOs, retry buffer, early credits, streaming metadata, diagnostics, L1) and re-tested prior fate/recovery findings. Transaction-fate ambiguity after destructive recovery remains reachable and is therefore not an artifact of the old one-flit window. Cross-client interference magnitude and deadline misses are sensitive to the former single-inflight assumption and may disappear under realistic multi-outstanding buffering.
