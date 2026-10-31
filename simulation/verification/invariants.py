"""Safety checks. Ground truth is compared with the initiator label after the run.

The classifier itself never reads ``ground_commit_count``. These checks do,
because that is how a false "safe to retry" decision is detected.
"""

from __future__ import annotations

from simulation.model.axi import Transaction
from simulation.model.types import Fate, Policy


def check_invariants(transactions: list[Transaction], policy: Policy) -> list[str]:
    del policy  # retained so callers can pass the policy that produced the run
    problems: list[str] = []
    for tx in transactions:
        label = tx.initiator_class
        if label == Fate.NOT_COMPLETED and tx.ground_commit_count != 0:
            problems.append(
                f"property 1: T{tx.logical_id} classified NOT_COMPLETED "
                f"but architectural_commit_count is {tx.ground_commit_count}"
            )
        if tx.commit_at_reissue not in (None, 0):
            problems.append(
                f"property 1: T{tx.logical_id} was automatically retried while "
                f"architectural_commit_count was already {tx.commit_at_reissue}"
            )
        if tx.ground_commit_count > 1:
            via = (
                "after automatic retry of the same logical transaction"
                if tx.software_retry_count
                else "inside one logical transaction"
            )
            problems.append(
                f"property 2: T{tx.logical_id} architectural_commit_count "
                f"{tx.ground_commit_count} {via}"
            )
        if label == Fate.COMPLETED and tx.ground_commit_count != 1:
            problems.append(
                f"property 3: T{tx.logical_id} classified COMPLETED "
                f"but architectural_commit_count is {tx.ground_commit_count}"
            )
        if tx.duplicate_completion:
            problems.append(
                f"property 7: duplicate successful completion of T{tx.logical_id}"
            )
    return problems
