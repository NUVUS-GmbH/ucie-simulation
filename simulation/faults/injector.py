"""Named faults and the phase each canonical case is aimed at.

The injector does not apply bit flips itself. ``World.fire`` does, when the
watched transaction reaches ``Scenario.at_phase``. Keeping that decision in
the scenario, rather than in the target or the adapter, is what makes the
same trace rerunnable under another recovery policy.
"""

from __future__ import annotations

from simulation.model.types import FaultKind, Phase

# Canonical phase for each required fault. The phase is part of the experiment,
# not a protocol rule.
CANONICAL_PHASE: dict[FaultKind, Phase] = {
    FaultKind.SINGLE_BIT: Phase.REQUEST_FLIT_TRANSMITTED,
    FaultKind.TWO_BIT: Phase.WDATA_FLIT_TRANSMITTED,
    FaultKind.THREE_BIT: Phase.WRITE_RESPONSE_TRANSMITTED,
    FaultKind.REPEATED_CORRUPTION: Phase.WRITE_RESPONSE_TRANSMITTED,
    FaultKind.CRC_FAIL_REPLAY: Phase.REQUEST_FLIT_TRANSMITTED,
    FaultKind.RETRY_TIMEOUT: Phase.WRITE_RESPONSE_TRANSMITTED,
    FaultKind.RETRY_THRESHOLD: Phase.WRITE_RESPONSE_TRANSMITTED,
    FaultKind.FORCED_RETRAIN: Phase.WRITE_RESPONSE_TRANSMITTED,
    FaultKind.FORCED_LINKRESET: Phase.WRITE_RESPONSE_TRANSMITTED,
    FaultKind.FORCED_LINKERROR: Phase.ARCHITECTURAL_COMMIT,
    FaultKind.UNILATERAL_RESET: Phase.WRITE_RESPONSE_TRANSMITTED,
    FaultKind.REMOTE_UNRESPONSIVE: Phase.WRITE_RESPONSE_TRANSMITTED,
    FaultKind.SIDEBAND_TIMEOUT: Phase.REQUEST_RECEIVED_REMOTELY,
    FaultKind.DROPPED_RESPONSE: Phase.WRITE_RESPONSE_TRANSMITTED,
    FaultKind.DROPPED_REQUEST: Phase.REQUEST_FLIT_TRANSMITTED,
    FaultKind.AFTER_COMMIT_BEFORE_RESPONSE: Phase.WRITE_RESPONSE_TRANSMITTED,
    FaultKind.BEFORE_ARCHITECTURAL_COMMIT: Phase.REQUEST_FLIT_TRANSMITTED,
    FaultKind.CREDIT: Phase.REQUEST_ACCEPTED_LOCALLY,
    FaultKind.ACTIVATION: Phase.CREATED,
    FaultKind.DEACTIVATION: Phase.TARGET_ACCEPTED,
    FaultKind.REPEATED_DURING_RECOVERY: Phase.WRITE_RESPONSE_TRANSMITTED,
}


FAULTS_REQUIRING_A_FLIT = {
    FaultKind.SINGLE_BIT,
    FaultKind.TWO_BIT,
    FaultKind.THREE_BIT,
    FaultKind.REPEATED_CORRUPTION,
    FaultKind.CRC_FAIL_REPLAY,
    FaultKind.RETRY_TIMEOUT,
    FaultKind.RETRY_THRESHOLD,
    FaultKind.AFTER_COMMIT_BEFORE_RESPONSE,
    FaultKind.BEFORE_ARCHITECTURAL_COMMIT,
    FaultKind.REPEATED_DURING_RECOVERY,
}


def flit_phases() -> list[Phase]:
    return [
        Phase.REQUEST_FLIT_TRANSMITTED,
        Phase.WDATA_FLIT_TRANSMITTED,
        Phase.WRITE_RESPONSE_TRANSMITTED,
    ]
