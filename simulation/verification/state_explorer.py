"""Systematic exploration of the reduced state space.

The search is explicit-state in the sense that every retained scenario is a
concrete state of the model (one or two outstanding transactions, a fault
phase, a fault kind, a recovery policy, and a retrain-buffer interpretation).
It is not a proof about interleavings outside that list. Traces that first
exhibit a fate or a violation are kept, shortest event list first.
"""

from __future__ import annotations

from dataclasses import dataclass, field

from simulation.experiments.deterministic import run_exploration
from simulation.model.system import RunResult
from simulation.model.types import Fate, UcieVersion


@dataclass
class ExplorerSummary:
    results: list[RunResult]
    minimal: dict[str, RunResult] = field(default_factory=dict)

    @property
    def simulations(self) -> int:
        return len(self.results)


def explore(version: UcieVersion = UcieVersion.UCIE_2_0) -> ExplorerSummary:
    results = run_exploration(version)
    minimal: dict[str, RunResult] = {}

    def keep(name: str, result: RunResult) -> None:
        current = minimal.get(name)
        if current is None or len(result.events) < len(current.events):
            minimal[name] = result

    for result in results:
        for tx in result.transactions:
            if tx.fdi_class == Fate.UNKNOWN and tx.ground_commit_count >= 1:
                keep("fdi_unknown_after_commit", result)
            if tx.fdi_class == Fate.UNKNOWN and tx.ground_commit_count == 0:
                keep("fdi_unknown_before_commit", result)
            if tx.disposition_class == Fate.NOT_COMPLETED and tx.ground_commit_count == 0:
                keep("disposition_not_completed", result)
            if tx.fdi_class == Fate.COMPLETED and tx.ground_commit_count == 1:
                keep("completed", result)
            if tx.ground_commit_count > 1:
                keep("duplicate_commit", result)
        if result.violations:
            keep("violation:" + result.violations[0].split(":")[0], result)
        if result.deadlock or not result.link_converged:
            keep("liveness", result)
    return ExplorerSummary(results=results, minimal=minimal)
