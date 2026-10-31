AoU 0.8 over UCIe 3.0: transaction fate after link recovery

UCIe 3.0 improves fault detection, sideband scheduling, and power-emergency signaling, but does not solve the question of whether an interrupted safety-critical command already executed.

This report is generated from the simulator. Counts are not hand-written.
UCIe behavior is taken from UCIe Specification Revision 3.0, Version 1.0
(5 August 2025). A UCIe 2.0 rule is used only where the 3.0 text still
states it. Those retained rules are listed with the 3.0 mechanisms below.
The AoU 0.8 specification text is not in this workspace. AoU behavior
remains the publicly cited §8.3.3 link-down rule. Outstanding AXI fate
after cleanup is still a policy, because UCIe 3.0 §10.3.4 still says the
adapter and PHY do not guarantee quiescence of protocol transactions.

Percentages, where they appear, are observed under the selected fault
workload and seed 1. They are not protocol probabilities.

Specification classification of mechanisms

Class F would mean the mechanism tells the initiator the architectural
result. None of the modeled mechanisms is class F. The classifier
refuses a commit bit that the specification does not define.

| Mechanism | Where | Class | What the specification gives the initiator |
|---|---|---|---|
| CRC, 3-bit detection (§3.7) | retained | A. improves fault detection | A CRC mismatch still makes the flit invalid and still requires replay. Detection is not a record of whether the target committed. |
| Retry and replay (§3.8) | retained | D. preserves transport state | The Tx retry buffer still holds a flit until it is acknowledged. Selective NAK and the Rx retry buffer are still forbidden. Replay of an already accepted sequence is still not forwarded twice. This is transport state, not the target's architectural result. |
| Replay timeout and replay-count recommendation (§3.8) | retained | C. improves recovery timing | REPLAY_TIMEOUT_FLIT_COUNT >= 375 still schedules replay. FLIT_REPLAY_NUM still points at the PCIe <= 32 GT/s recommendation and still does not restate an integer. The model keeps that integer as a parameter. A timeout is a correctable internal error, not a commit bit. |
| Retry-variable reinitialization on Reset and Retrain (§3.8) | retained | G. does not affect AXI transaction fate | Variables that PCIe initializes in the IDLE handshake are still initialized on RDI Reset or Retrain. The 3.0 text still does not say that the payload bytes, or an AXI id, survive that step. |
| Sequence-number handshake (§3.8, §10.2.8) | retained | G. does not affect AXI transaction fate | Still required on every FDI Active entry. Timeout is still 128 flits, and that timeout still exits to Retrain. It restarts sequence numbers. It does not report outstanding AXI transactions. |
| Retrain (§10.3.3.4) | retained | C. improves recovery timing | CRC or parity errors crossing a threshold still cause Retrain, and the threshold algorithm is still implementation specific. Protocol layers still must not request Retrain on FDI unless the link is in Raw format. Retrain still does not flush by the LinkReset rule. |
| LinkReset and Disabled flush (§10.3.3.5, §10.3.3.6) | retained | G. does not affect AXI transaction fate | Best-effort drain still must not block entry. After the FDI state machine enters, pipelines and retry buffers are still flushed. |
| LinkError (§10.3.3.7) | retained | B. improves fault propagation | Stall is still not required. The upper layer must still clean up after sampling LinkError, and in-flight packets may still be dropped. Minimum residency is still 16 ms. The state is visible. The identity of a committed AXI transaction is not. |
| Sideband timeout to LinkError (§10.3.3.8) | retained | B. improves fault propagation | No response to a LinkError, LinkReset, or Disabled sideband handshake for 8 ms still moves RDI to LinkError. |
| No protocol quiescence (§10.3.4 implementation note) | retained | G. does not affect AXI transaction fate | The 3.0 note still says that, regardless of protocol, the adapter and PHY have no hardware mechanism that guarantees quiescence of transactions on LinkReset or Disabled. Streaming is called out in the same note. Outstanding AXI fate stays outside UCIe. |
| FDI and RDI state (§3.5, §10.2, §10.3.3) | retained | B. improves fault propagation | For a streaming protocol the Adapter LSM is still what FDI exposes on pl_state_sts. That is link state. |
| D2D error registers and link event notification (§9.5.1, §9.5.3.2, §9.5.3.5) | retained | A. improves fault detection | Correctable bits still cover CRC, retrain, and an internal error. Uncorrectable bits still cover adapter timeout, receiver overflow, internal error, sideband error messages, and a bad parameter exchange. Header Log 1 still logs a sideband-mailbox header, not an AXI header. No field is an AXI id. |
| Link Health Monitor (§9.5.3.40) | retained from 1.1 | A. improves fault detection | UHM status and eye margin describe the electrical link. They do not name a transaction. |
| Management transport (§2.5, §8.1) | retained from 2.0 | G. does not affect AXI transaction fate | §8.1 defines manageability as independent of UCIe mainband protocols. MTP carries management packets. It is not an AXI completion path. |
| Runtime link-testing parity (§3.9) | 3.0 text | A. improves fault detection | A parity mismatch is a correctable error. It does not, by itself, declare a flit invalid. Software can count it. It does not identify which MMIO write committed. |
| Runtime recalibration and TARR (§4.6) | 3.0 enhancement | A. improves fault detection | Mainband data must continue to be sampled during recalibration. The 8 ms TARR handshake timeout is explicitly not an error and does not take RDI to LinkError. |
| Sideband performant mode (§4.1.5.1) | 3.0 | C. improves recovery timing | Removes the 32 UI sideband gap after negotiation. This model counts logical protocol events, not UI. A shorter sideband gap is not a completion. |
| Priority sideband packets (§4.1.5.2) | 3.0 | C. improves recovery timing | Optional. A priority packet may interrupt normal sideband traffic. The opcode distinguishes a chain from its end. The implementation note says further payload details are left to a future ECN. The model therefore records opcode-only and does not invent a transaction encoding. |
| Fast Throttle (§8.4.2.1) | 3.0 | G. does not affect AXI transaction fate | Optional power or thermal indication. Disabled at power-on. Logging is trigger and response history, not mainband transactions. The specification does not say that Fast Throttle drops a flit. |
| Emergency Shutdown (§8.4.2.2) | 3.0 | B. improves fault propagation | Optional and fatal. It can be left only by reset. A chiplet may take a configured power action. The specification does not define a log of outstanding mainband transactions, so the model does not add one. Any local drain would be an implementation policy. |
| Early Firmware Download (§8.4.1) | 3.0 | G. does not affect AXI transaction fate | Firmware download state. Not an AXI completion path. |
| 48 GT/s and 64 GT/s (revision history, §3.8) | 3.0 | G. does not affect AXI transaction fate | Higher rates do not add an end-to-end transaction acknowledgement. §3.8 still recommends the <= 32 GT/s replay-count rules. |
| Continuous NOP insertion (§3.3, §3.8) | 3.0 | G. does not affect AXI transaction fate | The adapter may keep sending NOPs instead of ending a burst with PDS. That is a power and latency choice. It does not report commit. |

Replayed UCIe 2.0 counterexample on the UCIe 3.0 model

The same non-idempotent MMIO increment is committed remotely, the
response is then lost, and the link returns to Active. The 3.0 status
latched on that path is link, CRC, retrain, and management-transport
state. It does not name the transaction.

UCIe version: 3.0

Transaction:
    ID: T17
    Type: MMIO_WRITE
    Operation: INCREMENT_COUNTER
    Address: 0x1000
    Idempotent: no

Trace:
    1. AXI request accepted
    2. AoU request packed
    3. AoU request sent
    4. Remote endpoint receives write request
    5. AXI write data accepted locally
    6. AoU write data packed
    7. AoU write data sent
    8. Remote endpoint receives write data
    9. Target accepts write
    10. Target commits operation
    11. Counter changes from 7 to 8
    12. Write response generated
    13. AoU response packed
    14. Write response transmitted
    15. Repeated corruption armed against replay of this flit
    16. UCIe CRC failure on the response flit (1-bit corruption); replay requested
    17. Replay 2 of the response flit fails CRC
    18. UCIe persistent transmission errors occur (3 CRC failures)
    19. Retry threshold exceeded
    20. UCIe enters Retrain
    21. AoU remains enabled across Retrain (FDI link-down is not signaled)
    22. Repeated errors during recovery
    23. Recovery fails
    24. LinkError entered
    25. Retrain did not restore the link
    26. LinkError propagated toward the remote link partner
    27. LinkError minimum residency (16 ms, UCIe 3.0 §10.3.3.7) elapses so a partner that missed the sideband message also enters LinkError
    28. UCIe 3.0 software-visible status latched (§9.5 correctable CRC/retrain bits, uncorrectable LinkError status, link event notification). No field carries an AXI identifier
    29. UCIe 3.0 management transport (§8.1) is independent of the mainband protocol and has no architectural-commit record for this transaction
    30. Link Health Monitor (§9.5.3.40) exposes link margin, not the outstanding AXI transaction
    31. Retry buffers and pipelines flushed
    32. AoU activation FSM returns to DISABLED
    33. Response is discarded during cleanup
    34. T17: discarded with no AXI completion
    35. Link is reset and reactivated
    36. UCIe link reaches Active
    37. Sequence-number handshake completes (timeout bound 128 flits, UCIe 3.0 §3.8)
    38. AoU activation handshake completes
    39. AoU credits initialized to depth 4 in each direction
    40. T17: initiator has no completion record

Ground truth:
    architectural_commit_count = 1
    target_accepted = true
    response_generated = true

Initiator observation:
    completion_received = false
    axi_accepted = true
    UCIe_state = recovered
    linkerror_status = true
    crc_error_status = true
    retrain_status = true
    emergency_shutdown = false
    fast_throttle = false
    priority_sideband_opcode_only = false
    runtime_parity = false
    runtime_recalibration = false
    management_reports_axi_commit = false

Classification:
    profile 1 AXI+FDI: UNKNOWN
    profile 2 AXI+FDI+software error state: UNKNOWN
    profile 3 AXI+standardized management: UNKNOWN

Safe automatic retry:
    NO

Ambiguity window (logical events from architectural commit):
    open = true
    length = 9

Matching non-commit trace

UCIe version: 3.0

Transaction:
    ID: T18
    Type: MMIO_WRITE
    Operation: INCREMENT_COUNTER
    Address: 0x1000
    Idempotent: no

Trace:
    1. AXI request accepted
    2. AoU request packed
    3. AoU request sent
    4. Repeated corruption armed against replay of this flit
    5. UCIe CRC failure on the request flit (1-bit corruption); replay requested
    6. Replay 2 of the request flit fails CRC
    7. UCIe persistent transmission errors occur (3 CRC failures)
    8. Retry threshold exceeded
    9. UCIe enters Retrain
    10. AoU remains enabled across Retrain (FDI link-down is not signaled)
    11. Repeated errors during recovery
    12. Recovery fails
    13. LinkError entered
    14. Retrain did not restore the link
    15. LinkError propagated toward the remote link partner
    16. LinkError minimum residency (16 ms, UCIe 3.0 §10.3.3.7) elapses so a partner that missed the sideband message also enters LinkError
    17. UCIe 3.0 software-visible status latched (§9.5 correctable CRC/retrain bits, uncorrectable LinkError status, link event notification). No field carries an AXI identifier
    18. UCIe 3.0 management transport (§8.1) is independent of the mainband protocol and has no architectural-commit record for this transaction
    19. Link Health Monitor (§9.5.3.40) exposes link margin, not the outstanding AXI transaction
    20. Retry buffers and pipelines flushed
    21. AoU activation FSM returns to DISABLED
    22. Unacknowledged request is discarded during cleanup
    23. T18: discarded with no AXI completion
    24. Link is reset and reactivated
    25. UCIe link reaches Active
    26. Sequence-number handshake completes (timeout bound 128 flits, UCIe 3.0 §3.8)
    27. AoU activation handshake completes
    28. AoU credits initialized to depth 4 in each direction
    29. T18: initiator has no completion record

Ground truth:
    architectural_commit_count = 0
    target_accepted = false
    response_generated = false

Initiator observation:
    completion_received = false
    axi_accepted = true
    UCIe_state = recovered
    linkerror_status = true
    crc_error_status = true
    retrain_status = true
    emergency_shutdown = false
    fast_throttle = false
    priority_sideband_opcode_only = false
    runtime_parity = false
    runtime_recalibration = false
    management_reports_axi_commit = false

Classification:
    profile 1 AXI+FDI: UNKNOWN
    profile 2 AXI+FDI+software error state: UNKNOWN
    profile 3 AXI+standardized management: UNKNOWN

Safe automatic retry:
    NO

Ambiguity window (logical events from architectural commit):
    open = false
    length = n/a

Pair: equivalent LinkError loss

Committed trace ground truth: architectural_commit_count = 1
Uncommitted trace ground truth: architectural_commit_count = 0

Profile fdi_axi: initiator views match
Profile software_error: initiator views match
Profile management: initiator views match

OBSERVATIONAL EQUIVALENCE FOUND

different ground truth
+
same standardized initiator-visible information
=
transaction-fate ambiguity

Committed initiator view, profile 3:
    axi_accepted=true, completion_received=false, signals=linkerror, crc_error, retrain, uncorrectable, remote_linkerror_sideband, axi_commit_bit=false
Uncommitted initiator view, profile 3:
    axi_accepted=true, completion_received=false, signals=linkerror, crc_error, retrain, uncorrectable, remote_linkerror_sideband, axi_commit_bit=false

UCIe 3.0-only fault pairs

Pair: Emergency Shutdown

Committed trace ground truth: architectural_commit_count = 1
Uncommitted trace ground truth: architectural_commit_count = 0

Profile fdi_axi: initiator views match
Profile software_error: initiator views match
Profile management: initiator views match

OBSERVATIONAL EQUIVALENCE FOUND

different ground truth
+
same standardized initiator-visible information
=
transaction-fate ambiguity

Committed initiator view, profile 3:
    axi_accepted=true, completion_received=false, signals=emergency_shutdown, endpoint_reset, axi_commit_bit=false
Uncommitted initiator view, profile 3:
    axi_accepted=true, completion_received=false, signals=emergency_shutdown, endpoint_reset, axi_commit_bit=false

Pair: priority sideband plus mainband loss

Committed trace ground truth: architectural_commit_count = 1
Uncommitted trace ground truth: architectural_commit_count = 0

Profile fdi_axi: initiator views match
Profile software_error: initiator views match
Profile management: initiator views match

OBSERVATIONAL EQUIVALENCE FOUND

different ground truth
+
same standardized initiator-visible information
=
transaction-fate ambiguity

Committed initiator view, profile 3:
    axi_accepted=true, completion_received=false, signals=linkerror, crc_error, retrain, uncorrectable, remote_linkerror_sideband, priority_sideband_opcode_only, axi_commit_bit=false
Uncommitted initiator view, profile 3:
    axi_accepted=true, completion_received=false, signals=linkerror, crc_error, retrain, uncorrectable, remote_linkerror_sideband, priority_sideband_opcode_only, axi_commit_bit=false

Pair: endpoint reset

Committed trace ground truth: architectural_commit_count = 1
Uncommitted trace ground truth: architectural_commit_count = 0

Profile fdi_axi: initiator views match
Profile software_error: initiator views match
Profile management: initiator views match

OBSERVATIONAL EQUIVALENCE FOUND

different ground truth
+
same standardized initiator-visible information
=
transaction-fate ambiguity

Committed initiator view, profile 3:
    axi_accepted=true, completion_received=false, signals=linkerror, uncorrectable, remote_linkerror_sideband, endpoint_reset, axi_commit_bit=false
Uncommitted initiator view, profile 3:
    axi_accepted=true, completion_received=false, signals=linkerror, uncorrectable, remote_linkerror_sideband, endpoint_reset, axi_commit_bit=false

Mechanisms that do not drop the mainband transaction

Fast Throttle (§8.4.2.1), runtime recalibration (§4.6), and runtime
link-testing parity (§3.9) are specified not to discard a valid flit
by themselves. The runs record the indication and let the write finish.

ucie3_fast_throttle_during_response: classification COMPLETED, architectural_commit_count = 1, completion_received = true
ucie3_recalibration_during_response: classification COMPLETED, architectural_commit_count = 1, completion_received = true
ucie3_parity_during_response: classification COMPLETED, architectural_commit_count = 1, completion_received = true

Automatic retry of the committed UCIe 3.0 loss

architectural_commit_count = 2
completion_received = true
classification = COMPLETED
violations = 3

Ambiguity window on the equivalent workload

The window is counted in logical protocol events, from architectural
commit to either the AXI completion or the return to Active without
one. UCIe 3.0 notification lines are not counted, so a longer trace
printout is not treated as a longer window. Sideband performant mode
and priority sideband change UI scheduling. This model does not turn
that UI gap into a fate event, and §4.1.5.2 does not define a
completion inside a priority packet.

| Window metric | UCIe 2.0 | UCIe 3.0 |
|---|---:|---:|
| Closed windows (completion arrived) | 137 | 137 |
| Open windows (fate unresolved) | 78 | 78 |
| Minimum closed window | 3 | 3 |
| Maximum closed window | 7 | 7 |
| Average closed window | 3.8 | 3.8 |
| Minimum open window | 2 | 2 |
| Maximum open window | 9 | 9 |
| Average open window | 5.4 | 5.4 |
| Window eliminated | No | No |

Conclusion

Systematic fault-injection analysis shows that the transaction-fate ambiguity previously identified under UCIe 2.0 remains reachable under UCIe 3.0. Although UCIe 3.0 provides enhanced error signaling and recovery mechanisms, these mechanisms describe link and transport state rather than the architectural commit state of an outstanding AXI transaction. Two executions with different architectural outcomes can therefore remain observationally equivalent to the initiator after recovery.

