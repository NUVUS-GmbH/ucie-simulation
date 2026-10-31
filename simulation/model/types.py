"""Shared enumerations.

Sources are named where a value is fixed by a specification. Values that exist
only so the simulator can explore a silent specification are marked ASSUMPTION.
"""

from __future__ import annotations

from enum import Enum, auto


class Phase(Enum):
    """Logical transaction phases. A fault may be armed at any one of them."""

    CREATED = auto()
    REQUEST_ACCEPTED_LOCALLY = auto()
    AOU_REQUEST_PACKED = auto()
    REQUEST_FLIT_TRANSMITTED = auto()
    REQUEST_RECEIVED_REMOTELY = auto()
    WRITE_DATA_ACCEPTED_LOCALLY = auto()
    AOU_WDATA_PACKED = auto()
    WDATA_FLIT_TRANSMITTED = auto()
    WRITE_DATA_RECEIVED_REMOTELY = auto()
    TARGET_ACCEPTED = auto()
    ARCHITECTURAL_COMMIT = auto()
    WRITE_RESPONSE_CREATED = auto()
    AOU_RESPONSE_PACKED = auto()
    WRITE_RESPONSE_TRANSMITTED = auto()
    WRITE_RESPONSE_RECEIVED = auto()
    TRANSACTION_COMPLETED = auto()


# Reads reuse the response phases. Write-data phases are skipped.
READ_SKIPPED_PHASES = {
    Phase.WRITE_DATA_ACCEPTED_LOCALLY,
    Phase.AOU_WDATA_PACKED,
    Phase.WDATA_FLIT_TRANSMITTED,
    Phase.WRITE_DATA_RECEIVED_REMOTELY,
}


class RequestType(Enum):
    WRITE = "WRITE"
    READ = "READ"


class OpKind(Enum):
    MEMORY_WRITE = "MEMORY_WRITE"
    MEMORY_READ = "MEMORY_READ"
    INCREMENT_COUNTER = "INCREMENT_COUNTER"
    PUSH_FIFO = "PUSH_FIFO"
    TRIGGER_DMA = "TRIGGER_DMA"
    START_ACCELERATOR = "START_ACCELERATOR"
    RING_DOORBELL = "RING_DOORBELL"
    ACK_INTERRUPT = "ACK_INTERRUPT"
    INCREMENT_ACTUATOR = "INCREMENT_ACTUATOR"
    POP_FIFO = "POP_FIFO"


IDEMPOTENT_OPS = {
    OpKind.MEMORY_WRITE,
    OpKind.MEMORY_READ,
}


class LinkState(Enum):
    """UCIe 2.0 adapter/RDI states used by this model (spec §10.3.3)."""

    RESET = "Reset"
    ACTIVE = "Active"
    RETRAIN = "Retrain"
    LINKRESET = "LinkReset"
    LINKERROR = "LinkError"
    DISABLED = "Disabled"


class AouState(Enum):
    """AoU activation FSM states referenced by public AoU 0.8 §8.3.3 citations.

    The full AoU 0.8 text is not in this workspace. Only the cited link-down
    behavior is treated as normative AoU. The other states are the activation
    sequence required to bring message exchange up, and the encoding of that
    handshake is abstracted.
    """

    DISABLED = "DISABLED"
    ACTIVATE = "ACTIVATE"
    ENABLED = "ENABLED"
    DEACTIVATE = "DEACTIVATE"


class Fate(Enum):
    COMPLETED = "COMPLETED"
    NOT_COMPLETED = "NOT_COMPLETED"
    UNKNOWN = "UNKNOWN"


class Policy(Enum):
    """What the protocol layer does with outstanding AXI transactions.

    UCIe 2.0 §10.3.4 says the adapter and PHY do not guarantee quiescence of
    protocol transactions on LinkReset or Disabled. AoU 0.8 §8.3.3, as cited by
    implementations, returns the activation FSM to DISABLED on FDI link-down
    and drops an in-flight Activate/Deactivate handshake. Neither document, in
    the text available here, defines the AXI-visible fate of an outstanding
    transaction. These policies are therefore explicit assumptions.
    """

    DISCARD = "A"
    PRESERVE_LOCAL = "B"
    EXPLICIT_UNKNOWN = "C"
    AUTO_RETRY = "D"
    NO_AUTO_RETRY = "E"


class Observation(Enum):
    """Which facts the initiator's classifier is allowed to use.

    FDI_AXI (profile 1) uses only an AXI completion (or its absence) and the
    fact that a destructive FDI state was entered. That is the recovery
    interface UCIe defines for the protocol layer: pl_state_sts, plus whatever
    AXI response was already delivered.

    SOFTWARE_ERROR (profile 2) also exposes the UCIe link and D2D error
    registers. Those registers name CRC, retrain, and LinkError conditions.
    They do not name an AXI transaction.

    MANAGEMENT (profile 3) also exposes standardized management information:
    management-transport status, Link Health Monitor presence, and, on
    UCIe 3.0, Fast Throttle, Emergency Shutdown, and priority-sideband
    opcode. None of those structures carry an architectural-commit bit.

    RETRY_DISPOSITION also uses the local adapter's ACK/NAK history latched
    before retry buffers are flushed. UCIe flushes retry state on Reset and
    on LinkReset entry. Nothing in UCIe 2.0 or 3.0 requires that history to
    be exported, tied to an AXI id, and retained across LinkError. Using it
    is an assumption, not a standardized profile.
    """

    FDI_AXI = "fdi_axi"
    SOFTWARE_ERROR = "software_error"
    MANAGEMENT = "management"
    RETRY_DISPOSITION = "retry_disposition"


class UcieVersion(Enum):
    """Which UCIe revision the adapter rules and the visible signals follow."""

    UCIE_2_0 = "2.0"
    UCIE_3_0 = "3.0"


class Effect(Enum):
    """What a UCIe mechanism contributes to the fate question.

    A through C can all be true of a mechanism that still leaves the
    initiator unable to name the architectural result. F is the only class
    that would close the ambiguity window by itself.
    """

    A_DETECTION = "A"
    B_PROPAGATION = "B"
    C_RECOVERY_TIMING = "C"
    D_TRANSPORT_STATE = "D"
    E_TRANSACTION_STATE = "E"
    F_COMMIT_KNOWLEDGE = "F"
    G_NO_FATE_EFFECT = "G"


class FaultKind(Enum):
    NONE = "none"
    SINGLE_BIT = "single_bit"
    TWO_BIT = "two_bit"
    THREE_BIT = "three_bit"
    REPEATED_CORRUPTION = "repeated_corruption"
    CRC_FAIL_REPLAY = "crc_fail_replay"
    RETRY_TIMEOUT = "retry_timeout"
    RETRY_THRESHOLD = "retry_threshold"
    FORCED_RETRAIN = "forced_retrain"
    FORCED_LINKRESET = "forced_linkreset"
    FORCED_LINKERROR = "forced_linkerror"
    UNILATERAL_RESET = "unilateral_reset"
    REMOTE_UNRESPONSIVE = "remote_unresponsive"
    SIDEBAND_TIMEOUT = "sideband_timeout"
    DROPPED_RESPONSE = "dropped_response"
    DROPPED_REQUEST = "dropped_request"
    AFTER_COMMIT_BEFORE_RESPONSE = "after_commit_before_response"
    BEFORE_ARCHITECTURAL_COMMIT = "before_architectural_commit"
    CREDIT = "credit"
    ACTIVATION = "activation"
    DEACTIVATION = "deactivation"
    REPEATED_DURING_RECOVERY = "repeated_during_recovery"
    # UCIe 3.0 mechanisms. They are not scheduled against a UCIe 2.0 run.
    EMERGENCY_SHUTDOWN = "emergency_shutdown"
    FAST_THROTTLE = "fast_throttle"
    PRIORITY_SIDEBAND = "priority_sideband"
    RUNTIME_RECAL = "runtime_recal"
    RUNTIME_PARITY = "runtime_parity"
    ENDPOINT_RESET = "endpoint_reset"


class Direction(Enum):
    A_TO_B = "AtoB"
    B_TO_A = "BtoA"


# UCIe 2.0 §3.7: the CRC polynomial gives a 3-bit detection guarantee.
CRC_DETECTION_GUARANTEE_BITS = 3

# UCIe 2.0 §3.8 incorporates the PCIe flit-replay rules by reference and
# recommends the <= 32.0 GT/s FLIT_REPLAY_NUM rules. UCIe does not restate the
# integer. The default of 3 is the modeled PCIe threshold (configurable).
DEFAULT_REPLAY_LIMIT = 3

# UCIe 2.0 §3.8: Replay Schedule Rule 0 checks REPLAY_TIMEOUT_FLIT_COUNT >= 375.
REPLAY_TIMEOUT_FLIT_COUNT = 375

# UCIe 2.0 §3.8: sequence-number handshake timeout is 128 flits.
SEQUENCE_HANDSHAKE_TIMEOUT_FLITS = 128

# UCIe 2.0 §10.3.3.8: no sideband response after 8 ms -> RDI LinkError.
SIDEBAND_TIMEOUT_MS = 8

# UCIe 2.0 §10.3.3.7: minimum LinkError residency is 16 ms.
LINKERROR_MIN_RESIDENCY_MS = 16

# UCIe 2.0 §3.8: MAX_UNACKNOWLEDGED_FLITS is min(Tx retry buffer, 127).
MAX_UNACKNOWLEDGED_FLITS_CAP = 127


def fault_bucket(phase: Phase) -> str:
    """Ground-truth phase at which a fault was armed. Not an initiator view.

    "After response delivery" means the AXI initiator has already taken B or R.
    A response that has only reached the local AoU FIFO is still before that
    handshake, so it stays in "After response generation".
    """

    if phase.value < Phase.TARGET_ACCEPTED.value:
        return "Before target acceptance"
    if phase.value < Phase.ARCHITECTURAL_COMMIT.value:
        return "After target acceptance"
    if phase == Phase.ARCHITECTURAL_COMMIT:
        return "After architectural commit"
    if phase.value < Phase.TRANSACTION_COMPLETED.value:
        return "After response generation"
    return "After response delivery"


BUCKET_ORDER = [
    "Before target acceptance",
    "After target acceptance",
    "After architectural commit",
    "After response generation",
    "After response delivery",
]
