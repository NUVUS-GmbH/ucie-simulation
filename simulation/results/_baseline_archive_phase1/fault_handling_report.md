# Fault-handling extension report — AXI4 ↔ AXI4 over UCIe-S (draft V0.1)

**Campaign status:** all **19** automotive matrix experiments **PASS** (seed=1, `configs/automotive.yaml`);  
central fault cases also **PASS** on reference config (`configs/torsten_reference_ucie2x.yaml`, 12 runs).  
**Website:** not modified. **Commit/push:** not performed.

**Scope note:** PASS counts confirm the matrix. They do **not** by themselves mean every scientific gap is closed as an implementable mechanism — see §6 (ACK ≠ non-delivery), §7 (timed vs event-sequence observer equality), and §9 (cancel as MODEL ASSUMPTION).

**Project draft now on file:** `UCIe-S_AXI_Fehlerspezifikation.pdf`  
(Entwurf V0.1, 30.10.2026 — „Fehlererkennung und Fehlerbehandlung AXI4 zu AXI4 über UCIe-S“).  
Coverage was re-checked against that PDF; see `fault_handling_coverage_matrix.md`.  
Symbolic registers and open databook points in the draft remain **PROJECT REQUIREMENT / OPEN**, not confirmed commercial IP.

Statement tiers used below:
- **directly measured** — observed in this campaign’s result JSON/traces
- **under explicit model assumptions** — follows if listed assumptions hold
- **not yet established** — needs databook/bridge contract or further work

---

## 1. Changes versus the previous model

| Area | Before | After |
| --- | --- | --- |
| LinkError outstanding AXI | Policy Discard → no AXI completion → fate UNKNOWN | **ERR-05** optional `LocalSlverr` → AXI `SLVERR` from Bridge |
| Global sim timeout only | `sim_timeout_ns` | Plus **WDT-03** per-transaction watchdog |
| Late remote responses | Flushed/dropped without explicit WDT-04 lifecycle | **WDT-04** discard after `bridge.local_closed`; response TX allowed after local close |
| Observer A/B/C | Divergence treated as internal error | Equality is **measured**, not enforced |
| Classifier vs SLVERR | N/A (no local SLVERR) | SLVERR ≠ COMPLETED and ≠ automatic NOT_COMPLETED |
| Diagnostics in classify() | Fields present but unused / ACK misuse | **Rule:** local SLVERR ∧ **never handed to UCIe TX** → NOT_COMPLETED; missing ACK alone stays UNKNOWN |
| Local SLVERR path | Immediate `complete_resp()` (bypassed READY) | Staged onto `completion_q`; READY delay + per-beat read handshakes |
| Observer projection | B/C/D identical; A got internal RESP source | Prefix projection: AXI / FDI / STATUS / DIAG / DISP |
| Safe retry | Armed on NOT_COMPLETED alone | Requires NOT_COMPLETED **and** remote cancel (shared-state / optional latency) or dup-suppress — **not** a wire cancel protocol |
| State dimensions | Fate + ground commit | A: AXI proto state; B: ground truth; C: effect knowledge |
| Master after error | AutoRetry policy only | **ERR-07** Abort / BlindRetry / RetryOnlyIfNotCompleted |
| Results location | prior trees retained | New: `simulation/results/fault_handling_*` + `fault_handling_reference_*` |

Historical result directories (`reference_architecture_runs/`, prior findings) were **not** rewritten.

---

## 2. Traceability to requirements and specifications

See `fault_handling_coverage_matrix.md` (aligned to the PDF sections 3–7 and open points §9).

### AXI (SPECIFICATION FACT)

Arm AMBA AXI / ACE (IHI 0022), response chapter (§A3.4.4 / Table A3-5 in recent issues):

- BRESP/RRESP: OKAY, EXOKAY, SLVERR, DECERR.
- SLVERR denotes an unsuccessful transaction; examples include subordinate timeout.
- Writes: one response for the burst; completion is the write-response handshake.
- Reads: required beat count even when an error is reported; RLAST on the last beat.
- **SLVERR does not encode an architectural commit bit.**

### UCIe (unchanged SPECIFICATION FACT usage)

UCIe 2.0 rules already used by the timed model (CRC detect, replay, LinkError residency, flush, no protocol quiescence guarantee) remain. UCIe still does **not** define the AXI completion after LinkError — local SLVERR remains **PROJECT REQUIREMENT / POLICY** (draft ERR-05).

### Project draft (PROJECT REQUIREMENT) — key clauses implemented

| Draft ID | Text gist | Model |
| --- | --- | --- |
| ERR-05 | Link-Down/LinkError: all open txns get SLVERR; none left hanging | `LocalSlverr` |
| ERR-07 | Master evaluates SLVERR/DECERR; retry/abort in master spec (§9 still open) | `MasterRetryPolicy` |
| WDT-02/03 | ~1 ms start; timeout → SLVERR; reads finish with RLAST | txn watchdog |
| WDT-04 | Late responses discarded (draft: ID-Tracking) | discard after local close; **token/generation MODEL ASSUMPTION** because AXI ID reuse is unsafe |
| WDT-06 | After N timeouts (start 3) treat link failed / LinkError IRQ | escalate → LinkError request |

Watchdog 1 ms / escalate-after-3 are **REFERENCE CONFIGURATION** from the draft, not AXI/UCIe constants (campaign uses shortened ns for discrete-event runs — see §10).

---

## 3. Implemented vs still missing

**Implemented in this extension:** ERR-05, WDT-03, WDT-04, ERR-07; three-way state; classifier without forced A/B/C equality; diagnostic evaluation rule; local SLVERR under backpressure; observer projection + automatic history diff; race timing variants; safe-retry cancel; late-response TX after local close; read-burst error beat handshakes; experiment matrix; reference-architecture replay; coverage matrix; separate results.

**Still missing / not claimed:** PHY training & degrade; ECC; Poison undo; full AXI4-Stream IP; vendor register maps; Exactly-once from a token alone; commercial bridge databook behavior.

---

## 4. Experiment matrix — measured results (seed=1, automotive)

| # | Experiment | Commit | AXI resp/source | Proto | Fate A / C / D | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | `fh_clean_success` | 1 | OKAY/TARGET | COMPLETED_OK | COMPLETED / COMPLETED / COMPLETED | Baseline success |
| 2 | `fh_loss_before_accept` | 0 | SLVERR/BRIDGE | COMPLETED_ERROR | UNKNOWN / UNKNOWN / NOT_COMPLETED | D via disposition (diag off) |
| 3 | `fh_commit_then_local_slverr` | **1** | **SLVERR/BRIDGE** | COMPLETED_ERROR | **UNKNOWN / UNKNOWN / UNKNOWN** | **Hypothesis case** |
| 4 | `fh_no_commit_local_slverr` | 0 | SLVERR/BRIDGE | COMPLETED_ERROR | UNKNOWN / UNKNOWN / NOT_COMPLETED | Same AXI view family as (3) for A |
| 5 | `fh_watchdog_then_late_okay` | 1 | SLVERR/WATCHDOG | COMPLETED_ERROR | UNKNOWN / … | WDT-03 then WDT-04 |
| 6 | `fh_timeout_then_remote_commit` | **1** | SLVERR/WATCHDOG | COMPLETED_ERROR | UNKNOWN / … | Commit **after** local SLVERR |
| 7 | `fh_blind_retry_after_slverr` | **2** | OKAY/TARGET (final) | COMPLETED_OK | COMPLETED | AT_MOST_ONCE + FALSE_SAFE_RETRY |
| 8 | `fh_no_retry_after_slverr` | 1 | SLVERR/BRIDGE | COMPLETED_ERROR | UNKNOWN | No second commit |
| 9 | `fh_axi_id_reuse_late_resp` | 1 then 0 | SLVERR/WATCHDOG ×2 | COMPLETED_ERROR | UNKNOWN / … | Token match required for OKAY assignment |
| 10 | `fh_multi_outstanding_backpressure` | 1×3 | OKAY/TARGET | COMPLETED_OK | COMPLETED | READY delay cycles > 0 |
| 11 | `fh_read_burst_error_completion` | 1 | SLVERR/BRIDGE | COMPLETED_ERROR | UNKNOWN | beat handshakes + READY delay + RLAST |
| 12a | `fh_race_resp_before_timeout` | 1 | SLVERR/BRIDGE | COMPLETED_ERROR | UNKNOWN | Bridge wins; watchdog does not |
| 12b | `fh_race_resp_simultaneous` | 1 | SLVERR/WATCHDOG | COMPLETED_ERROR | UNKNOWN | Near-simultaneous; one completion |
| 12c | `fh_race_resp_after_timeout` | 1 | SLVERR/WATCHDOG | COMPLETED_ERROR | UNKNOWN | Watchdog wins |
| 13 | `fh_classifier_diag_compare` | 1, 0 | SLVERR×2 | COMPLETED_ERROR | A UNKNOWN; C UNKNOWN→NOT_COMPLETED | Handed vs never-handed; with/without diag |
| 13b | `fh_diag_ack_loss_after_delivery` | **1** | SLVERR/WATCHDOG | COMPLETED_ERROR | **C/D UNKNOWN** | Delivered + ACK lost ≠ NOT_COMPLETED |
| 14 | `fh_observer_history_compare` | 1, 0 | SLVERR×2 | COMPLETED_ERROR | event-seq A/B equal; timed unequal | Dual comparison |
| 15 | `fh_safe_retry_cancel_remote` | 1 | OKAY/TARGET | COMPLETED_OK | COMPLETED (final) | Safe under **instant shared-state cancel** |
| 16 | `fh_cancel_latency_race` | **1** | SLVERR/WATCHDOG | COMPLETED_ERROR | UNKNOWN | Cancel requested, remote still commits |

Full per-run artifacts: `simulation/results/fault_handling_runs/<exp>/`.  
Reference replay: `simulation/results/fault_handling_reference_runs/` + `fault_handling_reference_summary.md`.

---

## 5. Protocol completion versus knowledge of effect

**Directly measured (exp 3, 5, 6):**

- AXI protocol state can be `COMPLETED_ERROR` with `SLVERR` while ground-truth `architectural_commit_count == 1`.
- Observer A effect knowledge remains `INDETERMINATE` / fate `UNKNOWN`.
- Therefore: **local SLVERR completion does not supply reliable commit knowledge.**

**Directly measured (exp 4 vs 3):**

- Commit 0 and commit 1 can both present Bridge SLVERR with A=`UNKNOWN`.
- Observer D can yield `NOT_COMPLETED` when disposition shows the request never reached UCIe / never ACKed (exp 2/4), which is an **assumption-based** disposition export, not a UCIe-mandated AXI id/commit bit.

---

## 6. Effect of additional diagnostics

**Diagnostic evaluation rule (implemented in `classify()`):**

> When `use_diag_in_classifier` is true for profiles C/D:  
> `axi_completed_err ∧ diag_bridge_local_slverr ∧ diag_never_handed` → `NOT_COMPLETED`.  
> `diag_never_handed` means the request was **never handed to UCIe TX** (`!handed_to_ucie`).  
> `diag_missing_ack` is recorded for observation but **must not** drive NOT_COMPLETED:  
> the receiver can already have queued/accepted the request before the ACK returns.  
> Diagnostics still cannot invent COMPLETED / a commit bit.

**Directly measured (exp 13):**

| Tx | Commit | C with diag | C without diag | D with diag |
| --- | --- | --- | --- | --- |
| commit-then-SLVERR (handed) | 1 | UNKNOWN | UNKNOWN | UNKNOWN |
| LinkError before UCIe handoff | 0 | **NOT_COMPLETED** | UNKNOWN | NOT_COMPLETED |

**Counterexample (exp 13b `fh_diag_ack_loss_after_delivery`) — measured:**

- Request delivered to target, ACK persistently suppressed, architectural commit=1, then local watchdog SLVERR.
- `diag_missing_ack=true`, `diag_never_handed=false`.
- Fate C = D = A = **UNKNOWN**.
- Therefore a missing ACK is **not** safe evidence of non-execution.

Presence of fields ≠ classifier use: `expose_bridge_diagnostics` vs `use_diag_in_classifier` remain separate knobs. Each run also records `fate_c_without_diag` / `fate_d_without_diag`.

---

## 7. Classification equality versus observer history

**Allowed observations (projection prefixes):**

| Profile | Sees |
| --- | --- |
| A | `AXI:` / `ACCEPT` / `RESP_CODE_*` / `RBEAT_*` — **not** internal RESP source |
| B | A + `FDI:` |
| C | B + `STATUS:` + `DIAG:` (when exposed) |
| D | C + `DISP:` / late-discard / RESP source |

**Two separate comparison modes** (see `fault_handling_observer_diff.json`):

| Mode | Definition | Measured (exp 14, A/B) |
| --- | --- | --- |
| **Event-sequence** | Same projected event *names* after stripping `time_ns\|` | A/B **equal** |
| **Timed** | Identical full `time_ns\|event` rows | A/B **unequal** |

„A/B gleich“ in campaign summaries means **event-sequence equality after timestamp removal**, not timed observational equivalence. Time-observing observers are **not** covered by the event-sequence claim.

Forced A/B/C equality as an internal error remains **removed**.

---

## 8. Local error responses under backpressure

**Before:** `local_slverr()` called `complete_resp()` immediately; READY delay only on the target OKAY path; read beats filled in a zero-time loop.

**After:** local SLVERR is staged onto `completion_q`. `AxiMaster` applies `axi_resp_ready_delay_cycles`, then per-beat handshakes for remaining read beats, then `complete_resp()`.

**Directly measured (exp 11):** `resp_backpressured`, `beat_handshake_count >= burst_len`, `resp_ready_to_handshake_ns >= 0`, RLAST set.

---

## 9. Safe retry and future execution

**Directly measured (exp 6):** remote commit can occur **after** local timeout SLVERR while `allow_remote_continue` remains true. Therefore „bisher kein Commit“ is **not** a safe-retry permit.

**Implemented rule for `RetryOnlyIfNotCompleted`:**

1. Classifier must yield `NOT_COMPLETED` (C or D or A), **and**
2. Remote must not still be allowed to execute (`remote_may_continue` false via cancel) **or** `target_duplicate_suppress`.

**Cancel semantics — restricted claim:**

- `cancel_remote_on_local_close` with `cancel_latency_ns == 0` clears a **shared local flag** immediately. That is a **MODEL ASSUMPTION**, not a transmitted/confirmed cancel protocol.
- Missing for an implementable mechanism: cancel message on the wire, cancel latency as protocol RTT, cancel confirmation, and the race between cancel and commit as a protocol state machine.

**Directly measured:**

- Exp 15 (`fh_safe_retry_cancel_remote`): safe retry **under instantaneous shared-state cancel** → one retry, commit=1.
- Exp 16 (`fh_cancel_latency_race`): cancel *requested* on local SLVERR with large `cancel_latency_ns`, remote still commits → cancel request alone does not prevent future execution.

Optional model: `target_duplicate_suppress` keys commits by bridge token.

---

## 10. Watchdog timing vs retry / recovery

Shortened `txn_watchdog_ns` values are **SIMULATION CONFIGURATION** so the watchdog can fire inside a discrete-event run. They are **not** claims that the draft ~1 ms WDT is shorter than UCIe retry/recovery.

| Quantity | Value | Provenance |
| --- | --- | --- |
| Draft WDT start | ~1 ms (1e6 ns) | PROJECT REFERENCE CONFIG |
| Campaign shortened WDT | 100–200 ns (examples) | SIMULATION CONFIG |
| UCIe LinkError residency | 16e6 ns | SPEC (overridden only in race variants) |
| Replay timeout | ≥375 flits | SPEC |
| Automotive retrain_ns | 5000 | MODEL / config |

In a real system, WDT_TIMEOUT must exceed max latency including retry + retrain (draft WDT-02).

---

## 11. Reference architecture transfer

Central fault cases were re-run with `configs/torsten_reference_ucie2x.yaml` (`single_inflight_slot: false`, FIFO/credit/retry depths from the Torsten slide). Results: `fault_handling_reference_summary.{md,json}`. Overriding `single_inflight_slot` in one automotive scenario is **not** treated as a substitute for this replay.

---

## 12. Limits and open implementation questions

1. Draft document databook/register confirmation **not yet established**.
2. Bridge token/generation is a **MODEL ASSUMPTION**, not AXI/UCIe architecture.
3. AXI-to-Stream wording does **not** imply a full AXI4-Stream implementation.
4. Poison: not modeled as effect undo; a post-commit error cannot retract ground truth (**OPEN** for commercial IP).
5. Manager permanent READY=0 after BVALID: recorded as progress assumption; not a full channel FSM.
6. Exactly-once: token alone insufficient without durable target duplicate suppression (**optional model flag only**).

---

## 13. Build / test / reproduce

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure -R 'core|baseline_unknown|precommit|blind_retry|observational|fh_'
./build/ucie_sim --config configs/automotive.yaml --experiment fh_commit_then_local_slverr --seed 1 \
  --output timed_results/fault_handling/fh_commit_then_local_slverr
python3 -m simulation.experiments.fault_handling_campaign
```

Reports: `simulation/results/fault_handling_report.md`, `fault_handling_summary.{md,json}`,  
`fault_handling_observer_diff.json`, `fault_handling_reference_summary.{md,json}`,  
`fault_handling_coverage_matrix.md`.

---

## Answers to the closing questions

### Which prior statements still hold?

- Under **Discard** (legacy baseline), post-commit response loss still yields no OKAY and A=`UNKNOWN` (**regression measured**).
- Blind retry after an ambiguous completion can double-commit (**measured** in exp 7 and legacy `blind_retry`).
- Standardized UCIe status without AXI id/commit bit does not resolve fate for A/B (**measured**).
- UCIe still does not define AXI completion after LinkError (**specification fact**).

### Which must be restricted or withdrawn?

- Restrict: “initiator is stuck waiting with no AXI completion” as the *only* post-LinkError story — **ERR-05 can complete AXI with SLVERR** (**measured**).
- Restrict: any wording that equated “no completion” with the sole reason for UNKNOWN — UNKNOWN remains after SLVERR (**measured**).
- Withdraw as invariant: “observers A/B/C must always match or else the model is broken” — equality is an outcome, not a hard internal error.
- Restrict: “diagnostics exposed ⇒ classifier uses them” — only `use_diag_in_classifier` enables the rule (**measured**).

### Does the new fault handling only stop permanent waiting, or does it also give reliable commit knowledge?

**Directly measured:** it can end permanent waiting (protocol reaches `COMPLETED_ERROR`), but **does not** by itself provide reliable commit knowledge. Exp 3/5/6: SLVERR + commit=1 + A=`UNKNOWN`. Exp 4: SLVERR + commit=0 + A=`UNKNOWN`. With diagnostics, C can reach `NOT_COMPLETED` only when the request was never ACKed.

### When is a retry after SLVERR safe?

**Under model assumptions / measured counterexamples:**

- **Not safe** if policy is blind retry and a commit may already have occurred (exp 7 → commit=2).
- **Not safe** from “no commit yet” alone — exp 6 shows remote commit after local timeout.
- **Safer only under model assumptions** if classifier returns `NOT_COMPLETED` **and** the old attempt cannot still execute (instant shared-state cancel, exp 15) **or** durable duplicate suppression exists. Exp 16 shows a delayed cancel does not suffice.
- Abort (exp 8) avoids double commit but leaves effect knowledge indeterminate when commit may have happened.
- A missing UCIe ACK is **not** evidence of non-execution (exp 13b).

### What is missing for a statement about a concrete commercial implementation?

Exactly the draft’s own §9 open points, plus bridge semantics not settled by AXI/UCIe alone:

- Databook register names/addresses vs symbolic placeholders (CNT_*, WDT_*, INT_*).
- Whether the deployed IP version supports Format 6 and runtime link testing.
- Native CRC/Retry counters vs wrapper (CNT-01/02).
- Measured latency/BER → final WDT_TIMEOUT and thresholds.
- Poison transport (USER vs AXI5) and Optional Bytes.
- Final master policy on SLVERR/DECERR (ERR-07).
- Concrete meaning of WDT-04 „ID-Tracking“ if it is only AXI ID (insufficient under ID reuse) vs a stronger bridge token.

Until those are closed, results apply to **this model + draft policies**, not to a named commercial IP.

---

## Scientific hypothesis (restated)

> Can an AXI transaction be locally completed with SLVERR while the initiator still cannot safely determine whether the remote target already produced an architectural effect?

**Directly measured answer in this model:** **Yes.**  
Counterexamples: `fh_commit_then_local_slverr`, `fh_watchdog_then_late_okay`, `fh_timeout_then_remote_commit`.
