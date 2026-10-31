"""Concrete scenarios, including the two milestone traces."""

from __future__ import annotations

from dataclasses import replace

from simulation.faults.injector import CANONICAL_PHASE, flit_phases
from simulation.model.system import OpSpec, Scenario
from simulation.model.types import (
    FaultKind,
    Observation,
    OpKind,
    Phase,
    Policy,
    RequestType,
    UcieVersion,
)


def mmio_increment(address: int = 0x1000, axi_id: int = 0) -> OpSpec:
    return OpSpec(op=OpKind.INCREMENT_COUNTER, address=address, axi_id=axi_id, data=1)


def milestone_unknown() -> Scenario:
    """Commit, then lose the write response in LinkError."""

    return Scenario(
        name="milestone_unknown_after_commit",
        ops=[mmio_increment()],
        fault=FaultKind.AFTER_COMMIT_BEFORE_RESPONSE,
        at_phase=Phase.WRITE_RESPONSE_TRANSMITTED,
        policy=Policy.DISCARD,
        observation=Observation.FDI_AXI,
        logical_id_base=17,
        counter_start=7,
    )


def milestone_before_acceptance() -> Scenario:
    """Corrupt the request until LinkError, before the target can accept it.

    The printed initiator classification uses retained retry disposition,
    which is an assumption. The FDI-only classification is also recorded.
    """

    return Scenario(
        name="milestone_not_completed_before_acceptance",
        ops=[mmio_increment()],
        fault=FaultKind.BEFORE_ARCHITECTURAL_COMMIT,
        at_phase=Phase.REQUEST_FLIT_TRANSMITTED,
        policy=Policy.DISCARD,
        observation=Observation.RETRY_DISPOSITION,
        logical_id_base=18,
        counter_start=7,
    )


def _base_write(name: str, fault: FaultKind, phase: Phase, **kwargs) -> Scenario:
    fields = {
        "name": name,
        "ops": [mmio_increment()],
        "fault": fault,
        "at_phase": phase,
        "counter_start": 0,
    }
    fields.update(kwargs)
    return Scenario(**fields)


def canonical_scenarios() -> list[Scenario]:
    scenarios = []
    for fault, phase in CANONICAL_PHASE.items():
        scenarios.append(
            _base_write(
                name=f"canonical_{fault.value}",
                fault=fault,
                phase=phase,
                activation_when="initial" if fault == FaultKind.ACTIVATION else "none",
            )
        )
    scenarios.append(
        _base_write(
            name="canonical_activation_during_reactivation",
            fault=FaultKind.ACTIVATION,
            phase=Phase.WRITE_RESPONSE_TRANSMITTED,
            activation_when="reactivation",
        )
    )
    return scenarios


def phase_sweep() -> list[Scenario]:
    """Forced LinkError between every pair of phases, plus flit-level faults."""

    scenarios: list[Scenario] = []
    for phase in Phase:
        scenarios.append(
            _base_write(
                name=f"linkerror_at_{phase.name}",
                fault=FaultKind.FORCED_LINKERROR,
                phase=phase,
            )
        )
    for phase in flit_phases():
        for fault in (
            FaultKind.SINGLE_BIT,
            FaultKind.TWO_BIT,
            FaultKind.THREE_BIT,
            FaultKind.REPEATED_CORRUPTION,
            FaultKind.RETRY_THRESHOLD,
            FaultKind.REPEATED_DURING_RECOVERY,
            FaultKind.RETRY_TIMEOUT,
        ):
            scenarios.append(
                _base_write(
                    name=f"{fault.value}_at_{phase.name}",
                    fault=fault,
                    phase=phase,
                )
            )
    return scenarios


def policy_matrix() -> list[Scenario]:
    """Same two traces under each recovery policy."""

    scenarios: list[Scenario] = []
    for policy in Policy:
        scenarios.append(replace(milestone_unknown(), name=f"policy_{policy.name}_unknown", policy=policy))
        scenarios.append(
            replace(
                milestone_before_acceptance(),
                name=f"policy_{policy.name}_before",
                policy=policy,
                # Keep FDI as the software-visible label in the matrix so
                # policy differences are not confused with the disposition flag.
                # The before-acceptance milestone itself still uses disposition.
                observation=Observation.FDI_AXI,
            )
        )
    return scenarios


def interpretation_matrix() -> list[Scenario]:
    """Retrain buffer retention is an interpretation, not a stated flush rule."""

    base = _base_write(
        name="retrain_preserve",
        fault=FaultKind.FORCED_RETRAIN,
        phase=Phase.WRITE_RESPONSE_TRANSMITTED,
        retrain_preserves_retry=True,
    )
    dropped = replace(
        base,
        name="retrain_reinitialize",
        retrain_preserves_retry=False,
    )
    threshold = _base_write(
        name="threshold_then_retrain_preserves",
        fault=FaultKind.RETRY_THRESHOLD,
        phase=Phase.WRITE_RESPONSE_TRANSMITTED,
        retrain_preserves_retry=True,
    )
    threshold_drop = replace(
        threshold,
        name="threshold_then_retrain_reinitializes",
        retrain_preserves_retry=False,
    )
    # Response has reached the initiating AoU and has not been handed to AXI.
    local_discard = _base_write(
        name="local_rx_discard",
        fault=FaultKind.FORCED_LINKERROR,
        phase=Phase.WRITE_RESPONSE_RECEIVED,
        policy=Policy.DISCARD,
    )
    local_preserve = replace(
        local_discard,
        name="local_rx_preserve",
        policy=Policy.PRESERVE_LOCAL,
    )
    return [base, dropped, threshold, threshold_drop, local_discard, local_preserve]


def device_matrix() -> list[Scenario]:
    """Every non-idempotent example, plus a plain memory write, lost after commit."""

    ops = [
        OpSpec(OpKind.MEMORY_WRITE, 0x2000, data=5),
        OpSpec(OpKind.MEMORY_READ, 0x2000, request=RequestType.READ),
        OpSpec(OpKind.INCREMENT_COUNTER, 0x1000),
        OpSpec(OpKind.PUSH_FIFO, 0x1004, data=3),
        OpSpec(OpKind.TRIGGER_DMA, 0x1008),
        OpSpec(OpKind.START_ACCELERATOR, 0x100C),
        OpSpec(OpKind.RING_DOORBELL, 0x1010),
        OpSpec(OpKind.ACK_INTERRUPT, 0x1014),
        OpSpec(OpKind.INCREMENT_ACTUATOR, 0x1018),
        OpSpec(OpKind.POP_FIFO, 0x101C, request=RequestType.READ),
    ]
    scenarios = []
    for op in ops:
        scenarios.append(
            Scenario(
                name=f"lost_response_{op.op.value}",
                ops=[op],
                fault=FaultKind.AFTER_COMMIT_BEFORE_RESPONSE,
                at_phase=Phase.WRITE_RESPONSE_TRANSMITTED,
            )
        )
    scenarios.append(
        Scenario(
            name="two_outstanding_peer_not_started",
            ops=[
                mmio_increment(0x1000, axi_id=1),
                OpSpec(OpKind.PUSH_FIFO, 0x1004, axi_id=2, data=9),
            ],
            fault=FaultKind.AFTER_COMMIT_BEFORE_RESPONSE,
            at_phase=Phase.WRITE_RESPONSE_TRANSMITTED,
            fault_tx=0,
            focus_watched=True,
        )
    )
    scenarios.append(
        Scenario(
            name="early_bresp_before_commit",
            ops=[mmio_increment()],
            fault=FaultKind.NONE,
            early_bresp=True,
        )
    )
    return scenarios


def ucie3_only_suite() -> list[Scenario]:
    """Faults that exist only because UCIe 3.0 defines the mechanism.

    These are not part of the equivalent 2.0 workload. Fast Throttle, runtime
    recalibration, and runtime parity are specified not to drop mainband
    traffic by themselves. Emergency Shutdown, priority sideband, and endpoint
    reset are destructive in this model only where the specification requires
    a reset or where a concurrent mainband CRC fault is the injected loss.
    """

    version = UcieVersion.UCIE_3_0

    def one(name: str, fault: FaultKind, phase: Phase, **kwargs) -> Scenario:
        return _base_write(
            name,
            fault,
            phase,
            version=version,
            counter_start=7,
            logical_id_base=42,
            **kwargs,
        )

    return [
        one(
            "ucie3_emergency_after_commit",
            FaultKind.EMERGENCY_SHUTDOWN,
            Phase.WRITE_RESPONSE_TRANSMITTED,
        ),
        one(
            "ucie3_emergency_before_acceptance",
            FaultKind.EMERGENCY_SHUTDOWN,
            Phase.REQUEST_FLIT_TRANSMITTED,
        ),
        one(
            "ucie3_pspt_after_commit",
            FaultKind.PRIORITY_SIDEBAND,
            Phase.WRITE_RESPONSE_TRANSMITTED,
        ),
        one(
            "ucie3_pspt_before_acceptance",
            FaultKind.PRIORITY_SIDEBAND,
            Phase.REQUEST_FLIT_TRANSMITTED,
        ),
        one(
            "ucie3_fast_throttle_during_response",
            FaultKind.FAST_THROTTLE,
            Phase.WRITE_RESPONSE_TRANSMITTED,
        ),
        one(
            "ucie3_recalibration_during_response",
            FaultKind.RUNTIME_RECAL,
            Phase.WRITE_RESPONSE_TRANSMITTED,
        ),
        one(
            "ucie3_parity_during_response",
            FaultKind.RUNTIME_PARITY,
            Phase.WRITE_RESPONSE_TRANSMITTED,
        ),
        one(
            "ucie3_endpoint_reset_after_commit",
            FaultKind.ENDPOINT_RESET,
            Phase.WRITE_RESPONSE_TRANSMITTED,
        ),
        one(
            "ucie3_endpoint_reset_before_acceptance",
            FaultKind.ENDPOINT_RESET,
            Phase.REQUEST_FLIT_TRANSMITTED,
        ),
        Scenario(
            name="ucie3_two_guests_shared_link",
            ops=[
                mmio_increment(0x1000, axi_id=1),
                mmio_increment(0x1004, axi_id=2),
            ],
            fault=FaultKind.FORCED_LINKERROR,
            at_phase=Phase.WRITE_RESPONSE_TRANSMITTED,
            fault_tx=0,
            version=version,
            counter_start=7,
            logical_id_base=70,
            focus_watched=False,
        ),
    ]


def exploration_suite() -> list[Scenario]:
    # Policy matrix replays the milestones; keep them in the suite once.
    return (
        [milestone_unknown(), milestone_before_acceptance()]
        + canonical_scenarios()
        + phase_sweep()
        + policy_matrix()
        + interpretation_matrix()
        + device_matrix()
    )
