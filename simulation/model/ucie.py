"""UCIe 2.0 adapter behavior used by the simulator.

Modeled from UCIe Specification Revision 2.0, Version 1.0 (August 6, 2024):

- §3.7 CRC. A CRC mismatch makes the flit invalid and replay must be
  requested. The polynomial gives a 3-bit detection guarantee. This model
  applies that guarantee. It does not execute the CRC LFSR in crc_gen.vs.
- §3.8 Retry. Retry is the PCIe flit replay scheme with the UCIe deltas:
  no selective NAK, no Rx retry buffer, 8-bit sequence numbers,
  REPLAY_TIMEOUT_FLIT_COUNT >= 375 schedules replay and is a correctable
  internal error, and a condition that would send PCIe to Recovery makes
  the UCIe adapter initiate Retrain. Sequence numbers restart their
  handshake on every Active entry from Reset or Retrain.
- §3.8 duplicate suppression. The receiver accepts a payload flit only when
  its sequence number is the next expected number. A replay of an already
  accepted sequence is acknowledged and not forwarded again. (PCIe rule
  incorporated by §3.8. Selective NAK and the Rx retry buffer are explicitly
  forbidden; sequence checking itself remains.)
- §10.3.3.4 Retrain. The CRC-error threshold that provokes Retrain is
  implementation specific. Protocol layers must not request Retrain on FDI
  unless the link is in Raw format. AoU over flit-mode streaming therefore
  does not request Retrain; the adapter does, on RDI, and Retrain is
  propagated to the adapter LSM.
- §10.3.3.5 LinkReset and §10.3.3.6 Disabled. Best-effort drain of retry
  buffers must not block entry. Once the FDI state machine has entered the
  state, the protocol layer and the adapter flush pipelines and retry buffers.
- §10.3.3.7 LinkError. Stall handshake is not required. The upper layer must
  clean up its datapath once it has sampled LinkError. In-flight packets may
  be dropped. Minimum residency is 16 ms so a partner that missed the
  sideband message still enters LinkError on timeout.
- §10.3.3.8. If a sideband handshake for LinkError, LinkReset, or Disabled
  gets no response for 8 ms, RDI enters LinkError.
- §10.3.4 implementation note. There is no hardware mechanism in the adapter
  or PHY that guarantees quiescence of protocol transactions on LinkReset
  or Disabled.

ASSUMPTION: time is logical. The 375-flit replay timeout, the 128-flit
sequence handshake timeout, and the 8 ms / 16 ms timers are events, not
wall-clock delays. Their order matches the specification: a timeout is a
single scheduled event that means "the specified threshold was reached".

ASSUMPTION: whether unacknowledged retry-buffer contents survive Retrain is
not stated as a flush rule (flush is stated for LinkReset, Disabled, and
LinkError cleanup). §3.8 does reinitialize the variables that PCIe
initializes in the IDLE flit handshake on entry to Retrain. Survival of the
payload bytes is therefore a policy: ``retrain_preserves_retry``.
"""

from __future__ import annotations

from dataclasses import dataclass, replace

from simulation.model.types import (
    CRC_DETECTION_GUARANTEE_BITS,
    DEFAULT_REPLAY_LIMIT,
    LINKERROR_MIN_RESIDENCY_MS,
    MAX_UNACKNOWLEDGED_FLITS_CAP,
    REPLAY_TIMEOUT_FLIT_COUNT,
    SEQUENCE_HANDSHAKE_TIMEOUT_FLITS,
    SIDEBAND_TIMEOUT_MS,
    Direction,
    LinkState,
)


@dataclass
class Flit:
    seq: int
    role: str  # request, wdata, response, credit, nop, ack, nak
    direction: Direction
    tx_index: int
    crc_ok: bool = True
    corrupt_bits: int = 0

    def replay_copy(self) -> "Flit":
        return replace(self, crc_ok=True, corrupt_bits=0)


@dataclass
class AdapterConfig:
    retry_buffer_depth: int = 8
    replay_limit: int = DEFAULT_REPLAY_LIMIT
    retrain_preserves_retry: bool = True

    @property
    def max_unacknowledged(self) -> int:
        # UCIe 2.0 §3.8.
        return min(self.retry_buffer_depth, MAX_UNACKNOWLEDGED_FLITS_CAP)


@dataclass
class AdapterPair:
    """Two link partners. Propagation is explicit so a one-sided fault shows up."""

    state_a: LinkState = LinkState.RESET
    state_b: LinkState = LinkState.RESET
    next_seq: dict[Direction, int] | None = None
    expected_seq: dict[Direction, int] | None = None
    retry: dict[Direction, list[Flit]] | None = None
    channel: dict[Direction, Flit | None] | None = None
    replay_without_progress: int = 0

    def __post_init__(self) -> None:
        self.next_seq = {Direction.A_TO_B: 0, Direction.B_TO_A: 0}
        self.expected_seq = {Direction.A_TO_B: 0, Direction.B_TO_A: 0}
        self.retry = {Direction.A_TO_B: [], Direction.B_TO_A: []}
        self.channel = {Direction.A_TO_B: None, Direction.B_TO_A: None}

    def both(self, state: LinkState) -> None:
        self.state_a = state
        self.state_b = state

    def active(self) -> bool:
        return self.state_a == LinkState.ACTIVE and self.state_b == LinkState.ACTIVE

    def compatible(self) -> bool:
        return self.state_a == self.state_b

    def reset_sequence_handshake(self) -> None:
        # UCIe 2.0 §3.8: sequence-number handshake on Active entry from Reset
        # or Retrain. Abstracted as resetting the expected/next counters when
        # a destructive transition drops every outstanding flit. A preserving
        # Retrain does not call this.
        self.next_seq = {Direction.A_TO_B: 0, Direction.B_TO_A: 0}
        self.expected_seq = {Direction.A_TO_B: 0, Direction.B_TO_A: 0}
        self.replay_without_progress = 0


def crc_detects(corrupt_bits: int) -> bool:
    """UCIe 2.0 §3.7 3-bit detection guarantee.

    One, two, and three flipped bits are detected. The model does not claim
    anything about four or more bits; those faults are not in the injection set.
    """

    if corrupt_bits <= 0:
        return False
    return corrupt_bits <= CRC_DETECTION_GUARANTEE_BITS


# Re-exported so call sites can name the normative constants without importing
# the raw integers from types.
SPEC_REPLAY_TIMEOUT_FLIT_COUNT = REPLAY_TIMEOUT_FLIT_COUNT
SPEC_SEQUENCE_HANDSHAKE_TIMEOUT_FLITS = SEQUENCE_HANDSHAKE_TIMEOUT_FLITS
SPEC_SIDEBAND_TIMEOUT_MS = SIDEBAND_TIMEOUT_MS
SPEC_LINKERROR_MIN_RESIDENCY_MS = LINKERROR_MIN_RESIDENCY_MS
SPEC_REPLAY_LIMIT_IS_REFERENCED = DEFAULT_REPLAY_LIMIT
