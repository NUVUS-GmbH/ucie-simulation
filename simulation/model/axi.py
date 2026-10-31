"""AXI initiator state.

The initiator is the AXI manager attached to AoU endpoint A. After it has
seen AWREADY/ARREADY it is waiting for B or R. UCIe does not deliver a
per-transaction fate on FDI, so a missing response is not, by itself, proof
that the target did nothing.
"""

from __future__ import annotations

from dataclasses import dataclass, field

from simulation.model.signals import StdSignals

from simulation.model.types import (
    IDEMPOTENT_OPS,
    Fate,
    Observation,
    OpKind,
    Phase,
    RequestType,
)


@dataclass
class InitiatorObservation:
    """Facts a classifier may read. Ground-truth commit count is not here."""

    axi_completed_ok: bool
    axi_accepted: bool
    explicit_unknown: bool
    handed_to_ucie: bool
    request_ack: bool
    data_ack: bool
    request_nak_only: bool
    data_nak_only: bool
    data_attempted: bool
    request_timeout: bool
    data_timeout: bool
    is_write: bool
    retain_progress: bool


@dataclass
class Transaction:
    logical_id: int
    axi_id: int
    request: RequestType
    address: int
    op: OpKind
    data: int = 1
    phase: Phase = Phase.CREATED
    # --- ground truth, never an input to classification ---
    ground_commit_count: int = 0
    target_accepted: bool = False
    response_generated: bool = False
    # --- initiator / adapter-visible state ---
    axi_accepted: bool = False
    axi_completed_ok: bool = False
    explicit_unknown: bool = False
    handed_to_ucie: bool = False
    request_ack: bool = False
    data_ack: bool = False
    response_ack: bool = False
    request_nak_only: bool = False
    data_nak_only: bool = False
    response_nak_only: bool = False
    data_attempted: bool = False
    request_timeout: bool = False
    data_timeout: bool = False
    response_timeout: bool = False
    response_at_local_aou: bool = False
    requester_received_response: bool = False
    stopped: bool = False
    ucie_replay_count: int = 0
    software_retry_count: int = 0
    recovery_epoch: int = 0
    commit_at_reissue: int | None = None
    duplicate_completion: bool = False
    # Filled in at the end of a run.
    initiator_class: Fate | None = None
    fdi_class: Fate | None = None
    software_class: Fate | None = None
    management_class: Fate | None = None
    disposition_class: Fate | None = None
    oracle_class: Fate | None = None
    fault_phase: Phase | None = None
    effect_log: list[str] = field(default_factory=list)
    signals: StdSignals = field(default_factory=StdSignals)
    # Logical-event distance from architectural commit to either an AXI
    # completion or the return to Active without one. None if this transaction
    # never committed. The counter ignores UCIe 3.0 notification lines so a
    # version comparison is not an artifact of extra log text.
    commit_milestone: int | None = None
    fate_known_milestone: int | None = None
    recovery_milestone: int | None = None
    ambiguity_window: int | None = None
    ambiguity_open: bool = False

    @property
    def is_write(self) -> bool:
        return self.request == RequestType.WRITE

    @property
    def idempotent(self) -> bool:
        return self.op in IDEMPOTENT_OPS

    @property
    def effect_occurred(self) -> bool:
        return self.ground_commit_count > 0

    @property
    def name(self) -> str:
        kind = "MMIO_WRITE" if self.is_write and not self.idempotent else (
            "MEMORY_WRITE" if self.is_write else (
                "MMIO_READ" if not self.idempotent else "MEMORY_READ"
            )
        )
        if self.op == OpKind.MEMORY_READ:
            kind = "MEMORY_READ"
        if self.op == OpKind.MEMORY_WRITE:
            kind = "MEMORY_WRITE"
        if self.is_write and not self.idempotent:
            kind = "MMIO_WRITE"
        if not self.is_write and not self.idempotent:
            kind = "MMIO_READ"
        return kind

    def observation(self, retain_progress: bool) -> InitiatorObservation:
        return InitiatorObservation(
            axi_completed_ok=self.axi_completed_ok,
            axi_accepted=self.axi_accepted,
            explicit_unknown=self.explicit_unknown,
            handed_to_ucie=self.handed_to_ucie,
            request_ack=self.request_ack,
            data_ack=self.data_ack,
            request_nak_only=self.request_nak_only,
            data_nak_only=self.data_nak_only,
            data_attempted=self.data_attempted,
            request_timeout=self.request_timeout,
            data_timeout=self.data_timeout,
            is_write=self.is_write,
            retain_progress=retain_progress,
        )


def classify_with_standardized_signals(obs: InitiatorObservation, signals: StdSignals) -> Fate:
    """Profiles 2 and 3.

    UCIe 3.0 error registers, Link Health Monitor, management transport,
    Fast Throttle, Emergency Shutdown, and priority-sideband opcode do not
    carry an AXI identifier or an architectural-commit bit. They must not
    change the FDI/AXI label. A true commit bit would mean the model had
    invented a field the specification does not define.
    """

    if (
        signals.management_reports_axi_commit
        or signals.header_log_has_axi_id
        or signals.link_health_has_axi_id
    ):
        raise RuntimeError(
            "a standardized UCIe structure was treated as carrying an AXI commit bit"
        )
    return classify(obs)


def classify(obs: InitiatorObservation) -> Fate:
    """Classify from initiator-visible facts only.

    COMPLETED requires an OKAY response the initiator actually got.
    An explicit UNKNOWN response (policy C) is UNKNOWN, not a successful
    completion and not permission to retry.

    FDI_AXI (retain_progress False): after AWREADY/ARREADY, a missing B/R is
    UNKNOWN. The master cannot see whether the flit was NAK'd, ACKed, or
    committed. A transaction the master has not handed to the endpoint is
    NOT_COMPLETED.

    RETRY_DISPOSITION (retain_progress True, ASSUMPTION): a write whose
    request or write-data was sent and received only NAKs, with no ACK and no
    replay timeout, was never forwarded to AoU. UCIe 2.0 §3.7 declares a
    CRC-mismatched flit invalid and requires replay rather than delivery.
    That case is NOT_COMPLETED. An ACK, or a timeout that could hide a lost
    ACK, is not proof, so the fate stays UNKNOWN.
    """

    if obs.axi_completed_ok:
        return Fate.COMPLETED
    if obs.explicit_unknown:
        return Fate.UNKNOWN
    if not obs.axi_accepted:
        return Fate.NOT_COMPLETED
    if not obs.retain_progress:
        return Fate.UNKNOWN

    if not obs.handed_to_ucie:
        # The endpoint still knows it never transmitted. ASSUMPTION: that
        # fact was latched before cleanup. FDI pl_state_sts does not carry it.
        return Fate.NOT_COMPLETED

    if obs.is_write and not obs.data_attempted:
        # ASSUMPTION (device model): a write does not commit before its data
        # is delivered. The endpoint can know it never transmitted the data
        # only if that fact is latched. FDI does not carry it.
        return Fate.NOT_COMPLETED

    if obs.is_write:
        request_absent = (
            obs.request_nak_only
            and not obs.request_ack
            and not obs.request_timeout
        )
        data_absent = (
            obs.data_attempted
            and obs.data_nak_only
            and not obs.data_ack
            and not obs.data_timeout
        )
        if request_absent or data_absent:
            return Fate.NOT_COMPLETED
        return Fate.UNKNOWN

    request_absent = (
        obs.request_nak_only
        and not obs.request_ack
        and not obs.request_timeout
    )
    if request_absent:
        return Fate.NOT_COMPLETED
    return Fate.UNKNOWN


def oracle_fate(tx: Transaction) -> Fate:
    """Label that matches the research definitions, using ground truth only
    to decide whether a NOT_COMPLETED or COMPLETED label is *safe*.

    This is not the initiator's belief. Callers must not present it as one.
    UNKNOWN here means the initiator did not receive OKAY while a commit
    exists, so neither COMPLETED nor NOT_COMPLETED is a sound initiator label.
    NOT_COMPLETED means the effect count is zero, so that label would be safe
    if the initiator were allowed to use it.
    """

    if tx.axi_completed_ok and tx.ground_commit_count == 1:
        return Fate.COMPLETED
    if tx.ground_commit_count == 0 and not tx.axi_completed_ok:
        return Fate.NOT_COMPLETED
    if tx.ground_commit_count == 0 and tx.axi_completed_ok:
        # Initiator was told OKAY although nothing committed. The sound
        # initiator label under the definitions is not COMPLETED.
        return Fate.UNKNOWN
    if tx.ground_commit_count >= 1 and not tx.axi_completed_ok:
        return Fate.UNKNOWN
    if tx.ground_commit_count > 1:
        return Fate.UNKNOWN
    return Fate.UNKNOWN


def safe_to_retry(fate: Fate) -> bool:
    return fate == Fate.NOT_COMPLETED
