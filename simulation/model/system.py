"""Discrete-step AoU-over-UCIe system.

One step is one protocol event (accept, pack, transmit, deliver, commit,
replay, or a link-state transition). It is not a PHY cycle.

Protocol rules live in the methods that cite them. Fault decisions come from
the scenario and are applied only in ``fire``. Classification reads initiator
observations, never ``ground_commit_count``.
"""

from __future__ import annotations

from dataclasses import dataclass, field, replace

from simulation.model.aou import AouPair, fresh_pools
from simulation.model.axi import Transaction, classify, classify_with_standardized_signals, oracle_fate
from simulation.model.channel import apply_corruption
from simulation.model.signals import StdSignals
from simulation.model.target import Target
from simulation.model.types import (
    AouState,
    Direction,
    Fate,
    FaultKind,
    LinkState,
    Observation,
    OpKind,
    Phase,
    Policy,
    RequestType,
    UcieVersion,
    fault_bucket,
)
from simulation.model.ucie import AdapterConfig, AdapterPair, Flit, crc_detects
from simulation.verification.invariants import check_invariants


WAITING = {
    Phase.REQUEST_FLIT_TRANSMITTED,
    Phase.WDATA_FLIT_TRANSMITTED,
    Phase.WRITE_RESPONSE_TRANSMITTED,
}

ROLE_DIRECTION = {
    "request": Direction.A_TO_B,
    "wdata": Direction.A_TO_B,
    "response": Direction.B_TO_A,
}


@dataclass
class OpSpec:
    op: OpKind
    address: int
    axi_id: int = 0
    data: int = 1
    request: RequestType | None = None

    def resolved_request(self) -> RequestType:
        if self.request is not None:
            return self.request
        if self.op in {OpKind.MEMORY_READ, OpKind.POP_FIFO}:
            return RequestType.READ
        return RequestType.WRITE


@dataclass
class Scenario:
    name: str
    ops: list[OpSpec]
    fault: FaultKind = FaultKind.NONE
    at_phase: Phase = Phase.WRITE_RESPONSE_TRANSMITTED
    policy: Policy = Policy.DISCARD
    observation: Observation = Observation.FDI_AXI
    retrain_preserves_retry: bool = True
    early_bresp: bool = False
    credit_depth: int = 4
    replay_limit: int = 3
    logical_id_base: int = 1
    counter_start: int = 0
    fail_retrain: bool = False
    remote_recovers: bool = True
    activation_when: str = "none"  # initial | reactivation | none
    corrupt_bits: int = 1
    fault_tx: int = 0
    seed: int | None = None
    max_steps: int = 400
    # Schedule constraint for the reduced explorer, not a protocol rule.
    # When set, only the watched transaction moves until the fault fires.
    focus_watched: bool = False
    version: UcieVersion = UcieVersion.UCIE_2_0

    def behavior(self) -> "Behavior":
        return BEHAVIOR.get(self.fault, Behavior())


@dataclass
class Behavior:
    oneshot_bits: int | None = None
    persistent: bool = False
    fail_retrain: bool = False
    force: str | None = None
    # credit | timeout | activation are forces too
    bits: int = 1


# How each named fault acts. The phase at which it acts is on the scenario.
# AFTER_COMMIT and BEFORE_COMMIT differ only in the phase the caller selects;
# both are persistent CRC failures that also fail Retrain, which is the
# "retry cannot recover" path into LinkError.
BEHAVIOR: dict[FaultKind, Behavior] = {
    FaultKind.NONE: Behavior(),
    FaultKind.SINGLE_BIT: Behavior(oneshot_bits=1, bits=1),
    FaultKind.TWO_BIT: Behavior(oneshot_bits=2, bits=2),
    FaultKind.THREE_BIT: Behavior(oneshot_bits=3, bits=3),
    FaultKind.CRC_FAIL_REPLAY: Behavior(oneshot_bits=1, bits=1),
    FaultKind.REPEATED_CORRUPTION: Behavior(persistent=True, fail_retrain=False, bits=1),
    FaultKind.RETRY_TIMEOUT: Behavior(force="timeout"),
    FaultKind.RETRY_THRESHOLD: Behavior(persistent=True, fail_retrain=False, bits=1),
    FaultKind.FORCED_RETRAIN: Behavior(force="retrain"),
    FaultKind.FORCED_LINKRESET: Behavior(force="linkreset"),
    FaultKind.FORCED_LINKERROR: Behavior(force="linkerror"),
    FaultKind.UNILATERAL_RESET: Behavior(force="unilateral"),
    FaultKind.REMOTE_UNRESPONSIVE: Behavior(force="unresponsive"),
    FaultKind.SIDEBAND_TIMEOUT: Behavior(force="sideband"),
    FaultKind.DROPPED_RESPONSE: Behavior(force="drop_response"),
    FaultKind.DROPPED_REQUEST: Behavior(force="drop_request"),
    FaultKind.AFTER_COMMIT_BEFORE_RESPONSE: Behavior(
        persistent=True, fail_retrain=True, bits=1
    ),
    FaultKind.BEFORE_ARCHITECTURAL_COMMIT: Behavior(
        persistent=True, fail_retrain=True, bits=1
    ),
    FaultKind.CREDIT: Behavior(force="credit"),
    FaultKind.ACTIVATION: Behavior(force="activation"),
    FaultKind.DEACTIVATION: Behavior(force="deactivation"),
    FaultKind.REPEATED_DURING_RECOVERY: Behavior(
        persistent=True, fail_retrain=True, bits=1
    ),
    # UCIe 3.0 §4.1.5.2. The packet is opcode-only in this revision. The
    # mainband loss that follows is the same replay-exhaustion path as a
    # repeated CRC fault. The priority packet is not given a transaction encoding.
    FaultKind.PRIORITY_SIDEBAND: Behavior(persistent=True, fail_retrain=True, bits=1),
    FaultKind.EMERGENCY_SHUTDOWN: Behavior(force="emergency"),
    FaultKind.FAST_THROTTLE: Behavior(force="fast_throttle"),
    FaultKind.RUNTIME_RECAL: Behavior(force="recal"),
    FaultKind.RUNTIME_PARITY: Behavior(force="parity"),
    FaultKind.ENDPOINT_RESET: Behavior(force="endpoint_reset"),
}


@dataclass
class RunStats:
    fault_injections: int = 0
    transparent_retries: int = 0
    retrain_events: int = 0
    linkreset_events: int = 0
    linkerror_events: int = 0
    crc_failures: int = 0
    correctable_errors: int = 0
    uncorrectable_errors: int = 0


@dataclass
class RunResult:
    scenario: Scenario
    events: list[str]
    transactions: list[Transaction]
    violations: list[str]
    stats: RunStats
    link_converged: bool
    deadlock: bool
    counter_end: int
    bucket: str

    @property
    def converged(self) -> bool:
        return self.link_converged and not self.deadlock


@dataclass
class World:
    scenario: Scenario
    events: list[str] = field(default_factory=list)
    txs: list[Transaction] = field(default_factory=list)
    target: Target = field(default_factory=Target)
    adapters: AdapterPair = field(default_factory=AdapterPair)
    aou: AouPair = field(default_factory=AouPair)
    config: AdapterConfig = field(default_factory=AdapterConfig)
    stats: RunStats = field(default_factory=RunStats)
    violations: list[str] = field(default_factory=list)
    rr: int = 0
    bringup_count: int = 0
    up: bool = False
    recovery_epoch: int = 0
    fault_fired: bool = False
    persistent: bool = False
    fail_retrain: bool = False
    oneshot_remaining: int = 0
    corrupt_bits: int = 1
    corrupt_roles: set[str] = field(default_factory=set)
    suppress_ack: bool = False
    pending_timeout: Flit | None = None
    remote_recovers: bool = True
    fail_on_bringup: int | None = None
    deadlock: bool = False
    link_converged: bool = True
    pending_reissue: bool = False
    early_bresp_done: set[int] = field(default_factory=set)
    _auditing: bool = True
    signals: StdSignals = field(default_factory=StdSignals)
    fate_milestone: int = 0

    def spec(self) -> str:
        return self.scenario.version.value

    def is_ucie3(self) -> bool:
        return self.scenario.version == UcieVersion.UCIE_3_0

    def note_fate(self) -> None:
        """Count one shared recovery milestone. Notification text does not."""

        self.fate_milestone += 1

    def remember_commit(self, tx: Transaction) -> None:
        if tx.commit_milestone is None:
            tx.commit_milestone = self.fate_milestone

    def remember_completion(self, tx: Transaction) -> None:
        if tx.commit_milestone is not None and tx.fate_known_milestone is None:
            tx.fate_known_milestone = self.fate_milestone

    def remember_recovery(self) -> None:
        for tx in self.txs:
            if (
                tx.commit_milestone is not None
                and tx.fate_known_milestone is None
                and tx.recovery_milestone is None
            ):
                tx.recovery_milestone = self.fate_milestone

    def log(self, text: str) -> None:
        self.events.append(text)

    def flag(self, text: str) -> None:
        if text not in self.violations:
            self.violations.append(text)

    # ---------------------------------------------------------------- bringup

    def bringup(self) -> None:
        """Reset -> Active, then AoU activation, then full-depth credits.

        UCIe 2.0 §10.3.3.1: exit from Reset toward Active completes only after
        the Active entry handshake. The handshake itself is one event here.
        AoU traffic waits until activation completes. FDI link-down during
        activation cancels it (AoU 0.8 §8.3.3, as cited).
        """

        self.bringup_count += 1
        self.recovery_epoch = self.bringup_count - 1
        if self.fail_on_bringup is not None and self.bringup_count == self.fail_on_bringup:
            # ASSUMPTION: a dropped ActivateAck leaves AoU DISABLED. The spec
            # citation covers a link-down cancelling the handshake. A fault
            # that drops the ack while the UCIe link is up is an injected fault.
            self.log("Fault during activation: ActivateAck dropped")
            self.log("AoU remains DISABLED and does not exchange messages")
            self.aou.state_a = AouState.DISABLED
            self.aou.state_b = AouState.DISABLED
            self.adapters.both(LinkState.ACTIVE)
            self.up = False
            self.link_converged = False
            self.deadlock = True
            return
        if not self.remote_recovers and self.bringup_count > 1:
            self.log("Remote endpoint remains unresponsive during reactivation")
            self.adapters.state_a = LinkState.ACTIVE
            self.adapters.state_b = LinkState.LINKERROR
            self.aou.state_a = AouState.ENABLED
            self.aou.state_b = AouState.DISABLED
            self.up = False
            self.link_converged = False
            self.deadlock = True
            return

        self.adapters.both(LinkState.RESET)
        self.adapters.reset_sequence_handshake()
        self.adapters.retry = {Direction.A_TO_B: [], Direction.B_TO_A: []}
        self.adapters.channel = {Direction.A_TO_B: None, Direction.B_TO_A: None}
        self.adapters.both(LinkState.ACTIVE)
        self.log("UCIe link reaches Active")
        if self.bringup_count > 1:
            # Return to Active is a fate milestone. The first bring-up is not:
            # nothing has committed yet.
            self.note_fate()
            self.remember_recovery()
        # Sequence handshake is required on this entry (UCIe §3.8, retained in 3.0).
        self.log(
            "Sequence-number handshake completes "
            f"(timeout bound {128} flits, UCIe {self.spec()} §3.8)"
        )
        self.aou.state_a = AouState.ACTIVATE
        self.aou.state_b = AouState.ACTIVATE
        self.aou.state_a = AouState.ENABLED
        self.aou.state_b = AouState.ENABLED
        self.log("AoU activation handshake completes")
        self.aou.pools = fresh_pools(self.scenario.credit_depth, self.recovery_epoch)
        self.log(
            f"AoU credits initialized to depth {self.scenario.credit_depth} in each direction"
        )
        self.up = True
        self.link_converged = True

    # ---------------------------------------------------------------- credits

    def pool(self, direction: Direction):
        return self.aou.pools[direction]

    def audit(self, direction: Direction) -> None:
        if not self._auditing:
            return
        for problem in self.pool(direction).audit():
            self.flag(problem)

    def take_credit(self, direction: Direction) -> bool:
        pool = self.pool(direction)
        if pool.available <= 0:
            return False
        pool.available -= 1
        pool.held += 1
        self.audit(direction)
        return True

    def return_credit(self, direction: Direction) -> None:
        pool = self.pool(direction)
        if pool.held <= 0:
            self.flag("property 4: credit return with nothing held")
            return
        pool.held -= 1
        pool.occupancy = max(0, pool.occupancy - 1)
        pool.available += 1
        self.audit(direction)

    def note_receive_occupancy(self, direction: Direction) -> None:
        pool = self.pool(direction)
        pool.occupancy += 1
        self.audit(direction)

    # ---------------------------------------------------------------- tx setup

    def launch(self) -> None:
        for index, op in enumerate(self.scenario.ops):
            request = op.resolved_request()
            tx = Transaction(
                logical_id=self.scenario.logical_id_base + index,
                axi_id=op.axi_id,
                request=request,
                address=op.address,
                op=op.op,
                data=op.data,
                recovery_epoch=self.recovery_epoch,
            )
            self.txs.append(tx)

    def watched(self) -> Transaction | None:
        if not self.txs:
            return None
        return self.txs[min(self.scenario.fault_tx, len(self.txs) - 1)]

    def fault_due(self) -> bool:
        if self.fault_fired or self.scenario.fault == FaultKind.NONE:
            return False
        if self.scenario.fault == FaultKind.ACTIVATION and self.scenario.activation_when == "initial":
            return False
        tx = self.watched()
        if tx is None:
            return False
        return tx.phase.value >= self.scenario.at_phase.value

    # ---------------------------------------------------------------- faults

    def roles_for_phase(self, phase: Phase, tx: Transaction) -> set[str]:
        if phase.value <= Phase.REQUEST_FLIT_TRANSMITTED.value:
            return {"request"}
        if tx.is_write and phase.value <= Phase.WDATA_FLIT_TRANSMITTED.value:
            return {"wdata"}
        if phase.value >= Phase.WRITE_RESPONSE_CREATED.value:
            return {"response"}
        return set()

    def corrupt_inflight(self) -> None:
        for direction, flit in list(self.adapters.channel.items()):
            if flit is None or flit.role not in self.corrupt_roles:
                continue
            if not crc_detects(self.corrupt_bits):
                continue
            flit.crc_ok = False
            flit.corrupt_bits = self.corrupt_bits

    def fire(self) -> None:
        self.fault_fired = True
        self.stats.fault_injections += 1
        behavior = self.scenario.behavior()
        tx = self.watched()
        if tx is not None:
            tx.fault_phase = tx.phase
        self.fail_retrain = behavior.fail_retrain or self.scenario.fail_retrain
        self.corrupt_bits = behavior.oneshot_bits or behavior.bits or self.scenario.corrupt_bits
        if tx is not None:
            self.corrupt_roles = self.roles_for_phase(tx.phase, tx)
        else:
            self.corrupt_roles = set()

        if self.scenario.fault == FaultKind.PRIORITY_SIDEBAND:
            if not self._require_ucie3("priority sideband"):
                return
            self.signals.pspt_opcode_only = True
            self.log(
                "Priority sideband packet inserted "
                "(opcode 11111b, UCIe 3.0 §4.1.5.2). "
                "Further payload details are left to a future ECN; "
                "this packet is not an AXI completion"
            )

        if behavior.oneshot_bits:
            self.oneshot_remaining = 1
            self.corrupt_bits = behavior.oneshot_bits
            self.corrupt_inflight()
            if self.oneshot_remaining and self._inflight_was_marked():
                self.oneshot_remaining = 0
            weight = behavior.oneshot_bits
            self.log(
                f"{weight}-bit corruption injected in a UCIe flit "
                f"(detected under the UCIe {self.spec()} §3.7 3-bit CRC guarantee)"
            )
            return

        if behavior.persistent:
            self.persistent = True
            if not self.corrupt_roles:
                self.log(
                    "No flit is in flight at this phase; retry cannot express the fault"
                )
                self.enter_linkerror(
                    "Fault in the target window escalates to LinkError"
                )
                return
            self.corrupt_inflight()
            self.log("Repeated corruption armed against replay of this flit")
            return

        force = behavior.force
        if force == "timeout":
            self.suppress_ack = True
            self.log("ACK withheld so the replay timer can expire")
            return
        if force == "retrain":
            self.do_retrain(fail=self.fail_retrain)
            return
        if force == "linkreset":
            self.enter_linkreset()
            return
        if force == "linkerror":
            self.enter_linkerror("Forced LinkError")
            return
        if force == "unilateral":
            self.log("Unilateral initiator endpoint reset")
            self.enter_linkerror(
                "Sideband timeout drives the remote partner into LinkError "
                f"(UCIe {self.spec()} §10.3.3.8, {8} ms)"
            )
            return
        if force == "unresponsive":
            self.remote_recovers = False
            self.log("Remote endpoint becomes unresponsive")
            self.enter_linkerror(
                "No sideband response; local side enters LinkError "
                f"(UCIe {self.spec()} §10.3.3.8, {8} ms). "
                f"Minimum LinkError residency is {16} ms "
                f"(UCIe {self.spec()} §10.3.3.7)."
            )
            return
        if force == "sideband":
            self.log(
                "Sideband state-transition response times out "
                f"(UCIe {self.spec()} §10.3.3.8, {8} ms)"
            )
            self.enter_linkerror("Originator enters LinkError after the sideband timeout")
            return
        if force == "drop_response":
            self.log("Response dropped during destructive recovery")
            self.enter_linkerror("LinkError discards the in-flight response")
            return
        if force == "drop_request":
            self.log("Request dropped during destructive recovery")
            self.enter_linkerror("LinkError discards the in-flight request")
            return
        if force == "deactivation":
            self.log("AoU deactivation starts")
            self.aou.state_a = AouState.DEACTIVATE
            self.aou.state_b = AouState.DEACTIVATE
            self.log("Fault during deactivation aborts the drain")
            # UCIe 2.0 §10.3.3.5: entry must not wait forever for a drain.
            self.enter_linkerror("Deactivation fault escalates to LinkError")
            return
        if force == "credit":
            self.inject_credit_fault()
            return
        if force == "activation":
            # Reactivation case: fail the bringup that follows this LinkError.
            self.fail_on_bringup = self.bringup_count + 1
            self.enter_linkerror("LinkError, then activation of the recovered link fails")
            return
        if force == "emergency":
            self._emergency_shutdown()
            return
        if force == "fast_throttle":
            self._fast_throttle()
            return
        if force == "recal":
            self._runtime_recalibration()
            return
        if force == "parity":
            self._runtime_parity()
            return
        if force == "endpoint_reset":
            self._endpoint_reset()
            return

    def _require_ucie3(self, name: str) -> bool:
        if self.is_ucie3():
            return True
        self.flag(
            f"internal: {name} is not a UCIe 2.0 mechanism and was not applied"
        )
        return False

    def _emergency_shutdown(self) -> None:
        """UCIe 3.0 §8.4.2.2. Fatal. Exit is reset. No transaction log is defined."""

        if not self._require_ucie3("Emergency Shutdown"):
            return
        self.signals.emergency_shutdown = True
        self.signals.endpoint_reset = True
        self.note_fate()
        self.persistent = False
        self.oneshot_remaining = 0
        self.adapters.both(LinkState.RESET)
        self.log(
            "Emergency Shutdown asserted (UCIe 3.0 §8.4.2.2). "
            "The condition is fatal and can be left only by reset. "
            "No standardized outstanding-transaction log is defined"
        )
        self.log(
            "The model does not invent a best-effort drain. "
            "Any such drain would be an implementation policy, not a UCIe 3.0 rule"
        )
        self.log("Retry buffers and pipelines flushed")
        self.flush_datapath()
        self.aou.state_a = AouState.DISABLED
        self.aou.state_b = AouState.DISABLED
        self.log("AoU activation FSM returns to DISABLED")
        if any(tx.response_generated and not tx.axi_completed_ok for tx in self.txs):
            self.log("Response is discarded during cleanup")
        if any(tx.handed_to_ucie and not tx.request_ack for tx in self.txs):
            self.log("Unacknowledged request is discarded during cleanup")
        self.apply_transaction_policy()
        self.log("Link is reset and reactivated")
        for tx in self.txs:
            tx.recovery_epoch = self.recovery_epoch + 1
        self.bringup()
        for tx in self.txs:
            if not tx.axi_completed_ok and not tx.explicit_unknown:
                tx.stopped = True

    def _fast_throttle(self) -> None:
        """UCIe 3.0 §8.4.2.1. Visible. The spec does not drop mainband flits."""

        if not self._require_ucie3("Fast Throttle"):
            return
        self.signals.fast_throttle = True
        self.log(
            "Fast Throttle asserted (UCIe 3.0 §8.4.2.1). "
            "Logging covers the throttle event, not an AXI transaction. "
            "Mainband transfer is not defined to stop"
        )

    def _runtime_recalibration(self) -> None:
        """UCIe 3.0 §4.6. Mainband continues. TARR timeout is not LinkError."""

        if not self._require_ucie3("runtime recalibration"):
            return
        self.signals.runtime_recalibration = True
        self.log(
            "Runtime recalibration runs (UCIe 3.0 §4.6). "
            "Mainband data continues to be sampled. "
            "A TARR handshake timeout does not enter LinkError"
        )

    def _runtime_parity(self) -> None:
        """UCIe 3.0 §3.9. Correctable. Does not by itself invalidate the flit."""

        if not self._require_ucie3("runtime link-testing parity"):
            return
        self.signals.runtime_parity = True
        self.stats.correctable_errors += 1
        self.log(
            "Runtime link-testing parity error logged as correctable "
            "(UCIe 3.0 §3.9). This check does not declare the flit invalid"
        )

    def _endpoint_reset(self) -> None:
        """Reset reinitializes retry variables. It does not record commit."""

        self.signals.endpoint_reset = True
        self.log(
            f"Endpoint reset (UCIe {self.spec()} §3.8 reinitializes retry "
            "variables on Reset). No architectural-commit record is defined"
        )
        self.enter_linkerror(
            f"Sideband timeout drives the remote partner into LinkError "
            f"(UCIe {self.spec()} §10.3.3.8, {8} ms)"
        )

    def _inflight_was_marked(self) -> bool:
        return any(
            flit is not None and not flit.crc_ok
            for flit in self.adapters.channel.values()
        )

    def inject_credit_fault(self) -> None:
        """Silently create one credit. This is an injected fault, not a spec rule."""

        pool = self.pool(Direction.A_TO_B)
        pool.available += 1
        self.log("Fault during AoU credit handling: one credit created with no return")
        self.audit(Direction.A_TO_B)

    def should_corrupt(self, role: str) -> bool:
        if role not in self.corrupt_roles:
            return False
        if self.persistent:
            return True
        if self.oneshot_remaining > 0:
            self.oneshot_remaining -= 1
            return True
        return False

    # ---------------------------------------------------------------- link FSM

    def flush_datapath(self) -> None:
        """Drop retry buffers, the channel, and in-flight messages.

        UCIe §10.3.3.5 / §10.3.3.7, retained in Revision 3.0: pipelines and
        retry buffers are flushed on LinkReset and on LinkError cleanup.
        Credits are reinitialized at the next activation rather than returned
        one by one. That reinitialization is an epoch boundary, not a
        conservation failure inside an epoch.
        """

        self.note_fate()
        self._auditing = False
        self.adapters.channel = {Direction.A_TO_B: None, Direction.B_TO_A: None}
        self.adapters.retry = {Direction.A_TO_B: [], Direction.B_TO_A: []}
        self.adapters.reset_sequence_handshake()
        self.pending_timeout = None
        self._auditing = True

    def apply_transaction_policy(self) -> None:
        policy = self.scenario.policy
        for tx in self.txs:
            if tx.axi_completed_ok:
                continue
            if policy == Policy.PRESERVE_LOCAL and tx.response_at_local_aou:
                self.log(
                    f"T{tx.logical_id}: locally received completion preserved and delivered"
                )
                self.complete_axi(tx)
                continue
            if policy == Policy.EXPLICIT_UNKNOWN and tx.axi_accepted:
                tx.explicit_unknown = True
                tx.stopped = True
                self.log(
                    f"T{tx.logical_id}: explicit UNKNOWN returned to the software model"
                )
                continue
            if policy == Policy.AUTO_RETRY and tx.axi_accepted:
                self.pending_reissue = True
                tx.stopped = True
                continue
            if tx.axi_accepted:
                tx.stopped = True
                self.log(f"T{tx.logical_id}: discarded with no AXI completion")
            else:
                tx.stopped = True

    def enter_linkerror(self, reason: str) -> None:
        # UCIe §10.3.3.7 and §10.3.3.8, retained in Revision 3.0.
        self.note_fate()
        self.stats.linkerror_events += 1
        self.stats.uncorrectable_errors += 1
        self.persistent = False
        self.oneshot_remaining = 0
        self.adapters.both(LinkState.LINKERROR)
        self._latch_link_status()
        self.log("LinkError entered")
        self.log(reason)
        self.log("LinkError propagated toward the remote link partner")
        self.log(
            f"LinkError minimum residency ({16} ms, UCIe {self.spec()} §10.3.3.7) elapses "
            "so a partner that missed the sideband message also enters LinkError"
        )
        self._log_standardized_status()
        self.log("Retry buffers and pipelines flushed")
        self.flush_datapath()
        # AoU 0.8 §8.3.3 as cited: FDI link-down returns activation to DISABLED
        # and drops an in-flight Activate/Deactivate handshake. Retrain does not.
        self.aou.state_a = AouState.DISABLED
        self.aou.state_b = AouState.DISABLED
        self.log("AoU activation FSM returns to DISABLED")
        if any(tx.response_generated and not tx.axi_completed_ok for tx in self.txs):
            self.log("Response is discarded during cleanup")
        if any(tx.handed_to_ucie and not tx.request_ack for tx in self.txs):
            self.log("Unacknowledged request is discarded during cleanup")
        self.apply_transaction_policy()
        self.log("Link is reset and reactivated")
        for tx in self.txs:
            tx.recovery_epoch = self.recovery_epoch + 1
        self.bringup()
        for tx in self.txs:
            if not tx.axi_completed_ok and not tx.explicit_unknown:
                tx.stopped = True

    def _latch_link_status(self) -> None:
        """Registers from UCIe §9.5. Present in 2.0 and still present in 3.0.

        Latching them does not add an AXI identifier. Header Log 1 remains a
        sideband-mailbox header. The three commit-shaped flags stay false.
        """

        self.signals.linkerror = True
        self.signals.uncorrectable = True
        self.signals.remote_linkerror_sideband = True
        if self.stats.crc_failures:
            self.signals.crc_error = True
        if self.stats.retrain_events:
            self.signals.retrain = True

    def _log_standardized_status(self) -> None:
        """UCIe 3.0 notifications. They describe the link, not the write."""

        if not self.is_ucie3():
            return
        self.log(
            "UCIe 3.0 software-visible status latched "
            "(§9.5 correctable CRC/retrain bits, uncorrectable LinkError status, "
            "link event notification). No field carries an AXI identifier"
        )
        self.log(
            "UCIe 3.0 management transport (§8.1) is independent of the mainband "
            "protocol and has no architectural-commit record for this transaction"
        )
        self.log(
            "Link Health Monitor (§9.5.3.40) exposes link margin, not the "
            "outstanding AXI transaction"
        )
        if self.signals.pspt_opcode_only:
            self.log(
                "The priority sideband opcode does not identify the AXI transaction"
            )
        if self.signals.emergency_shutdown:
            self.log(
                "Emergency Shutdown remains a link-and-power indication without "
                "a committed-transaction bitmap"
            )

    def enter_linkreset(self) -> None:
        # UCIe §10.3.3.5, retained in 3.0. The §10.3.4 note still says the
        # adapter does not itself finish protocol transactions.
        self.note_fate()
        self.stats.linkreset_events += 1
        self.signals.linkreset = True
        self.persistent = False
        self.oneshot_remaining = 0
        self.adapters.both(LinkState.LINKRESET)
        self.log("LinkReset entered")
        self.log(
            "Best-effort retry drain does not delay LinkReset; "
            f"pipelines and retry buffers are then flushed (UCIe {self.spec()} §10.3.3.5)"
        )
        if self.is_ucie3():
            self.log(
                "UCIe 3.0 §10.3.4 still provides no adapter mechanism that "
                "guarantees quiescence of protocol transactions on LinkReset"
            )
        self.flush_datapath()
        self.aou.state_a = AouState.DISABLED
        self.aou.state_b = AouState.DISABLED
        self.log("AoU activation FSM returns to DISABLED")
        if any(tx.response_generated and not tx.axi_completed_ok for tx in self.txs):
            self.log("Response is discarded during LinkReset cleanup")
        self.apply_transaction_policy()
        self.log("LinkReset returns through Reset and the link is reactivated")
        self.adapters.both(LinkState.RESET)
        self.bringup()
        for tx in self.txs:
            if not tx.axi_completed_ok and not tx.explicit_unknown:
                tx.stopped = True

    def do_retrain(self, fail: bool) -> None:
        # UCIe §10.3.3.4, retained in 3.0. The CRC/parity threshold algorithm
        # is implementation specific; reaching it is the caller's decision.
        self.note_fate()
        self.stats.retrain_events += 1
        self.signals.retrain = True
        self.adapters.both(LinkState.RETRAIN)
        self.log("UCIe enters Retrain")
        # Cited AoU behavior: Retrain does not drop I_INT_FSM_IN_ACTIVE and
        # does not raise the FDI-link-down path. Outstanding AoU state is kept.
        self.log("AoU remains enabled across Retrain (FDI link-down is not signaled)")
        if fail or self.fail_retrain:
            self.log("Repeated errors during recovery")
            self.log("Recovery fails")
            self.enter_linkerror("Retrain did not restore the link")
            return
        self.log("Retrain completes and both adapters return to Active")
        self.log(
            f"Sequence-number handshake runs on the entry to Active (UCIe {self.spec()} §3.8)"
        )
        self.adapters.both(LinkState.ACTIVE)
        self.adapters.replay_without_progress = 0
        self.replay_without_progress_clear()
        if not self.scenario.retrain_preserves_retry:
            # Interpretation of §3.8 variable reinitialization, not a stated
            # payload-flush rule. See AdapterPair docstring.
            self.log(
                "Retry state is reinitialized on Retrain and unacknowledged flits are discarded"
            )
            self.adapters.retry = {Direction.A_TO_B: [], Direction.B_TO_A: []}
            self.adapters.channel = {Direction.A_TO_B: None, Direction.B_TO_A: None}
            self.persistent = False
            for tx in self.txs:
                if tx.phase in WAITING and not tx.axi_completed_ok:
                    tx.stopped = True
                    self.deadlock = True
                    self.log(
                        f"T{tx.logical_id}: initiator waits with no retry data and no LinkError"
                    )
            return
        # Preserving interpretation: corruption that was only a PHY problem
        # stops once Retrain succeeds, and the retry buffer is sent again.
        self.persistent = False
        self.oneshot_remaining = 0
        self.requeue_unacked()

    def replay_without_progress_clear(self) -> None:
        self.adapters.replay_without_progress = 0

    def requeue_unacked(self) -> None:
        for direction, buffered in self.adapters.retry.items():
            if buffered and self.adapters.channel[direction] is None:
                flit = buffered[0].replay_copy()
                if self.should_corrupt(flit.role):
                    flit.crc_ok = False
                    flit.corrupt_bits = self.corrupt_bits
                self.adapters.channel[direction] = flit

    # ---------------------------------------------------------------- flits

    def transmit(self, tx: Transaction, role: str) -> bool:
        direction = ROLE_DIRECTION[role]
        # UCIe 2.0 §3.8 MAX_UNACKNOWLEDGED_FLITS.
        if self.adapters.channel[direction] is not None:
            return False
        if len(self.adapters.retry[direction]) >= self.config.max_unacknowledged:
            return False
        seq = self.adapters.next_seq[direction]
        self.adapters.next_seq[direction] = (seq + 1) & 0xFF
        flit = Flit(seq=seq, role=role, direction=direction, tx_index=self.txs.index(tx))
        if self.should_corrupt(role):
            apply_corruption(flit, self.corrupt_bits)
        self.adapters.channel[direction] = flit
        self.adapters.retry[direction].append(flit)
        tx.handed_to_ucie = True
        if role == "wdata":
            tx.data_attempted = True
        return True

    def replay(self, flit: Flit) -> None:
        resent = flit.replay_copy()
        owner = self.txs[flit.tx_index]
        owner.ucie_replay_count += 1
        if self.should_corrupt(flit.role):
            resent.crc_ok = False
            resent.corrupt_bits = self.corrupt_bits
        self.adapters.channel[flit.direction] = resent

    def on_nak(self, flit: Flit) -> None:
        tx = self.txs[flit.tx_index]
        if flit.role == "request" and not tx.request_ack:
            tx.request_nak_only = True
        elif flit.role == "wdata" and not tx.data_ack:
            tx.data_nak_only = True
        elif flit.role == "response" and not tx.response_ack:
            tx.response_nak_only = True

    def on_ack(self, flit: Flit, first_accept: bool) -> None:
        tx = self.txs[flit.tx_index]
        if flit.role == "request":
            tx.request_ack = True
            tx.request_nak_only = False
        elif flit.role == "wdata":
            tx.data_ack = True
            tx.data_nak_only = False
        elif flit.role == "response":
            tx.response_ack = True
            tx.response_nak_only = False
        direction = flit.direction
        self.adapters.retry[direction] = [
            item for item in self.adapters.retry[direction] if item.seq != flit.seq
        ]
        del first_accept

    def on_crc_fail(self, flit: Flit) -> None:
        # UCIe §3.7, retained in 3.0: invalid flit, replay must be requested.
        # Not forwarded. A detected CRC error is not evidence of commit.
        self.note_fate()
        self.stats.crc_failures += 1
        self.stats.correctable_errors += 1
        self.signals.crc_error = True
        self.adapters.replay_without_progress += 1
        self.on_nak(flit)
        count = self.adapters.replay_without_progress
        limit = self.config.replay_limit
        if count == 1:
            bits = flit.corrupt_bits or self.corrupt_bits
            self.log(
                f"UCIe CRC failure on the {flit.role} flit "
                f"({bits}-bit corruption); replay requested"
            )
        elif count >= limit:
            self.log(
                f"UCIe persistent transmission errors occur ({count} CRC failures)"
            )
            self.log("Retry threshold exceeded")
            # UCIe §3.8, retained in 3.0: a condition that sends PCIe to
            # Recovery makes the adapter initiate Retrain.
            self.do_retrain(fail=self.fail_retrain)
            return
        else:
            self.log(f"Replay {count} of the {flit.role} flit fails CRC")
        self.replay(flit)

    def forward(self, flit: Flit) -> None:
        tx = self.txs[flit.tx_index]
        self.note_receive_occupancy(flit.direction)
        # AoU credit tracks the message, not the UCIe ACK. Return the credit
        # when the far side accepts the message. A lost UCIe ACK must not
        # create or destroy that credit, and a replay must not return it twice.
        self.return_credit(flit.direction)
        if flit.role == "request":
            tx.phase = Phase.REQUEST_RECEIVED_REMOTELY
            if tx.is_write:
                self.log("Remote endpoint receives write request")
            else:
                self.log("Remote endpoint receives read request")
        elif flit.role == "wdata":
            tx.phase = Phase.WRITE_DATA_RECEIVED_REMOTELY
            self.log("Remote endpoint receives write data")
        elif flit.role == "response":
            tx.phase = Phase.WRITE_RESPONSE_RECEIVED
            tx.response_at_local_aou = True
            self.note_fate()
            if tx.is_write:
                self.log("Write response received by the initiating AoU endpoint")
            else:
                self.log("Read response received by the initiating AoU endpoint")

    def deliver(self, direction: Direction) -> bool:
        flit = self.adapters.channel[direction]
        if flit is None:
            return False
        self.adapters.channel[direction] = None
        if not flit.crc_ok:
            if not crc_detects(flit.corrupt_bits or self.corrupt_bits):
                # Outside the requested fault set. Fail closed by not delivering.
                self.flag("internal: corrupt flit was not covered by the CRC guarantee")
            self.on_crc_fail(flit)
            return True

        expected = self.adapters.expected_seq[direction]
        if flit.seq != expected:
            if flit.seq < expected or self._already_seen(flit):
                # Already accepted. UCIe/PCIe retry: ACK, do not forward again.
                self.stats.transparent_retries += 1
                self.log(
                    f"Duplicate {flit.role} flit suppressed; replay is not delivered twice"
                )
                self.on_ack(flit, first_accept=False)
                return True
            # UCIe 2.0 §3.8: a bad explicit sequence number is an uncorrectable
            # internal error and is not a PCIe Data Link Protocol Error.
            self.log("Invalid flit sequence number; uncorrectable internal error")
            self.enter_linkerror("Sequence-number failure escalates to LinkError")
            return True

        self.adapters.expected_seq[direction] = (expected + 1) & 0xFF
        if self.adapters.replay_without_progress:
            self.stats.transparent_retries += 1
            self.log("Replay succeeds; the CRC error stays correctable and is not an AoU error")
            self.adapters.replay_without_progress = 0

        if self.suppress_ack:
            # Good flit forwarded, ACK not sent. A later replay must not commit again.
            self.forward(flit)
            self.suppress_ack = False
            self.pending_timeout = flit
            self.log("CRC-good flit forwarded, but its ACK is not transmitted")
            return True

        self.forward(flit)
        self.on_ack(flit, first_accept=True)
        return True

    def _already_seen(self, flit: Flit) -> bool:
        tx = self.txs[flit.tx_index]
        if flit.role == "request":
            return tx.phase.value > Phase.REQUEST_FLIT_TRANSMITTED.value
        if flit.role == "wdata":
            return tx.phase.value > Phase.WDATA_FLIT_TRANSMITTED.value
        if flit.role == "response":
            return tx.phase.value > Phase.WRITE_RESPONSE_TRANSMITTED.value
        return False

    def apply_timeout(self) -> bool:
        flit = self.pending_timeout
        self.pending_timeout = None
        if flit is None:
            return False
        # UCIe 2.0 §3.8: REPLAY_TIMEOUT_FLIT_COUNT >= 375 schedules replay and
        # is logged as a correctable internal error. Time is one event.
        self.stats.correctable_errors += 1
        self.log(
            "Replay timeout reached "
            f"(REPLAY_TIMEOUT_FLIT_COUNT >= {375}, UCIe {self.spec()} §3.8)"
        )
        tx = self.txs[flit.tx_index]
        if flit.role == "request":
            tx.request_timeout = True
        elif flit.role == "wdata":
            tx.data_timeout = True
        elif flit.role == "response":
            tx.response_timeout = True
        self.replay(flit)
        return True

    # ---------------------------------------------------------------- phases

    def response_noun(self, tx: Transaction) -> str:
        return "Write response" if tx.is_write else "Read response"

    def complete_axi(self, tx: Transaction) -> None:
        if tx.axi_completed_ok:
            tx.duplicate_completion = True
            self.flag(
                f"property 7: duplicate successful completion of T{tx.logical_id}"
            )
            return
        tx.axi_completed_ok = True
        tx.requester_received_response = True
        tx.phase = Phase.TRANSACTION_COMPLETED
        tx.stopped = True
        self.remember_completion(tx)
        self.log(f"T{tx.logical_id}: initiator receives completion")

    def local_step(self, tx: Transaction) -> bool:
        if not self.up or not self.aou.enabled():
            return False
        phase = tx.phase
        if phase == Phase.CREATED:
            tx.axi_accepted = True
            tx.phase = Phase.REQUEST_ACCEPTED_LOCALLY
            self.log("AXI request accepted")
            if self.scenario.early_bresp and tx.is_write and tx.logical_id not in self.early_bresp_done:
                # Implementation feature of some AoU bridges, not baseline
                # AoU 0.8 behavior. Off unless the scenario enables it.
                self.early_bresp_done.add(tx.logical_id)
                self.log(
                    "Early write response OKAY returned before the remote commit "
                    "(implementation-specific; not an AoU 0.8 baseline rule)"
                )
                self.complete_axi(tx)
            return True
        if phase == Phase.REQUEST_ACCEPTED_LOCALLY:
            if not self.take_credit(Direction.A_TO_B):
                return False
            tx.phase = Phase.AOU_REQUEST_PACKED
            self.log("AoU request packed")
            return True
        if phase == Phase.AOU_REQUEST_PACKED:
            if not self.transmit(tx, "request"):
                return False
            tx.phase = Phase.REQUEST_FLIT_TRANSMITTED
            self.log("AoU request sent")
            return True
        if phase == Phase.REQUEST_RECEIVED_REMOTELY:
            if tx.is_write:
                tx.phase = Phase.WRITE_DATA_ACCEPTED_LOCALLY
                self.log("AXI write data accepted locally")
            else:
                tx.phase = Phase.TARGET_ACCEPTED
                tx.target_accepted = True
                self.log("Target accepts read")
            return True
        if phase == Phase.WRITE_DATA_ACCEPTED_LOCALLY:
            if not self.take_credit(Direction.A_TO_B):
                return False
            tx.phase = Phase.AOU_WDATA_PACKED
            self.log("AoU write data packed")
            return True
        if phase == Phase.AOU_WDATA_PACKED:
            if not self.transmit(tx, "wdata"):
                return False
            tx.phase = Phase.WDATA_FLIT_TRANSMITTED
            self.log("AoU write data sent")
            return True
        if phase == Phase.WRITE_DATA_RECEIVED_REMOTELY:
            tx.phase = Phase.TARGET_ACCEPTED
            tx.target_accepted = True
            self.log("Target accepts write")
            return True
        if phase == Phase.TARGET_ACCEPTED:
            message = self.target.commit(tx)
            tx.phase = Phase.ARCHITECTURAL_COMMIT
            tx.effect_log.append(message)
            self.remember_commit(tx)
            self.log("Target commits operation")
            self.log(message)
            return True
        if phase == Phase.ARCHITECTURAL_COMMIT:
            tx.response_generated = True
            tx.phase = Phase.WRITE_RESPONSE_CREATED
            self.note_fate()
            self.log(f"{self.response_noun(tx)} generated")
            return True
        if phase == Phase.WRITE_RESPONSE_CREATED:
            if not self.take_credit(Direction.B_TO_A):
                return False
            tx.phase = Phase.AOU_RESPONSE_PACKED
            self.log("AoU response packed")
            return True
        if phase == Phase.AOU_RESPONSE_PACKED:
            if not self.transmit(tx, "response"):
                return False
            tx.phase = Phase.WRITE_RESPONSE_TRANSMITTED
            self.note_fate()
            self.log(f"{self.response_noun(tx)} transmitted")
            return True
        if phase == Phase.WRITE_RESPONSE_RECEIVED:
            if tx.axi_completed_ok:
                tx.stopped = True
                return False
            self.complete_axi(tx)
            return True
        return False

    def advance(self) -> bool:
        if self.pending_timeout is not None:
            return self.apply_timeout()
        for direction in (Direction.A_TO_B, Direction.B_TO_A):
            if self.adapters.channel[direction] is not None:
                return self.deliver(direction)
        count = len(self.txs)
        if count == 0:
            return False
        watched = self.watched()
        watched_open = (
            self.scenario.focus_watched
            and watched is not None
            and not (watched.stopped or watched.axi_completed_ok or watched.explicit_unknown)
        )
        if watched_open:
            order = [watched]
        else:
            order = [self.txs[(self.rr + offset) % count] for offset in range(count)]
        for offset, tx in enumerate(order):
            if tx is None or tx.stopped or tx.axi_completed_ok or tx.explicit_unknown:
                continue
            if tx.phase in WAITING:
                continue
            if self.local_step(tx):
                if not watched_open:
                    self.rr = (self.rr + offset + 1) % count
                return True
        return False

    def all_terminal(self) -> bool:
        if not self.txs:
            return True
        return all(
            tx.stopped or tx.axi_completed_ok or tx.explicit_unknown for tx in self.txs
        )

    def drive(self, limit: int) -> None:
        steps = 0
        while steps < limit:
            steps += 1
            if self.fault_due():
                self.fire()
            if self.all_terminal():
                return
            if not self.up:
                self.deadlock = True
                self.log("No forward progress: AoU is not enabled")
                return
            if not self.advance():
                self.deadlock = True
                self.link_converged = self.adapters.compatible() and self.up
                self.log("No forward progress")
                return
        self.deadlock = True
        self.log("Step budget exhausted")

    def reissue_outstanding(self) -> None:
        if not self.up:
            self.log("Automatic retry skipped because the link did not return to Active")
            self.deadlock = True
            return
        for tx in self.txs:
            if tx.axi_completed_ok or tx.explicit_unknown or not tx.axi_accepted:
                continue
            tx.commit_at_reissue = tx.ground_commit_count
            if tx.commit_at_reissue:
                self.log(
                    f"T{tx.logical_id}: automatic retry issued while "
                    f"architectural_commit_count is already {tx.commit_at_reissue}"
                )
            else:
                self.log(
                    f"T{tx.logical_id}: automatic retry issued after recovery"
                )
            tx.software_retry_count += 1
            tx.stopped = False
            tx.phase = Phase.CREATED
            tx.axi_accepted = False
            tx.handed_to_ucie = False
            tx.request_ack = False
            tx.data_ack = False
            tx.response_ack = False
            tx.request_nak_only = False
            tx.data_nak_only = False
            tx.response_nak_only = False
            tx.data_attempted = False
            tx.request_timeout = False
            tx.data_timeout = False
            tx.response_timeout = False
            tx.response_at_local_aou = False
            tx.response_generated = False
            tx.target_accepted = False
            # ground_commit_count is intentionally kept.
        self.pending_reissue = False
        self.drive(self.scenario.max_steps)

    def _label(self, tx: Transaction) -> None:
        tx.signals = replace(self.signals)
        fdi_obs = tx.observation(False)
        tx.fdi_class = classify(fdi_obs)
        # Profiles 2 and 3 read the latched signals and still get the FDI label.
        tx.software_class = classify_with_standardized_signals(fdi_obs, tx.signals)
        tx.management_class = classify_with_standardized_signals(fdi_obs, tx.signals)
        tx.disposition_class = classify(tx.observation(True))
        observation = self.scenario.observation
        if observation == Observation.RETRY_DISPOSITION:
            tx.initiator_class = tx.disposition_class
        elif observation == Observation.SOFTWARE_ERROR:
            tx.initiator_class = tx.software_class
        elif observation == Observation.MANAGEMENT:
            tx.initiator_class = tx.management_class
        else:
            tx.initiator_class = tx.fdi_class
        tx.oracle_class = oracle_fate(tx)
        if tx.commit_milestone is None or tx.ground_commit_count == 0:
            tx.ambiguity_open = False
            tx.ambiguity_window = None
        elif (
            tx.requester_received_response
            and tx.fate_known_milestone is not None
            and tx.software_retry_count == 0
            and tx.ground_commit_count == 1
        ):
            tx.ambiguity_open = False
            tx.ambiguity_window = tx.fate_known_milestone - tx.commit_milestone
        else:
            # Still open if the only later OKAY came from an automatic retry.
            # That OKAY is a second execution, not knowledge of the first result.
            tx.ambiguity_open = True
            end = (
                tx.recovery_milestone
                if tx.recovery_milestone is not None
                else self.fate_milestone
            )
            tx.ambiguity_window = end - tx.commit_milestone

    def finish(self) -> RunResult:
        for tx in self.txs:
            self._label(tx)
            if (
                tx.initiator_class == Fate.COMPLETED
                and not tx.requester_received_response
                and not tx.axi_completed_ok
            ):
                self.flag("internal: COMPLETED without an initiator completion")
        if self.pending_reissue:
            self.reissue_outstanding()
            for tx in self.txs:
                self._label(tx)

        if not self.txs:
            self.log("Initiator has no transaction record")
        else:
            for tx in self.txs:
                if not tx.axi_completed_ok:
                    self.log(f"T{tx.logical_id}: initiator has no completion record")

        if self.adapters.state_a != self.adapters.state_b or not self.link_converged:
            self.link_converged = False
            self.flag(
                "property 6: endpoints did not reach compatible operational states"
            )
        if self.deadlock:
            self.flag("property 6: liveness failure (deadlock or no forward progress)")

        self.violations.extend(
            item
            for item in check_invariants(self.txs, self.scenario.policy)
            if item not in self.violations
        )
        watched = self.watched()
        phase = watched.fault_phase if watched and watched.fault_phase else self.scenario.at_phase
        return RunResult(
            scenario=self.scenario,
            events=list(self.events),
            transactions=self.txs,
            violations=list(self.violations),
            stats=self.stats,
            link_converged=self.link_converged and self.adapters.compatible(),
            deadlock=self.deadlock,
            counter_end=self.target.counter,
            bucket=fault_bucket(phase) if self.scenario.fault != FaultKind.NONE else "none",
        )


def run_scenario(scenario: Scenario) -> RunResult:
    world = World(
        scenario=scenario,
        target=Target(counter=scenario.counter_start),
        config=AdapterConfig(
            replay_limit=scenario.replay_limit,
            retrain_preserves_retry=scenario.retrain_preserves_retry,
            retry_buffer_depth=max(8, scenario.credit_depth),
        ),
        remote_recovers=scenario.remote_recovers,
    )
    if (
        scenario.fault == FaultKind.ACTIVATION
        and scenario.activation_when == "initial"
    ):
        world.fail_on_bringup = 1
    world.bringup()
    world.launch()
    if not world.up:
        if scenario.fault != FaultKind.NONE:
            world.stats.fault_injections += 1
        for tx in world.txs:
            tx.stopped = True
            tx.fault_phase = Phase.CREATED
        return world.finish()
    world.drive(scenario.max_steps)
    return world.finish()


def with_observation(scenario: Scenario, observation: Observation) -> RunResult:
    """Replay is not required: reclassify a finished run's transactions.

    The datapath does not depend on the observation mode. Re-running keeps
    the event trace identical for a pure observation change; we still re-run
    so each result owns its scenario record.
    """

    return run_scenario(replace(scenario, observation=observation))
