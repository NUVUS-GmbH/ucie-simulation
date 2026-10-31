"""UCIe 3.0 mechanisms that the fault model is allowed to use.

Each entry is a rule read from UCIe Specification Revision 3.0, Version 1.0
(5 August 2025). ``effect`` is the single class that best describes what the
mechanism contributes. Class F is architectural-commit knowledge. No entry
in this table is class F. The simulator treats that as a fact: standardized
signals never reclassify an AXI transaction.

"Retained" means the UCIe 3.0 text still states the rule. A UCIe 2.0
citation is not used unless the 3.0 text still says it.
"""

from __future__ import annotations

from dataclasses import dataclass

from simulation.model.types import Effect


@dataclass(frozen=True)
class Mechanism:
    name: str
    section: str
    where: str
    effect: Effect
    note: str


def catalog() -> tuple[Mechanism, ...]:
    g = Effect.G_NO_FATE_EFFECT
    return (
        Mechanism(
            "CRC, 3-bit detection",
            "§3.7",
            "retained",
            Effect.A_DETECTION,
            "A CRC mismatch still makes the flit invalid and still requires replay. "
            "Detection is not a record of whether the target committed.",
        ),
        Mechanism(
            "Retry and replay",
            "§3.8",
            "retained",
            Effect.D_TRANSPORT_STATE,
            "The Tx retry buffer still holds a flit until it is acknowledged. "
            "Selective NAK and the Rx retry buffer are still forbidden. "
            "Replay of an already accepted sequence is still not forwarded twice. "
            "This is transport state, not the target's architectural result.",
        ),
        Mechanism(
            "Replay timeout and replay-count recommendation",
            "§3.8",
            "retained",
            Effect.C_RECOVERY_TIMING,
            "REPLAY_TIMEOUT_FLIT_COUNT >= 375 still schedules replay. "
            "FLIT_REPLAY_NUM still points at the PCIe <= 32 GT/s recommendation "
            "and still does not restate an integer. The model keeps that integer "
            "as a parameter. A timeout is a correctable internal error, not a commit bit.",
        ),
        Mechanism(
            "Retry-variable reinitialization on Reset and Retrain",
            "§3.8",
            "retained",
            g,
            "Variables that PCIe initializes in the IDLE handshake are still "
            "initialized on RDI Reset or Retrain. The 3.0 text still does not "
            "say that the payload bytes, or an AXI id, survive that step.",
        ),
        Mechanism(
            "Sequence-number handshake",
            "§3.8, §10.2.8",
            "retained",
            g,
            "Still required on every FDI Active entry. Timeout is still 128 flits, "
            "and that timeout still exits to Retrain. It restarts sequence numbers. "
            "It does not report outstanding AXI transactions.",
        ),
        Mechanism(
            "Retrain",
            "§10.3.3.4",
            "retained",
            Effect.C_RECOVERY_TIMING,
            "CRC or parity errors crossing a threshold still cause Retrain, and "
            "the threshold algorithm is still implementation specific. "
            "Protocol layers still must not request Retrain on FDI unless the "
            "link is in Raw format. Retrain still does not flush by the LinkReset rule.",
        ),
        Mechanism(
            "LinkReset and Disabled flush",
            "§10.3.3.5, §10.3.3.6",
            "retained",
            g,
            "Best-effort drain still must not block entry. After the FDI state "
            "machine enters, pipelines and retry buffers are still flushed.",
        ),
        Mechanism(
            "LinkError",
            "§10.3.3.7",
            "retained",
            Effect.B_PROPAGATION,
            "Stall is still not required. The upper layer must still clean up "
            "after sampling LinkError, and in-flight packets may still be dropped. "
            "Minimum residency is still 16 ms. The state is visible. The identity "
            "of a committed AXI transaction is not.",
        ),
        Mechanism(
            "Sideband timeout to LinkError",
            "§10.3.3.8",
            "retained",
            Effect.B_PROPAGATION,
            "No response to a LinkError, LinkReset, or Disabled sideband handshake "
            "for 8 ms still moves RDI to LinkError.",
        ),
        Mechanism(
            "No protocol quiescence",
            "§10.3.4 implementation note",
            "retained",
            g,
            "The 3.0 note still says that, regardless of protocol, the adapter and "
            "PHY have no hardware mechanism that guarantees quiescence of "
            "transactions on LinkReset or Disabled. Streaming is called out in "
            "the same note. Outstanding AXI fate stays outside UCIe.",
        ),
        Mechanism(
            "FDI and RDI state",
            "§3.5, §10.2, §10.3.3",
            "retained",
            Effect.B_PROPAGATION,
            "For a streaming protocol the Adapter LSM is still what FDI exposes "
            "on pl_state_sts. That is link state.",
        ),
        Mechanism(
            "D2D error registers and link event notification",
            "§9.5.1, §9.5.3.2, §9.5.3.5",
            "retained",
            Effect.A_DETECTION,
            "Correctable bits still cover CRC, retrain, and an internal error. "
            "Uncorrectable bits still cover adapter timeout, receiver overflow, "
            "internal error, sideband error messages, and a bad parameter exchange. "
            "Header Log 1 still logs a sideband-mailbox header, not an AXI header. "
            "No field is an AXI id.",
        ),
        Mechanism(
            "Link Health Monitor",
            "§9.5.3.40",
            "retained from 1.1",
            Effect.A_DETECTION,
            "UHM status and eye margin describe the electrical link. They do not "
            "name a transaction.",
        ),
        Mechanism(
            "Management transport",
            "§2.5, §8.1",
            "retained from 2.0",
            g,
            "§8.1 defines manageability as independent of UCIe mainband protocols. "
            "MTP carries management packets. It is not an AXI completion path.",
        ),
        Mechanism(
            "Runtime link-testing parity",
            "§3.9",
            "3.0 text",
            Effect.A_DETECTION,
            "A parity mismatch is a correctable error. It does not, by itself, "
            "declare a flit invalid. Software can count it. It does not identify "
            "which MMIO write committed.",
        ),
        Mechanism(
            "Runtime recalibration and TARR",
            "§4.6",
            "3.0 enhancement",
            Effect.A_DETECTION,
            "Mainband data must continue to be sampled during recalibration. "
            "The 8 ms TARR handshake timeout is explicitly not an error and "
            "does not take RDI to LinkError.",
        ),
        Mechanism(
            "Sideband performant mode",
            "§4.1.5.1",
            "3.0",
            Effect.C_RECOVERY_TIMING,
            "Removes the 32 UI sideband gap after negotiation. This model counts "
            "logical protocol events, not UI. A shorter sideband gap is not a "
            "completion.",
        ),
        Mechanism(
            "Priority sideband packets",
            "§4.1.5.2",
            "3.0",
            Effect.C_RECOVERY_TIMING,
            "Optional. A priority packet may interrupt normal sideband traffic. "
            "The opcode distinguishes a chain from its end. The implementation "
            "note says further payload details are left to a future ECN. "
            "The model therefore records opcode-only and does not invent a "
            "transaction encoding.",
        ),
        Mechanism(
            "Fast Throttle",
            "§8.4.2.1",
            "3.0",
            g,
            "Optional power or thermal indication. Disabled at power-on. "
            "Logging is trigger and response history, not mainband transactions. "
            "The specification does not say that Fast Throttle drops a flit.",
        ),
        Mechanism(
            "Emergency Shutdown",
            "§8.4.2.2",
            "3.0",
            Effect.B_PROPAGATION,
            "Optional and fatal. It can be left only by reset. A chiplet may "
            "take a configured power action. The specification does not define "
            "a log of outstanding mainband transactions, so the model does not "
            "add one. Any local drain would be an implementation policy.",
        ),
        Mechanism(
            "Early Firmware Download",
            "§8.4.1",
            "3.0",
            g,
            "Firmware download state. Not an AXI completion path.",
        ),
        Mechanism(
            "48 GT/s and 64 GT/s",
            "revision history, §3.8",
            "3.0",
            g,
            "Higher rates do not add an end-to-end transaction acknowledgement. "
            "§3.8 still recommends the <= 32 GT/s replay-count rules.",
        ),
        Mechanism(
            "Continuous NOP insertion",
            "§3.3, §3.8",
            "3.0",
            g,
            "The adapter may keep sending NOPs instead of ending a burst with PDS. "
            "That is a power and latency choice. It does not report commit.",
        ),
    )


def commit_knowledge_defined() -> bool:
    """True only if some modeled mechanism is class F. None is."""

    return any(item.effect == Effect.F_COMMIT_KNOWLEDGE for item in catalog())
