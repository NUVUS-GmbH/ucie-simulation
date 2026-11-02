# Fault-handling campaign results (fault_handling_v0.1)

Separate result area for the AXI4↔AXI4-over-UCIe-S fault-handling extension.
Historical `reference_architecture_runs/` and prior findings are unchanged.

All pass: **True**

| Experiment | Pass | Commit(s) | AXI resp/source | Fate A/C/D | Late discard |
| --- | --- | --- | --- | --- | --- |
| `fh_clean_success` | True | 1 | OKAY/TARGET | COMPLETED/COMPLETED/COMPLETED | 0 |
| `fh_loss_before_accept` | True | 0 | SLVERR/BRIDGE | UNKNOWN/UNKNOWN/NOT_COMPLETED | 0 |
| `fh_commit_then_local_slverr` | True | 1 | SLVERR/BRIDGE | UNKNOWN/UNKNOWN/UNKNOWN | 0 |
| `fh_no_commit_local_slverr` | True | 0 | SLVERR/BRIDGE | UNKNOWN/UNKNOWN/NOT_COMPLETED | 0 |
| `fh_watchdog_then_late_okay` | True | 1 | SLVERR/WATCHDOG | UNKNOWN/UNKNOWN/UNKNOWN | 1 |
| `fh_timeout_then_remote_commit` | True | 1 | SLVERR/WATCHDOG | UNKNOWN/UNKNOWN/UNKNOWN | 1 |
| `fh_blind_retry_after_slverr` | True | 2 | OKAY/TARGET | COMPLETED/COMPLETED/COMPLETED | 0 |
| `fh_no_retry_after_slverr` | True | 1 | SLVERR/BRIDGE | UNKNOWN/UNKNOWN/UNKNOWN | 0 |
| `fh_axi_id_reuse_late_resp` | True | 1,0 | SLVERR/WATCHDOG,SLVERR/WATCHDOG | UNKNOWN/UNKNOWN/UNKNOWN,UNKNOWN/UNKNOWN/NOT_COMPLETED | 1 |
| `fh_multi_outstanding_backpressure` | True | 1,1,1 | OKAY/TARGET,OKAY/TARGET,OKAY/TARGET | COMPLETED/COMPLETED/COMPLETED,COMPLETED/COMPLETED/COMPLETED,COMPLETED/COMPLETED/COMPLETED | 0 |
| `fh_read_burst_error_completion` | True | 1 | SLVERR/BRIDGE | UNKNOWN/UNKNOWN/UNKNOWN | 0 |
| `fh_race_resp_before_timeout` | True | 1 | SLVERR/BRIDGE | UNKNOWN/UNKNOWN/UNKNOWN | 0 |
| `fh_race_resp_simultaneous` | True | 1 | SLVERR/WATCHDOG | UNKNOWN/UNKNOWN/UNKNOWN | 0 |
| `fh_race_resp_after_timeout` | True | 1 | SLVERR/WATCHDOG | UNKNOWN/UNKNOWN/UNKNOWN | 0 |
| `fh_classifier_diag_compare` | True | 1,0 | SLVERR/BRIDGE,SLVERR/BRIDGE | UNKNOWN/UNKNOWN/UNKNOWN,UNKNOWN/NOT_COMPLETED/NOT_COMPLETED | 0 |
| `fh_diag_ack_loss_after_delivery` | True | 1 | SLVERR/WATCHDOG | UNKNOWN/UNKNOWN/UNKNOWN | 1 |
| `fh_observer_history_compare` | True | 1,0 | SLVERR/BRIDGE,SLVERR/BRIDGE | UNKNOWN/UNKNOWN/UNKNOWN,UNKNOWN/UNKNOWN/NOT_COMPLETED | 0 |
| `fh_safe_retry_cancel_remote` | True | 1 | OKAY/TARGET | COMPLETED/COMPLETED/COMPLETED | 0 |
| `fh_cancel_latency_race` | True | 1 | SLVERR/WATCHDOG | UNKNOWN/UNKNOWN/UNKNOWN | 1 |

## Observer-history compare (commit vs no-commit)

Mode: `single_run_two_txs`

Allowed observations:
- **A**: AXI handshakes + response codes (no internal RESP source)
- **B**: A + FDI link-down
- **C**: B + standardized status + optional DIAG when exposed
- **D**: C + disposition / late-discard / RESP source


### Event-sequence equality (timestamps stripped)

- Profile A: equal=True only_left=[] only_right=[]
- Profile B: equal=True only_left=[] only_right=[]
- Profile C: equal=False only_left=['DIAG:local_slverr=1,never_handed=0,missing_ack=0'] only_right=['DIAG:local_slverr=1,never_handed=0,missing_ack=1']
- Profile D: equal=False only_left=['DIAG:local_slverr=1,never_handed=0,missing_ack=0', 'DISP:fate_d=UNKNOWN,fate_d_no_diag=UNKNOWN'] only_right=['DIAG:local_slverr=1,never_handed=0,missing_ack=1', 'DISP:fate_d=NOT_COMPLETED,fate_d_no_diag=NOT_COMPLETED']

### Timed equality (timestamps retained)

- Profile A: equal=False left_len=4 right_len=4
- Profile B: equal=False left_len=4 right_len=4
- Profile C: equal=False left_len=6 right_len=6
- Profile D: equal=False left_len=8 right_len=8

Note: „A/B equal“ in the event-sequence sense does **not** imply timed observational equivalence.

## Watchdog vs retry/recovery timing

Shortened txn_watchdog_ns values are SIMULATION CONFIGURATION so the watchdog can fire inside a discrete-event run. They are not claims that 1 ms draft WDT is shorter than UCIe retry/recovery. In a real system, WDT_TIMEOUT must exceed max latency including retry + retrain (draft WDT-02).

- Draft WDT start: 1000000 ns
- UCIe LinkError residency (spec): 16000000 ns
- UCIe replay timeout flits (spec): 375
- Automotive retrain_ns: 5000
- Campaign shortened WDT examples: {'fh_watchdog_then_late_okay': 200, 'fh_timeout_then_remote_commit': 100, 'fh_race_resp_after_timeout': 100}

