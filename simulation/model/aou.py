"""AoU endpoint abstraction: packing, credits, activation.

Normative, from citations of AoU 0.8 §8.3.3:
  When FDI leaves the link-up condition (LinkError, LinkReset, Disabled,
  Reset), the activation FSM returns to DISABLED and an in-flight
  Activate/Deactivate handshake is dropped. Retrain is not that event.

ASSUMPTION:
  One credit represents one AoU message (request, write data, or response).
  Real AoU credits are per message class and depend on granule size. The
  granule encoding is not required to answer whether an AXI transaction's
  fate is known, and it is not invented here. Conservation is checked on
  this abstracted pool.

ASSUMPTION:
  Initial credit advertisement equals the receive-buffer depth at activation.
  Implementations that follow AoU describe a full-depth advertisement after
  reset. The on-wire credit encoding is not modeled.
"""

from __future__ import annotations

from dataclasses import dataclass, field

from simulation.model.types import AouState, Direction


@dataclass
class CreditPool:
    """One direction of abstracted AoU credits."""

    depth: int
    available: int
    held: int = 0
    occupancy: int = 0
    epoch: int = 0

    def __post_init__(self) -> None:
        if self.available == 0 and self.held == 0:
            self.available = self.depth

    def audit(self) -> list[str]:
        problems: list[str] = []
        if self.available + self.held != self.depth:
            problems.append(
                "property 4: credit conservation "
                f"(available {self.available} + held {self.held} != depth {self.depth})"
            )
        if self.available < 0 or self.held < 0:
            problems.append(
                "property 5: sender consumed credits the receiver had not made available"
            )
        if self.occupancy > self.depth:
            problems.append(
                "property 5: receiver buffer occupancy exceeded advertised credit"
            )
        if self.occupancy < 0:
            problems.append("property 5: receiver occupancy went negative")
        return problems


def fresh_pools(depth: int, epoch: int) -> dict[Direction, CreditPool]:
    return {
        Direction.A_TO_B: CreditPool(depth=depth, available=depth, epoch=epoch),
        Direction.B_TO_A: CreditPool(depth=depth, available=depth, epoch=epoch),
    }


@dataclass
class AouPair:
    """Both AoU endpoints. They share one activation state in this model.

    ASSUMPTION: the two endpoints run the same activation edge. A fault can
    still fail one side's handshake; that is recorded as activation failure
    rather than as two independently coded FSMs.
    """

    state_a: AouState = AouState.DISABLED
    state_b: AouState = AouState.DISABLED
    pools: dict[Direction, CreditPool] = field(default_factory=lambda: fresh_pools(4, 0))

    def enabled(self) -> bool:
        return self.state_a == AouState.ENABLED and self.state_b == AouState.ENABLED
