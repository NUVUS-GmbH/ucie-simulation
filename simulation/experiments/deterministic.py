"""Run a fixed scenario list. Same scenarios always produce the same traces."""

from __future__ import annotations

from dataclasses import replace

from simulation.faults.scenarios import (
    exploration_suite,
    milestone_before_acceptance,
    milestone_unknown,
)
from simulation.model.system import RunResult, Scenario, run_scenario
from simulation.model.types import Observation, UcieVersion


def run_many(scenarios: list[Scenario]) -> list[RunResult]:
    return [run_scenario(scenario) for scenario in scenarios]


def run_milestones(version: UcieVersion = UcieVersion.UCIE_2_0) -> dict[str, RunResult]:
    unknown = run_scenario(replace(milestone_unknown(), version=version))
    before = run_scenario(replace(milestone_before_acceptance(), version=version))
    before_fdi = run_scenario(
        replace(
            milestone_before_acceptance(),
            name="milestone_before_fdi_only",
            observation=Observation.FDI_AXI,
            version=version,
        )
    )
    return {
        "unknown": unknown,
        "before_disposition": before,
        "before_fdi": before_fdi,
    }


def run_exploration(version: UcieVersion = UcieVersion.UCIE_2_0) -> list[RunResult]:
    return run_many([replace(item, version=version) for item in exploration_suite()])
