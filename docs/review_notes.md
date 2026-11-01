# Reference-model site review

The page is rebuilt with `python3 scripts/build_study.py`. It uses stored evidence;
this site change does not rerun or modify simulation results. GitHub Pages still
publishes `docs/` through the existing workflow.

## Claim provenance audit

| Page claim | Classification | Evidence / boundary |
| --- | --- | --- |
| Commit=1 with UNKNOWN A–D; commit=0 with UNKNOWN A–C | SIMULATION RESULT | reference validation JSON, baseline_unknown / precommit |
| Observational equivalence A/B/C | SIMULATION RESULT | reference validation metrics / paired runs |
| Blind software retry duplicates effects | SIMULATION RESULT + IMPLEMENTATION POLICY | blind_retry; not link replay duplicate suppression |
| TX32 / RX64 / retry16, early credits threshold2 | REFERENCE CONFIGURATION | neutral YAML exports; no universal requirement |
| Peak1 becomes peak3 | SIMULATION RESULT + INTERPRETATION | validation finding MODEL_ARTIFACT; workload-specific occupancy |
| B latency46→90 ns, delta44 | SIMULATION RESULT | shared_quiet_ref / ref_cross_client |
| Budget55.2 ns | SIMULATION CONFIGURATION | quiet46×1.20; old budget73.2 applies only to old campaign |
| Shared replay/sequence path delays B | INTERPRETATION + IMPLEMENTATION POLICY | CSV sequence discards2019/2031, receipt2043; adapter replay priority |
| Retrain does not explain B completion delay | INTERPRETATION | B completes2090, Retrain starts2104 |
| Retained ACK/NAK history determines pre-target fate for D | MODEL ASSUMPTION + SIMULATION RESULT | observer D; not standardized commit metadata |
| 16 ms minimum LinkError residency | SPECIFICATION FACT as cited by study | UCIe2.0 §10.3.3.7, original findings report |
| Retrain5µs / reactivation2µs | SIMULATION CONFIGURATION | YAML; no vendor-IP timing claim |
| L1 state preservation / AoU credit depth32 | MODEL ASSUMPTION | reference report assumptions |
| Discard after LinkError / preserve retry across Retrain | IMPLEMENTATION POLICY | reference configuration |
| Same baseline fate in revision comparison | SIMULATION RESULT | narrow timed reference coverage; earlier Python coverage separate |
| Mitigation latency values | EXPLORATORY SIMULATION RESULT | earlier single-slot campaign only; not reference-revalidated |
| Remaining concrete-IP behavior | OPEN QUESTION | no RTL, PHY, or commercial implementation validation |

## Verification

- Desktop (1280 px) and mobile (390 px) browser checks: no page-level overflow.
- Rendered counterexample, architecture, and time-scaled replay trace inspected.
- 37 local links and anchors resolved after consolidation.
- Exported JSON runs and metrics equal original source data.
- Neutral configuration exports preserve all non-comment parameter lines.
- Public text/config/code exports contain no reference-provider identifying names.
- `git diff --check` passed.

The supplied report's final generic caveat says interference may disappear under
multi-outstanding buffering. Its stored reference metrics reproduce a 44 ns delay;
the page explains this discrepancy without editing the original evidence.

Old secondary URLs and German URLs redirect to the current English study. A
reviewed translation is required before restoring German content; the old phrase
replacement generator no longer applies to this narrative.
