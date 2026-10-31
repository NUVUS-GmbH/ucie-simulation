# Review notes

Notes for a technical reader of the GitHub Pages site in `docs/`. The site is a presentation of the simulator output. It is not a new experiment.

Canonical reports:

- `simulation/results/findings.md`
- `simulation/results/findings_ucie3.md`
- `simulation/results/comparison_ucie2_vs_ucie3.md`

`docs/sources/` holds copies of those three files so the published site can link to them. `docs/data/findings.json` repeats the figures in a structured form. If the simulator is re-run, those copies need to be refreshed. The pages do not recompute rates.

## Claims made

- After link recovery, the modeled AXI initiator cannot always tell whether an outstanding non-idempotent transaction committed.
- Link recovery and transaction recovery are different events. A recovered link can coexist with `completion_received = false`.
- `UNKNOWN` is reachable for UCIe 2.0 and for UCIe 3.0 on the equivalent workload.
- T17 (commit count 1, response lost) and T18 (commit count 0, request never accepted) are observationally equivalent under FDI/AXI. On the UCIe 3.0 replay, profiles 1, 2, and 3 also match. The shared profile-3 signal string is quoted from `findings_ucie3.md`.
- The same equivalence is reported for Emergency Shutdown, priority sideband plus mainband loss, and endpoint reset.
- UCIe 3.0 does not eliminate a UCIe 2.0 ambiguity class on this workload, does not reduce the UNKNOWN count (171 and 171), and does not shorten the logical-event ambiguity window.
- Blind retry is not always safe. Policy D on the committed loss reaches `architectural_commit_count = 2`.
- Transport acknowledgement is not architectural commit: 9 systematic UCIe 3.0 transactions had a transport ACK with commit count 0, and 33 had a commit without an initiator completion.
- Two guests on one link (`ucie3_two_guests_shared_link`) are not separated, for retrospective fate, by link-scoped UCIe status. Both watched transactions are UNKNOWN with commit count 1.
- Fast Throttle, runtime recalibration, and runtime parity, in the runs that keep the flit, finish as COMPLETED with commit count 1. They are not presented as cases where a lost completion becomes safe to replay.
- The cross-layer sentence in the specification section is an interpretation of those results, not a claim that the UCIe text contains a coding error. The reports themselves call the policy A/B response-queue case an integration choice, not a detected coding bug.

## Assumptions carried onto the site

- Policy A is the default reading of the printed counterexample. Policies B, C, D, and E are shown as named alternatives.
- FDI/AXI is the standardized initiator view. Latched retry disposition is marked as an assumption wherever its 11.6% pre-acceptance rate appears.
- The device commits a write only after request and write data have both arrived. Early write response stays off unless a scenario enables it.
- AoU behavior beyond the cited §8.3.3 link-down rule is an abstraction: one message per flit, one credit per message, credit depth 4 in the printed traces.
- The replay-count limit of 3 is a parameter. The CRC LFSR is not executed. Millisecond and flit thresholds are single events.
- UCIe 3.0 priority sideband is opcode-only. No transaction encoding was added.
- “Level-4” is used only as the audience context for the virtual-ECU question the comparison already states. The study does not report an ASIL, an ISO 26262 metric, or a vehicle-level test.

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

- The site says UCIe 3.0 improves link-level detection, sideband scheduling, and emergency or power signaling because the comparison’s opening sentence and the mechanism classes say that. It does not say those additions close transaction fate.
- “Additional status” is qualified: the fields still match on both sides of every equivalence pair, and no modeled mechanism is class F.
- Emergency Shutdown and Fast Throttle are described as optional, and Emergency Shutdown as fatal, following `findings_ucie3.md`. The UCIe 2.0 column says those mechanisms are not the ones added in the 3.0 text. It does not claim Revision 2.0 has no power management of any kind.
- Rates are labeled “observed under the selected seeded fault workload” and “not intrinsic protocol probabilities”.
- Credit violations and liveness failures are listed under the report’s names. The site does not explain them as UCIe defects. The model’s credit fault is an injected fault; the report does not split the 10 hits by cause, so the site does not either.
- Avoided: “critical flaw”, “broken specification”, “catastrophic”, and “UCIe bug”.
- The required sentence is on the summary and the limitations page: the study does not claim that UCIe is incorrect; it identifies a cross-layer recovery gap.

## Presentation choices

- The counterexample steps on the summary page are a condensation of T17 and T18 in report order. The line-by-line trace stays in the markdown reports.
- Bar widths use the published percentages on a 0–100% scale. They are not renormalized to the largest bar.
- Diagrams are inline SVG so they follow the NUVUS light and dark tokens. The files in `docs/assets/` are the editable copies. Standalone SVG loads `tokens.css` from the same directory and therefore opens in the light theme.
- Colour, type, space, radius, and the wordmark come from the NUVUS brand. Fonts are the SIL Open Font License files shipped with that brand.
