# Review notes

Notes for a technical reader of the GitHub Pages site in `docs/`. The site is a presentation of the simulator output. It is not a new experiment.

Canonical reports:

- `simulation/results/findings.md`
- `simulation/results/findings_ucie3.md`
- `simulation/results/comparison_ucie2_vs_ucie3.md`

`docs/sources/` holds copies of those three files so the published site can link to them. `docs/data/findings.json` repeats the figures in a structured form. If the simulator is re-run, those copies need to be refreshed. The pages do not recompute rates.

## How a claim is classified

The public pages do not print these labels on every sentence. This list is the check used while revising them.

- SPECIFICATION FACT: a rule the reports cite from UCIe 2.0 or 3.0, or the publicly cited AoU §8.3.3 link-down behavior. The complete AoU 0.8 text was not in the workspace.
- SIMULATION RESULT: a count, trace, or reachability result copied from the generated reports.
- MODEL ASSUMPTION: device commit point, packing, credits, replay-count parameter, latched retry disposition, named policies A–E.
- INTERPRETATION: what those results mean for transaction-fate observability. Not a claim that UCIe is incorrect.
- AUTOMOTIVE IMPLICATION: a system-level decision the result forces into the open. Not an ISO 26262 metric and not a freedom-from-interference verdict.
- FUTURE RESEARCH QUESTION: including every UCIe 3D item. No 3D result is in the 2.0/3.0 counts.

## Claims made

- SIMULATION RESULT. Under the modeled observation profiles, there exist executions in which a committed and a non-committed non-idempotent transaction are observationally equivalent to the initiator after destructive link recovery.
- INTERPRETATION. UCIe link recovery alone does not determine the architectural fate of every outstanding higher-layer transaction. The study does not say UCIe promised AXI transaction recovery.
- SIMULATION RESULT. `UNKNOWN` is reachable for UCIe 2.0 and for UCIe 3.0 on the equivalent workload. UNKNOWN 171 and 171. Equality of those counts is this workload and this property only.
- INTERPRETATION. UNKNOWN is the sound label for the modeled observer when the facts prove neither completion nor non-execution. A system may hide that label with containment, reset, or an extra contract. Hiding it does not reconstruct the effect.
- SIMULATION RESULT. T17 (commit count 1, response lost) and T18 (commit count 0, request never accepted) match under FDI/AXI. On the UCIe 3.0 replay, profiles 1, 2, and 3 also match. The shared profile-3 signal string is quoted from `findings_ucie3.md`.
- SIMULATION RESULT. The same equivalence is reported for Emergency Shutdown, priority sideband plus mainband loss, and endpoint reset.
- INTERPRETATION. Emergency Shutdown is containment of a severe condition. The post-event question is what software can infer. The mechanism is not called defective for not preserving completion.
- SIMULATION RESULT. For this transaction-fate property, the tested UCIe 3.0 mechanisms did not eliminate the ambiguity, did not reduce the UNKNOWN count, and did not shorten the logical-event ambiguity window.
- INTERPRETATION. That equality is not “UCIe 3.0 is no better than UCIe 2.0.” Revision 3.0 still adds link, management, sideband, recalibration, and emergency-handling capabilities.
- SIMULATION RESULT. Policy D on the committed loss reaches `architectural_commit_count = 2`.
- SIMULATION RESULT. In this model, transport acknowledgement and architectural commit are distinct: 9 systematic UCIe 3.0 transactions had a transport ACK with commit count 0, and 33 had a commit without an initiator completion.
- SIMULATION RESULT and MODEL ASSUMPTION. Latched retry disposition changes UNKNOWN before target acceptance from 48.8% (21/43) to 11.6% (5/43). The latch is an assumption, not a standardized AXI fate mechanism. Later buckets are unchanged.
- SIMULATION RESULT and AUTOMOTIVE IMPLICATION. `ucie3_two_guests_shared_link` leaves both watched transactions UNKNOWN with commit count 1 and matching link-scoped signals. The page says this motivates a freedom-from-interference study. It does not state an ISO 26262 violation.
- SIMULATION RESULT. Fast Throttle, runtime recalibration, and runtime parity, in the runs that keep the flit, finish as COMPLETED with commit count 1.
- INTERPRETATION. Retrain retry-state handling is an integration ambiguity. The page does not call it a normative defect. The queued-response policy A/B case remains an integration choice, as the report says.
- The cross-layer sentence remains: the study does not claim that UCIe is incorrect. It identifies a cross-layer integration responsibility.

## Assumptions carried onto the site

- Policy A is the default reading of the printed counterexample. Policies B, C, D, and E are shown as named alternatives.
- FDI/AXI is the standardized initiator view. Latched retry disposition is marked as an assumption wherever its 11.6% pre-acceptance rate appears.
- The device commits a write only after request and write data have both arrived. Early write response stays off unless a scenario enables it.
- AoU behavior beyond the cited §8.3.3 link-down rule is an abstraction: one message per flit, one credit per message, credit depth 4 in the printed traces.
- The replay-count limit of 3 is a parameter. The CRC LFSR is not executed. Millisecond and flit thresholds are single events.
- UCIe 3.0 priority sideband is opcode-only. No transaction encoding was added.
- The pages do not report an ASIL, an ISO 26262 metric, or a vehicle-level test. “Level-4” was removed from the summary so it would not be read as one. The two-guest scenario is not presented as a freedom-from-interference violation.
- UCIe 3D appears only as an open question. It is not in the executed model.

## Metrics used

UCIe 2.0 baseline, all from `findings.md`:

- 289 simulations, 89 systematic, 200 randomized
- 288 fault injections, 33 transparent retries, 76 Retrain, 11 LinkReset, 158 LinkError
- FDI/AXI labels: 158 COMPLETED, 10 NOT_COMPLETED, 171 UNKNOWN
- 1 duplicate-commit violation, 1 false-safe-retry violation, 17 liveness failures, 10 credit invariant violations, 0 receiver overflows

Position table, randomized watched transaction, FDI/AXI: 48.8% (21/43), 52.1% (25/48), 69.4% (25/36), 54.5% (18/33), 0.0% (0/40). The latched-disposition table is included only on the results page, including 11.6% (5/43) before acceptance.

Equivalent-workload comparison: the same position rates, 171 and 171 UNKNOWN, 17 and 17 liveness failures, 204 and 204 recovery traces, and the ambiguity-window rows (137 / 78 closed and open, averages 3.8 and 5.4, window eliminated No).

Policy D committed loss: commit count 2, completion received, classification COMPLETED, violations 3. The site does not add these 3 violations to the baseline total of 1 duplicate-commit violation. They are reported as separate lines, matching the reports.

Counter values used: T17 changes the counter from 7 to 8. The duplicate-effect illustration says “7 → 8, then a second commit”. It does not print a measured counter value of 9. The reports do not print that third value.

## Wording kept conservative

- The site says UCIe 3.0 adds useful link, management, sideband, recalibration, and emergency-handling capabilities. It does not say those additions close transaction fate, and it does not say Revision 3.0 is no better than Revision 2.0.
- Headline on the summary: “Link recovery alone does not determine AXI transaction fate.” The older “does not guarantee AXI transaction recovery” wording was retired so the page would not suggest that UCIe promises that recovery.
- “Additional status” is qualified: the fields still match on both sides of every equivalence pair. The public pages no longer use the internal label “class F.” They say the modeled status does not identify the AXI transaction’s architectural commit state.
- Emergency Shutdown and Fast Throttle are described as optional, and Emergency Shutdown as fatal, following `findings_ucie3.md`. The UCIe 2.0 column says those mechanisms are not the ones added in the 3.0 text. It does not claim Revision 2.0 has no power management of any kind.
- Rates are labeled “observed under the selected seeded fault workload” and “not intrinsic protocol probabilities”.
- Liveness failures (17) and credit-invariant violations (10) stay in the tables. They are not in the summary’s lead cards. The site calls them simulator outcomes and does not attribute them to a normative UCIe defect. The model injects a credit fault. The report does not split the 10 hits by cause, so the site does not either.
- Avoided: “critical flaw”, “broken specification”, “catastrophic”, and “UCIe bug”.
- The required sentence is on the summary and the limitations page: the study does not claim that UCIe is incorrect; it identifies a cross-layer recovery gap.

## Presentation choices

- The counterexample steps on the summary page are a condensation of T17 and T18 in report order. The line-by-line trace stays in the markdown reports.
- Bar widths use the published percentages on a 0–100% scale. They are not renormalized to the largest bar.
- Diagrams are inline SVG so they follow the NUVUS light and dark tokens. The files in `docs/assets/` are the editable copies. Standalone SVG loads `tokens.css` from the same directory and therefore opens in the light theme.
- Colour, type, space, radius, and the wordmark come from the NUVUS brand. Fonts are the SIL Open Font License files shipped with that brand.
