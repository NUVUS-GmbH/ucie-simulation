# Requirement coverage matrix — UCIe-S AXI Fehlerspezifikation V0.1

**Source:** `UCIe-S_AXI_Fehlerspezifikation.pdf`  
„Fehlererkennung und Fehlerbehandlung AXI4 zu AXI4 über UCIe-S (x16, bis 32 GT/s, Flit Format 6)“,  
Entwurf V0.1, Datum 30.10.2026.

**Provenance rules:**
- Draft requirements = **PROJECT REQUIREMENT** (not proof of commercial IP capability).
- Register names in the draft are **symbolic placeholders** pending vendor databook alignment (draft §1 / §9).
- Vendor IP names from the draft are not restated here as confirmed product claims.

Status legend:
- **implemented (prior)** — timed model before this extension
- **implemented (this)** — fault-handling extension
- **abstracted** — timing/state only
- **not modeled**
- **needs databook/bridge contract**

| ID | Draft requirement (summary) | Status | Model notes |
| --- | --- | --- | --- |
| **D2D-01** | Negotiate Flit Format 6 both sides; verify in status after linkup | abstracted / prior | Format 6 recorded as reference metadata; no real negotiation FSM |
| **D2D-02** | Raw Format blocked; no Active if Format 6 unavailable | not modeled | Needs IP/capability model |
| **D2D-03** | CRC generate/check active | implemented (prior) | `crc_enabled`; detection guarantee, not LFSR |
| **D2D-04** | Retry/replay active; depth/timeout from latency @ 32 GT/s | implemented (prior) / needs databook | Replay + buffers; depth/timeout are config, not databook-derived |
| **D2D-05** | Replay rollover → Retrain; fail again → LinkError | implemented (prior) | Replay limit → Retrain; fail_retrain path → LinkError |
| **D2D-06** | Optional Bytes identical both sides | not modeled | Draft: project decision; open in §9 |
| **D2D-07** | Capability consistency check before linkup | not modeled | Needs capability registers |
| **D2D-08** | Error logging CE/UE/Fatal + interrupt | abstracted | Study `error_log`; not vendor interrupt map |
| **PHY-01** | Allow up to 32 GT/s | abstracted | Rate is serialization abstraction |
| **PHY-02** | Speed degrade 32→24→16 allowed | not modeled / abstracted | Older degrade experiment exists; not this matrix |
| **PHY-03** | Width degrade x16→x8 (no lane repair on UCIe-S) | not modeled | |
| **PHY-04** | Full link training each linkup/retrain | not modeled | PHY not cycle-accurate |
| **PHY-05** | Runtime link testing with parity if IP supports | not modeled | Draft §9: verify IP support |
| **PHY-06** | Runtime recalibration / retrain for drift | abstracted | Retrain dwell is config |
| **PHY-07** | Sideband parity + handshake timeouts | abstracted / prior | Sideband timeout modeled; parity not |
| **PHY-08** | Degrade raises interrupt for SW bandwidth awareness | not modeled | |
| **CNT-01** | CRC_ERR_CNT[15:0] saturating | abstracted | Study counters / crc_failures; not 16-bit sticky CSR |
| **CNT-02** | RETRY_CNT[15:0] saturating | abstracted | transparent_retries / retry counts |
| **CNT-03** | CNT_CTRL clear/freeze | not modeled | |
| **CNT-04** | CNT_STATUS saturation flags W1C | not modeled | |
| **THR-01** | CRC_ERR_THR start 0x0010 | not modeled | Draft: bring-up suggestion |
| **THR-02** | RETRY_THR start 0x0008 | not modeled | |
| **THR-03** | IRQ when count ≥ threshold; 0 disables | not modeled | |
| **ECC-01** | SECDED on bridge/streaming FIFOs | not modeled | Explicitly out of UCIe protection (draft §5) |
| **ECC-02** | Encode at bridge input, check at output | not modeled | |
| **ECC-03** | CE counted + INT_ECC_CE; UE → INT_ECC_UE | not modeled | |
| **ECC-04** | UE marks transaction faulty (§5.2) | not modeled | |
| **ECC-05** | ECC on by default, not disableable | not modeled | |
| **ERR-01** | No native AXI4 poison; USER/sideband or AXI5 poison | not modeled | Open point draft §9 |
| **ERR-02** | UE on write → poison; target must not take data | not modeled | Do **not** claim post-facto undo of committed effects |
| **ERR-03** | Bad read → RRESP SLVERR; bad write → BRESP SLVERR | implemented (this) for local policies | Target-path ECC/poison SLVERR not modeled |
| **ERR-04** | Undecodable address → DECERR | not modeled | Enum exists; no decode map |
| **ERR-05** | Link-Down/LinkError: complete all open txns with SLVERR; none left hanging | **implemented (this)** | `LocalSlverr` / `local_slverr_on_link_error` |
| **ERR-06** | Each error response → INT_AXI_ERR; log address + AXI ID | abstracted (this) | Logged in trace/error_log; no interrupt pin model |
| **ERR-07** | Master handles SLVERR/DECERR; retry/abort in master spec | **implemented (this)** as configurable policy | Abort / BlindRetry / RetryOnlyIfNotCompleted; safe retry also requires remote cancel or dup-suppress |
| **WDT-01** | Per write (AW/W/B) and read (AR/R) group; arm from address accept to BVALID/RLAST complete | **implemented (this)** | Per outstanding txn; channel-group split is logical not separate HW timers |
| **WDT-02** | WDT_TIMEOUT in cycles; > max latency incl. retry/retrain; ~1 ms start | **implemented (this)** | Config `txn_watchdog_ns`; campaign uses shortened values. **REFERENCE CONFIGURATION** |
| **WDT-03** | Timeout → SLVERR; reads finish remaining beats with RLAST | **implemented (this)** | Measured under READY backpressure + per-beat handshakes (`fh_read_burst_error_completion`) |
| **WDT-04** | Late responses after timeout discarded; must not disturb AXI (via ID-Tracking) | **implemented (this)** | Draft says ID-Tracking. Model uses **bridge token + generation** (**MODEL ASSUMPTION**): AXI ID alone is insufficient (exp `fh_axi_id_reuse_late_resp`) |
| **WDT-05** | Timeout → INT_WDT; log address, AXI ID, direction | abstracted (this) | Logged; interrupt pin not modeled |
| **WDT-06** | Repeated timeouts (WDT_MAX_TO, start 3) → link failed, INT_LINK_ERROR | **implemented (this)** | Escalate requests LinkError; count is project config |
| **WDT-07** | Watchdog default on; 0 disables only in debug | **implemented (this)** | `txn_watchdog_enabled` |

## Draft §7 coverage table vs model

| Fault class (draft) | Mechanism | Model |
| --- | --- | --- |
| Link bit errors | CRC + Retry | prior |
| High error rate | Threshold IRQs | not modeled |
| Lane failure UCIe-S | Runtime test, retrain, width degrade | not modeled |
| Signal drift | Recal / retrain / speed degrade | abstracted retrain only |
| Sideband errors | Parity, timeouts | timeout abstracted |
| Bridge/FIFO bit errors | SECDED | not modeled |
| Bad address | DECERR | not modeled |
| Link-Down | LinkError + WDT → SLVERR | **this** (ERR-05, WDT) |
| Hung transaction | AXI WDT → SLVERR | **this** |
| Undetected CRC | residual risk | acknowledged, not closed |

## Draft §9 open points (still open)

- Real register names/addresses from vendor databook
- IP support for Format 6 and runtime link testing
- Native CNT-01/02 vs wrapper
- Thresholds and WDT timeout after measured latency/BER
- Poison transport (USER vs AXI5) and Optional Bytes
- Final AXI master policy on SLVERR/DECERR (ERR-07 project decision)

## AXI specification facts used with this draft

Arm IHI 0022 (AXI response chapter / Table A3-5): OKAY/EXOKAY/SLVERR/DECERR; write completion = B handshake; reads must return required beats incl. RLAST on error; SLVERR ≠ architectural commit bit.
