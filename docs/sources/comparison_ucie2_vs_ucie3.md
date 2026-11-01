UCIe 2.0 versus UCIe 3.0: AXI transaction fate

UCIe 3.0 improves fault detection, sideband scheduling, and power-emergency signaling, but does not solve the question of whether an interrupted safety-critical command already executed.

This comparison replays one systematic suite and one seeded random
suite (seed 1, 200 injections) on both protocol
models. A third set exercises mechanisms that UCIe 3.0 adds and UCIe 2.0
does not define. Percentages are observed under that workload and seed.
They are not intrinsic protocol probabilities. The reachability rows
are the stronger result.

Equivalent-workload results

| Metric | UCIe 2.0 | UCIe 3.0 |
|---|---:|---:|
| UNKNOWN reachable | Yes | Yes |
| Duplicate commit reachable | Yes | Yes |
| False-safe-retry reachable | Yes | Yes |
| Liveness failures | 17 | 17 |
| Recovery traces explored | 204 | 204 |
| UNKNOWN before target acceptance | 48.8% (21/43) | 48.8% (21/43) |
| UNKNOWN after target acceptance | 52.1% (25/48) | 52.1% (25/48) |
| UNKNOWN after architectural commit | 69.4% (25/36) | 69.4% (25/36) |
| UNKNOWN after response generation | 54.5% (18/33) | 54.5% (18/33) |
| UNKNOWN after response delivery | 0.0% (0/40) | 0.0% (0/40) |

Equivalent-workload UNKNOWN transactions: UCIe 2.0 171, UCIe 3.0 171.
Equivalent-workload simulations: UCIe 2.0 289 (89 systematic + 200 random), UCIe 3.0 289 (89 systematic + 200 random).

Answers

1. UNKNOWN transaction fate is reachable in UCIe 2.0: Yes.
2. UNKNOWN transaction fate is reachable in UCIe 3.0: Yes. The equivalent workload already reaches it. The 3.0-only destructive
 faults do not remove it.
3. Fault scenarios that create UNKNOWN are the runs whose watched
 transaction ends without an AXI completion after the initiator had
 accepted it, and where standardized status does not prove the effect
 was absent. On the equivalent workload the UCIe 3.0 model reproduces
 the UCIe 2.0 set: 0 scenario names leave that set and
 0 names enter it.
4. UCIe 3.0 eliminates a UCIe 2.0 ambiguity class on this workload: No.
5. UCIe 3.0 does make additional standardized status visible: software
 error registers, Link Health Monitor, management-transport independence,
 and, on the 3.0-only faults, Fast Throttle, Emergency Shutdown, a
 priority-sideband opcode, runtime parity, and runtime recalibration.
 In every equivalence pair those fields are the same on the committed
 trace and the uncommitted trace. Visibility is not commit knowledge.
6. The logical-event ambiguity window on the equivalent workload is
 not shorter under UCIe 3.0.
 Sideband performant mode removes a 32 UI sideband gap. That gap is
 not a fate milestone in this model, and the specification does not
 define it as one.
7. UCIe 3.0 reduces the number of UNKNOWN transactions on the
 equivalent workload: No (171 and 171).
8. Automatic retry is safe in a new UCIe 3.0 scenario: No. The committed loss, retried under policy D, still produces a second
 architectural effect (architectural_commit_count = 2). Fast Throttle, parity, and recalibration complete without a retry,
 because the specification does not drop the flit. They are not new
 cases in which a lost completion becomes safe to replay.
9. Transport acknowledgement is still not architectural commit.
 On the UCIe 3.0 systematic runs, 9 transactions
 had a transport ACK with architectural_commit_count 0, and
 33 transactions had a commit without an
 initiator completion.
10. The additional software-visible and management-visible signals
 listed in answer 5 are available. They improve classification of the
 link. They do not change the transaction label. Profile 1, profile 2,
 and profile 3 agree on every equivalence pair.

Equivalence pairs

A pair is an equivalence when the commit counts differ and all three
 observation profiles still match.

- equivalent LinkError loss
- Emergency Shutdown
- priority sideband plus mainband loss
- endpoint reset

UCIe 3.0-only scenarios whose watched transaction is UNKNOWN

- ucie3_emergency_after_commit
- ucie3_emergency_before_acceptance
- ucie3_pspt_after_commit
- ucie3_pspt_before_acceptance
- ucie3_endpoint_reset_after_commit
- ucie3_endpoint_reset_before_acceptance
- ucie3_two_guests_shared_link

Ambiguity window

Logical events, equivalent workload, watched transaction, counted
 only after architectural commit. Observed under the selected fault
 workload and seed 1.

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

The window is eliminated only when no open window remains. An open
 window means the initiator returned to an operational link without
 a completion, and without a standardized commit bit.

Automotive interpretation

Link recovery and transaction recovery are different events. The
 question for a virtual ECU is what it knows about the last
 safety-relevant command after the UCIe link is healthy again.

1. A virtual ECU still needs an UNKNOWN transaction state under
 UCIe 3.0: Yes. Profile 3, which includes every
 standardized management fact this model is allowed to read, still
 returns UNKNOWN for a committed loss and for a loss that never
 reached the target.
2. Automatic replay of non-idempotent MMIO is not safe. The UCIe 3.0
 replay of the committed loss duplicates the architectural effect
 (architectural_commit_count = 2).
3. UCIe 3.0 error signaling cannot be mapped directly to a
 guest-visible transaction failure. The same CRC, retrain, and
 LinkError status appears for a command that committed and for a
 command that did not.
4. A LinkError requires the guest's outstanding transaction state
 to be invalidated as a known success. It does not justify recording
 the command as known not to have executed. The sound state is
 UNKNOWN unless a completion was already delivered.
5. Several guests sharing one endpoint are not isolated, for the
 purpose of retrospective fate, by a physical link fault. UCIe status
 is link-scoped. In the two-initiator run `ucie3_two_guests_shared_link`, the profile-3 views are identical.
   - T70 axi_id 1: architectural_commit_count = 1, classification = UNKNOWN, axi_accepted=true, completion_received=false, signals=linkerror, uncorrectable, remote_linkerror_sideband, axi_commit_bit=false
   - T71 axi_id 2: architectural_commit_count = 1, classification = UNKNOWN, axi_accepted=true, completion_received=false, signals=linkerror, uncorrectable, remote_linkerror_sideband, axi_commit_bit=false
6. The virtual platform cannot determine which guest transactions
 may already have committed. No modeled register carries an AXI id
 or a commit bit, and the equivalence pairs have different commit
 counts under the same status.
7. UCIe 3.0 does not materially shorten the logical-event interval
 during which transaction fate is ambiguous. It adds faster sideband
 scheduling and a fatal Emergency Shutdown path. Those change how
 quickly the platform learns that the link or the power state is
 bad. They do not say whether the last command took effect.
 After the UCIe link is healthy again, the virtual ECU still does
 not know what happened to the last safety-relevant command.

Conclusion

Systematic fault-injection analysis shows that the transaction-fate ambiguity previously identified under UCIe 2.0 remains reachable under UCIe 3.0. Although UCIe 3.0 provides enhanced error signaling and recovery mechanisms, these mechanisms describe link and transport state rather than the architectural commit state of an outstanding AXI transaction. Two executions with different architectural outcomes can therefore remain observationally equivalent to the initiator after recovery.
