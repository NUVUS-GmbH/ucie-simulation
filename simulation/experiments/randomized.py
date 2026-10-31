"""Seeded random fault injection.

``random.Random(seed)`` is the only randomness. Two calls with the same
seed and the same ``count`` return the same classifications and event traces.
"""

from __future__ import annotations

import random

from simulation.model.system import OpSpec, RunResult, Scenario, run_scenario
from simulation.model.types import (
    BUCKET_ORDER,
    READ_SKIPPED_PHASES,
    FaultKind,
    OpKind,
    Phase,
    RequestType,
    UcieVersion,
    fault_bucket,
)

# Writes and reads the target actually implements. Memory ops are idempotent
# in final state; the others are not.
_WRITE_OPS = [
    OpKind.MEMORY_WRITE,
    OpKind.INCREMENT_COUNTER,
    OpKind.PUSH_FIFO,
    OpKind.TRIGGER_DMA,
    OpKind.START_ACCELERATOR,
    OpKind.RING_DOORBELL,
    OpKind.ACK_INTERRUPT,
    OpKind.INCREMENT_ACTUATOR,
]
_READ_OPS = [OpKind.MEMORY_READ, OpKind.POP_FIFO]

_FAULTS = [
    FaultKind.SINGLE_BIT,
    FaultKind.SINGLE_BIT,
    FaultKind.TWO_BIT,
    FaultKind.THREE_BIT,
    FaultKind.CRC_FAIL_REPLAY,
    FaultKind.RETRY_TIMEOUT,
    FaultKind.REPEATED_CORRUPTION,
    FaultKind.RETRY_THRESHOLD,
    FaultKind.FORCED_RETRAIN,
    FaultKind.FORCED_LINKRESET,
    FaultKind.FORCED_LINKERROR,
    FaultKind.FORCED_LINKERROR,
    FaultKind.UNILATERAL_RESET,
    FaultKind.SIDEBAND_TIMEOUT,
    FaultKind.DROPPED_RESPONSE,
    FaultKind.DROPPED_REQUEST,
    FaultKind.AFTER_COMMIT_BEFORE_RESPONSE,
    FaultKind.AFTER_COMMIT_BEFORE_RESPONSE,
    FaultKind.BEFORE_ARCHITECTURAL_COMMIT,
    FaultKind.DEACTIVATION,
    FaultKind.CREDIT,
    FaultKind.REPEATED_DURING_RECOVERY,
    FaultKind.REMOTE_UNRESPONSIVE,
]


def _op(rng: random.Random, axi_id: int) -> OpSpec:
    if rng.random() < 0.75:
        kind = rng.choice(_WRITE_OPS)
        return OpSpec(
            op=kind,
            address=rng.choice([0x1000, 0x1004, 0x1008, 0x2000, 0x2004]),
            axi_id=axi_id,
            data=rng.randint(1, 9),
            request=RequestType.WRITE,
        )
    kind = rng.choice(_READ_OPS)
    return OpSpec(
        op=kind,
        address=0x101C if kind == OpKind.POP_FIFO else 0x2000,
        axi_id=axi_id,
        request=RequestType.READ,
    )


def _phase_for(op: OpSpec, bucket: str, rng: random.Random) -> Phase:
    phases = [phase for phase in Phase if fault_bucket(phase) == bucket]
    if op.resolved_request() == RequestType.READ:
        phases = [phase for phase in phases if phase not in READ_SKIPPED_PHASES]
    if not phases:
        return Phase.REQUEST_FLIT_TRANSMITTED
    return rng.choice(phases)


def build_random_suite(seed: int, count: int) -> list[Scenario]:
    rng = random.Random(seed)
    scenarios: list[Scenario] = []
    for index in range(count):
        # Draw a private stream so scenario i depends only on `seed` and i.
        child = random.Random(rng.randrange(1 << 63))
        n_ops = 1 if child.random() < 0.7 else 2
        ops = [_op(child, axi_id=i) for i in range(n_ops)]
        bucket = child.choice(BUCKET_ORDER)
        phase = _phase_for(ops[0], bucket, child)
        fault = child.choice(_FAULTS)
        scenarios.append(
            Scenario(
                name=f"random_{seed}_{index}",
                ops=ops,
                fault=fault,
                at_phase=phase,
                logical_id_base=index + 1,
                seed=child.randrange(1 << 63),
                counter_start=child.randint(0, 4),
                remote_recovers=fault != FaultKind.REMOTE_UNRESPONSIVE,
            )
        )
    return scenarios


def run_randomized(
    seed: int,
    count: int,
    version: UcieVersion = UcieVersion.UCIE_2_0,
) -> list[RunResult]:
    from dataclasses import replace

    scenarios = [
        replace(scenario, version=version) for scenario in build_random_suite(seed, count)
    ]
    return [run_scenario(scenario) for scenario in scenarios]
