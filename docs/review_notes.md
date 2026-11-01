# Reference-model site review

The page is rebuilt with `python3 scripts/build_study.py`. It uses stored evidence;
this site change does not rerun or modify simulation results. GitHub Pages still
publishes `docs/` through the existing workflow.

## Narrative frame

The public page is organized as a self-contained technical study:

problem → evidence → architecture → robustness → scope / limits

It does not organize content around “original finding,” “previous experiment,” or
research chronology. Robustness is framed as a check against simplified transport
assumptions, not as a timeline of iterations.

## Claim provenance audit

| Page claim | Classification | Evidence / boundary |
| --- | --- | --- |
| Commit=1 with UNKNOWN A–D; commit=0 with UNKNOWN A–C | SIMULATION RESULT | reference validation JSON, baseline_unknown / precommit |
| Same UNKNOWN fate labels and absent completion for A/B/C | SIMULATION RESULT + CLASSIFICATION POLICY | timed checker compares final labels and completion bits; full observer-history equivalence is not established |
| Blind software retry duplicates effects | SIMULATION RESULT + IMPLEMENTATION POLICY | blind_retry; not link replay duplicate suppression |
| TX32 / RX64 / retry16, early credits threshold2 | REFERENCE CONFIGURATION | neutral YAML exports; no universal requirement |
| Retry-buffer peak 1 is model-dependent; peak 3 under reference | SIMULATION RESULT + INTERPRETATION | validation finding MODEL_ARTIFACT; workload-specific occupancy |
| B latency 46→90 ns, delta 44 | SIMULATION RESULT | shared_quiet_ref / ref_cross_client |
| Budget 55.2 ns | SIMULATION CONFIGURATION | quiet 46×1.20; experimental budget only |
| Shared replay/sequence path delays B | INTERPRETATION + IMPLEMENTATION POLICY | CSV sequence discards 2019/2031, receipt 2043; adapter replay priority |
| Retrain does not explain B completion delay | INTERPRETATION | B completes 2090, Retrain starts 2104 |
| Retained ACK/NAK history determines pre-target fate for D | MODEL ASSUMPTION + SIMULATION RESULT | observer D; not standardized commit metadata |
| 16 ms minimum LinkError residency | SPECIFICATION FACT as cited by study | UCIe 2.0 §10.3.3.7, findings report |
| Retrain 5µs / reactivation 2µs | SIMULATION CONFIGURATION | YAML; no vendor-IP timing claim |
| L1 state preservation / AoU credit depth 32 | MODEL ASSUMPTION | reference report assumptions |
| Discard after LinkError / preserve retry across Retrain | IMPLEMENTATION POLICY | reference configuration |
| Same baseline fate in revision comparison | SIMULATION RESULT | narrow timed reference coverage; broader survey separate |
| Mitigation latency values | EXPLORATORY SIMULATION RESULT | exploratory campaign; not reference-revalidated |
| Remaining concrete-IP behavior | OPEN QUESTION | no RTL, PHY, or commercial implementation validation |

## Interpretation correction

- Strongly supported: commit counts 1/0 with UNKNOWN under the implemented classifier; blind retry produces 2 commits.
- Supported with qualification: those outcomes remain reachable with reference-configured buffers, credits and multiple outstanding flits under the same classification policy.
- Not established: full observational equivalence, or the impossibility of determining fate using richer diagnostics.
- Profiles B/C omit their additional diagnostic fields from the fate decision; their agreement with A is enforced by the implementation. Profile A does not consume diagnostic logs.
- The revision comparison labels broader state-model coverage separately from timed reference-model coverage in the visible panels.
- Historical reports and machine keys remain unchanged and are explicitly qualified on the page.
- Removed the trailing comma that made the comparison body a tuple.

## Verification

- Desktop (1280 px) and mobile (390 px) browser checks: no page-level overflow.
- Rendered traces, architecture, observer matrix, and time-scaled replay trace inspected.
- Local links and anchors resolved after rebuild.
- Exported JSON runs and metrics equal original source data.
- Neutral configuration exports preserve all non-comment parameter lines.
- Public text/config/code exports contain no reference-provider identifying names.
- `git diff --check` passed.

The supplied report's final generic caveat says interference may disappear under
multi-outstanding buffering. Its stored reference metrics reproduce a 44 ns delay;
the page explains this discrepancy without editing the original evidence.

Old secondary URLs and German URLs redirect to the current English study. A
reviewed translation is required before restoring German content.
