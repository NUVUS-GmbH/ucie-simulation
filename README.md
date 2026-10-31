# AXI over UCIe fault-injection simulation

Discrete-event model of an AXI manager talking to a device across one AoU
endpoint pair and one UCIe 2.0 link. The question is whether, after LinkError
or reset recovery, the initiator can put every interrupted AXI transaction
into exactly one of `COMPLETED`, `NOT_COMPLETED`, or `UNKNOWN`.

It is not a PHY and not a cycle-accurate UCIe model. Time is a sequence of
protocol events.

## Run

From this directory:

```bash
python -m unittest discover -s simulation/tests -v
python -m simulation.main --seed 1 --random 200
```

The second command writes three reports. Counts come from the run.

- `simulation/results/findings.md` is the UCIe 2.0 study.
- `simulation/results/findings_ucie3.md` replays that study on the UCIe 3.0 model.
- `simulation/results/comparison_ucie2_vs_ucie3.md` is the paired comparison.

UCIe 3.0 is not assumed to inherit a UCIe 2.0 rule. Retained rules are the ones
Revision 3.0 still states. They are listed in `simulation/model/spec30.py`.
Mechanisms that Revision 3.0 adds (priority sideband, Fast Throttle, Emergency
Shutdown, runtime recalibration, runtime parity) are modeled only to the
extent the 3.0 text defines them. None of those sections defines an AXI
commit bit, so the classifier is not given one. Where the text is silent,
the behavior is a named policy rather than an invented completion.

## What is taken from the specifications

UCIe behavior is taken from UCIe Specification Revision 2.0, Version 1.0
(6 August 2024), the PDF in this directory. The rules that the model actually
uses are named in `simulation/model/ucie.py`: CRC's 3-bit detection guarantee
(§3.7), replay instead of delivery of an invalid flit, the replay timeout at
375 flit-times and the mapping of PCIe Recovery onto Retrain (§3.8), flush of
retry buffers and pipelines on LinkReset and on LinkError cleanup (§10.3.3.5,
§10.3.3.7), the 8 ms sideband timeout and the 16 ms LinkError residency
(§10.3.3.8, §10.3.3.7), and the statement that the adapter does not guarantee
quiescence of protocol transactions (§10.3.4).

The 375-flit, 128-flit, 8 ms, and 16 ms thresholds are single events. They are
not wall-clock delays. The CRC LFSR in the specification is not executed. One,
two, and three bit flips are detected because §3.7 guarantees that. The
replay-count limit defaults to 3 because §3.8 points at the PCIe rules for
speeds at or below 32 GT/s and does not restate the integer. It is a parameter.

The AoU 0.8 specification is not in this workspace. The only AoU rule treated
as normative is the publicly cited §8.3.3 behavior: FDI link-down (LinkError,
LinkReset, Disabled, Reset) returns the activation FSM to DISABLED and drops
an in-flight Activate/Deactivate handshake. Retrain does not. Packing is one
message per flit. Credits are one per message, not per granule. Both are
labeled as abstractions in `simulation/model/aou.py`.

## Policies

UCIe does not say what AXI completion an outstanding transaction receives
after LinkError. The model will not invent one silent answer. `Policy` in
`simulation/model/types.py` is the choice:

| Policy | Meaning |
| --- | --- |
| A | Discard outstanding AoU transactions. No AXI completion. |
| B | If a completion already sat in the initiating AoU, deliver it after cleanup. |
| C | Return an explicit UNKNOWN to the software model. Not OKAY, and not permission to retry. |
| D | Automatically retry the same logical transaction after the link is up. |
| E | Do not retry automatically. Same initiator-visible result as A in this model. |

A second switch, `Observation`, chooses what the classifier may read.

- `fdi_axi` uses only "AXI was accepted" and "OKAY was received", plus the fact of a destructive link event. That is the recovery interface UCIe defines for the protocol layer.
- `retry_disposition` also uses ACK/NAK history latched before the retry buffer is flushed. §3.8 and §10.3.3.5 do not require that history to survive, or to be tied to an AXI id. Using it is an assumption.

The device commits a write only after the write data has been delivered. That is a device-model assumption, not an AoU rule. Early write response is off unless a scenario turns it on. It is an implementation feature, not baseline AoU 0.8 behavior.

## Website

The study is presented as a static site in `docs/` for a technical audience. It uses the NUVUS brand (tokens, Geist, and the wordmark). Those assets are copied into `docs/assets/` so GitHub Pages does not need a build or a secret.

Figures on the site are taken from the three reports above. `docs/sources/` is a copy of those reports. `docs/review_notes.md` lists the claims, the assumptions, the metrics, and the places where the wording stays inside the evidence.

### Local preview

From this directory:

```bash
python3 -m http.server 8000 -d docs
```

Open `http://127.0.0.1:8000/`.

### GitHub Pages

1. Push the repository to GitHub.
2. Open Settings, then Pages.
3. Under Build and deployment, choose Deploy from a branch.
4. Choose branch `main` and folder `/docs`.
5. Save.

The site is then served at `https://nuvus-gmbh.github.io/ucie-simulation/`. There is no backend and no build step. Pages must use the `/docs` folder. The parent `simulation/` tree is not part of that artifact. The pages link to the copies under `docs/sources/` and name the repository paths for the model and the tests.

`docs/.nojekyll` is present so GitHub Pages serves the files as written.

## Layout

```
simulation/
    model/          AXI, AoU, UCIe, target, channel, system
    faults/         injector and scenarios
    verification/   invariants and the systematic explorer
    experiments/    deterministic and seeded random runners
    reporting/      traces and the generated report
    tests/
    main.py
```
