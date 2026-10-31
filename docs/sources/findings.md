AoU 0.8 over UCIe 2.0: transaction fate after link recovery

This report is generated from the simulator. Counts are not hand-written.
The AoU 0.8 specification text is not in this workspace. AoU behavior is
limited to the publicly cited §8.3.3 link-down rule (activation returns
to DISABLED; Retrain is not that event) plus an explicit credit and
packing abstraction. Outstanding-transaction fate is a policy.
UCIe rules that are implemented are cited in simulation/model/ucie.py.

Case: response lost after architectural commit

Transaction T17: MMIO_WRITE
Address: 0x1000
Operation: INCREMENT_COUNTER
Idempotent: no
Policy: A (DISCARD)
Observation used for the initiator label: fdi_axi

Step 1: AXI request accepted
Step 2: AoU request packed
Step 3: AoU request sent
Step 4: Remote endpoint receives write request
Step 5: AXI write data accepted locally
Step 6: AoU write data packed
Step 7: AoU write data sent
Step 8: Remote endpoint receives write data
Step 9: Target accepts write
Step 10: Target commits operation
Step 11: Counter changes from 7 to 8
Step 12: Write response generated
Step 13: AoU response packed
Step 14: Write response transmitted
Step 15: Repeated corruption armed against replay of this flit
Step 16: UCIe CRC failure on the response flit (1-bit corruption); replay requested
Step 17: Replay 2 of the response flit fails CRC
Step 18: UCIe persistent transmission errors occur (3 CRC failures)
Step 19: Retry threshold exceeded
Step 20: UCIe enters Retrain
Step 21: AoU remains enabled across Retrain (FDI link-down is not signaled)
Step 22: Repeated errors during recovery
Step 23: Recovery fails
Step 24: LinkError entered
Step 25: Retrain did not restore the link
Step 26: LinkError propagated toward the remote link partner
Step 27: LinkError minimum residency (16 ms, UCIe 2.0 §10.3.3.7) elapses so a partner that missed the sideband message also enters LinkError
Step 28: Retry buffers and pipelines flushed
Step 29: AoU activation FSM returns to DISABLED
Step 30: Response is discarded during cleanup
Step 31: T17: discarded with no AXI completion
Step 32: Link is reset and reactivated
Step 33: UCIe link reaches Active
Step 34: Sequence-number handshake completes (timeout bound 128 flits, UCIe 2.0 §3.8)
Step 35: AoU activation handshake completes
Step 36: AoU credits initialized to depth 4 in each direction
Step 37: T17: initiator has no completion record

Ground truth:
    architectural_commit_count = 1
    target_accepted = true
    response_generated = true

Initiator observation:
    completion_received = false
    axi_accepted = true
    request_acknowledged = true
    write_data_acknowledged = true
    request_nak_only = false
    write_data_transmitted = true

Classification:
    UNKNOWN

Classification if only FDI state and AXI completion are visible:
    UNKNOWN

Classification if retry disposition was latched before flush (assumption):
    UNKNOWN

Safe automatic retry:
    NO

Safe automatic retry from FDI and AXI alone:
    NO

Invariant results:
    none fired

Case: fault before target acceptance

Transaction T18: MMIO_WRITE
Address: 0x1000
Operation: INCREMENT_COUNTER
Idempotent: no
Policy: A (DISCARD)
Observation used for the initiator label: retry_disposition

Step 1: AXI request accepted
Step 2: AoU request packed
Step 3: AoU request sent
Step 4: Repeated corruption armed against replay of this flit
Step 5: UCIe CRC failure on the request flit (1-bit corruption); replay requested
Step 6: Replay 2 of the request flit fails CRC
Step 7: UCIe persistent transmission errors occur (3 CRC failures)
Step 8: Retry threshold exceeded
Step 9: UCIe enters Retrain
Step 10: AoU remains enabled across Retrain (FDI link-down is not signaled)
Step 11: Repeated errors during recovery
Step 12: Recovery fails
Step 13: LinkError entered
Step 14: Retrain did not restore the link
Step 15: LinkError propagated toward the remote link partner
Step 16: LinkError minimum residency (16 ms, UCIe 2.0 §10.3.3.7) elapses so a partner that missed the sideband message also enters LinkError
Step 17: Retry buffers and pipelines flushed
Step 18: AoU activation FSM returns to DISABLED
Step 19: Unacknowledged request is discarded during cleanup
Step 20: T18: discarded with no AXI completion
Step 21: Link is reset and reactivated
Step 22: UCIe link reaches Active
Step 23: Sequence-number handshake completes (timeout bound 128 flits, UCIe 2.0 §3.8)
Step 24: AoU activation handshake completes
Step 25: AoU credits initialized to depth 4 in each direction
Step 26: T18: initiator has no completion record

Ground truth:
    architectural_commit_count = 0
    target_accepted = false
    response_generated = false

Initiator observation:
    completion_received = false
    axi_accepted = true
    request_acknowledged = false
    write_data_acknowledged = false
    request_nak_only = true
    write_data_transmitted = false

Classification:
    NOT_COMPLETED

Classification if only FDI state and AXI completion are visible:
    UNKNOWN

Classification if retry disposition was latched before flush (assumption):
    NOT_COMPLETED

Safe automatic retry:
    YES

Safe automatic retry from FDI and AXI alone:
    NO

Invariant results:
    none fired

Same pre-acceptance trace, classified from FDI state and AXI completion only

Transaction T18: MMIO_WRITE
Address: 0x1000
Operation: INCREMENT_COUNTER
Idempotent: no
Policy: A (DISCARD)
Observation used for the initiator label: fdi_axi

Step 1: AXI request accepted
Step 2: AoU request packed
Step 3: AoU request sent
Step 4: Repeated corruption armed against replay of this flit
Step 5: UCIe CRC failure on the request flit (1-bit corruption); replay requested
Step 6: Replay 2 of the request flit fails CRC
Step 7: UCIe persistent transmission errors occur (3 CRC failures)
Step 8: Retry threshold exceeded
Step 9: UCIe enters Retrain
Step 10: AoU remains enabled across Retrain (FDI link-down is not signaled)
Step 11: Repeated errors during recovery
Step 12: Recovery fails
Step 13: LinkError entered
Step 14: Retrain did not restore the link
Step 15: LinkError propagated toward the remote link partner
Step 16: LinkError minimum residency (16 ms, UCIe 2.0 §10.3.3.7) elapses so a partner that missed the sideband message also enters LinkError
Step 17: Retry buffers and pipelines flushed
Step 18: AoU activation FSM returns to DISABLED
Step 19: Unacknowledged request is discarded during cleanup
Step 20: T18: discarded with no AXI completion
Step 21: Link is reset and reactivated
Step 22: UCIe link reaches Active
Step 23: Sequence-number handshake completes (timeout bound 128 flits, UCIe 2.0 §3.8)
Step 24: AoU activation handshake completes
Step 25: AoU credits initialized to depth 4 in each direction
Step 26: T18: initiator has no completion record

Ground truth:
    architectural_commit_count = 0
    target_accepted = false
    response_generated = false

Initiator observation:
    completion_received = false
    axi_accepted = true
    request_acknowledged = false
    write_data_acknowledged = false
    request_nak_only = true
    write_data_transmitted = false

Classification:
    UNKNOWN

Classification if only FDI state and AXI completion are visible:
    UNKNOWN

Classification if retry disposition was latched before flush (assumption):
    NOT_COMPLETED

Safe automatic retry:
    NO

Safe automatic retry from FDI and AXI alone:
    NO

Invariant results:
    none fired

Comparison

The two traces are not the same execution. In the first, the target commits (architectural_commit_count = 1) and the write response is then lost. In the second, every copy of the request fails CRC before the target accepts it (architectural_commit_count = 0).

What the initiating AXI manager can see, once FDI has gone through LinkError and come back, is the same in both traces: the request was accepted, and no B response arrived. FDI/AXI classification of the committed trace: UNKNOWN. FDI/AXI classification of the uncommitted trace: UNKNOWN.

Those two initiator views match. UCIe reports link state on FDI (pl_state_sts) and AoU 0.8 §8.3.3, as cited, returns the activation FSM to DISABLED on that link-down. Neither interface identifies which outstanding AXI transaction had reached architectural commit. The initiator therefore cannot tell the committed increment from the increment that never ran.

If the initiator's adapter latches retry disposition before the LinkError flush, the two traces separate. The uncommitted request was only NAK'd (UCIe 2.0 §3.7 does not deliver a CRC-invalid flit) and is classified NOT_COMPLETED. The committed request was acknowledged and its response was lost, so it stays UNKNOWN. That latch is an assumption. UCIe 2.0 flushes retry buffers on this path and does not require the NAK/ACK history to be kept, tied to an AXI id, and shown to software. AoU 0.8, in the text available for this model, does not require it either.

A label of NOT_COMPLETED is safe for the second trace because the commit count is 0. Applying that same label to the first trace would be a false safe retry: the commit count there is 1.

Summary

total simulations: 289
    systematic: 89
    randomized (seed 1): 200
total fault injections: 288
successful transparent retries: 33
Retrain events: 76
LinkReset events: 11
LinkError events: 158
FDI/AXI classification counts (systematic + randomized transactions):
    COMPLETED: 158
    NOT_COMPLETED: 10
    UNKNOWN: 171
duplicate-commit violations: 1
false-safe-retry violations: 1
deadlocks or liveness failures: 17
credit invariant violations: 10
receiver-overflow violations: 0

Randomized simulation, initiator class by fault position
(watched transaction only; n is the number of injected faults in that bucket).
A non-zero COMPLETED count means replay or a non-destructive fault still
delivered the response. UNKNOWN rate uses UNKNOWN/n from this table.

Fault position                    COMPLETED  NOT_COMPLETED  UNKNOWN     n  UNKNOWN rate
--------------------------------------------------------------------------------------
Before target acceptance                 17              5       21    43         48.8%
After target acceptance                  23              0       25    48         52.1%
After architectural commit               11              0       25    36         69.4%
After response generation                15              0       18    33         54.5%
After response delivery                  40              0        0    40          0.0%

Same runs, UNKNOWN rate if retry disposition is latched (assumption)

Fault position                    COMPLETED  NOT_COMPLETED  UNKNOWN     n  UNKNOWN rate
--------------------------------------------------------------------------------------
Before target acceptance                 17             21        5    43         11.6%
After target acceptance                  23              0       25    48         52.1%
After architectural commit               11              0       25    36         69.4%
After response generation                15              0       18    33         54.5%
After response delivery                  40              0        0    40          0.0%

Policy and interpretation differences

Policy A (DISCARD), FDI/AXI observation, on the post-commit loss: initiator UNKNOWN, commit 1, completion_received false, explicit_unknown false, violations 0
Policy A (DISCARD), FDI/AXI observation, on the pre-acceptance loss: initiator UNKNOWN, commit 0, violations 0
Policy B (PRESERVE_LOCAL), FDI/AXI observation, on the post-commit loss: initiator UNKNOWN, commit 1, completion_received false, explicit_unknown false, violations 0
Policy B (PRESERVE_LOCAL), FDI/AXI observation, on the pre-acceptance loss: initiator UNKNOWN, commit 0, violations 0
Policy C (EXPLICIT_UNKNOWN), FDI/AXI observation, on the post-commit loss: initiator UNKNOWN, commit 1, completion_received false, explicit_unknown true, violations 0  [differs from policy A]
Policy C (EXPLICIT_UNKNOWN), FDI/AXI observation, on the pre-acceptance loss: initiator UNKNOWN, commit 0, violations 0
Policy D (AUTO_RETRY), FDI/AXI observation, on the post-commit loss: initiator COMPLETED, commit 2, completion_received true, explicit_unknown false, violations 3  [differs from policy A]
Policy D (AUTO_RETRY), FDI/AXI observation, on the pre-acceptance loss: initiator COMPLETED, commit 1, violations 0
Policy E (NO_AUTO_RETRY), FDI/AXI observation, on the post-commit loss: initiator UNKNOWN, commit 1, completion_received false, explicit_unknown false, violations 0
Policy E (NO_AUTO_RETRY), FDI/AXI observation, on the pre-acceptance loss: initiator UNKNOWN, commit 0, violations 0
Retrain while the response is still unacknowledged: preserve-retry ends as COMPLETED commit 1 deadlock false; reinitialize-retry ends as UNKNOWN commit 1 deadlock true.
These two outcomes are an integration ambiguity: UCIe 2.0 §3.8 reinitializes the IDLE-handshake retry variables on Retrain, and §10.3.3.5 states a flush for LinkReset, not for Retrain.
Write response sitting in the initiating AoU, not yet accepted on AXI, then LinkError: policy A initiator UNKNOWN commit 1; policy B initiator COMPLETED commit 1.
Policy A and policy B disagree here. UCIe does not say whether a response already queued above the adapter survives LinkError cleanup. That is an integration choice, not a detected coding bug.

Conclusion

Systematic fault injection identified legal execution traces in which a remote AXI operation reached its architectural commit point before UCIe entered LinkError, while the corresponding completion response was lost during error recovery. In these traces, the initiating endpoint could not distinguish a committed operation from an operation that had not executed. The modeled AoU 0.8/UCIe 2.0 stack therefore requires an explicit UNKNOWN transaction state or an additional end-to-end recovery mechanism for safe handling of non-idempotent transactions.

Automatic retry after UCIe link recovery can result in duplicate architectural effects when a transaction commits remotely before its completion response is lost. Exactly-once semantics therefore cannot be assumed without additional transaction sequencing, persistent commit tracking or duplicate suppression.

