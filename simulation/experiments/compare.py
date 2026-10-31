"""Equivalent UCIe 2.0 and UCIe 3.0 workloads, plus 3.0-only mechanisms.

The equivalent set is the existing systematic suite and the same seeded
random suite, executed once per revision. The 3.0-only set is not mixed
into the percentages. Those percentages are observed frequencies under this
workload, not protocol probabilities.
"""

from __future__ import annotations

from dataclasses import dataclass, field, replace

from simulation.experiments.deterministic import run_milestones
from simulation.experiments.randomized import run_randomized
from simulation.faults.scenarios import milestone_unknown, ucie3_only_suite
from simulation.model.axi import Transaction
from simulation.model.system import RunResult, run_scenario
from simulation.model.types import Fate, Policy, UcieVersion
from simulation.reporting.statistics import bucket_rates, tally
from simulation.verification.state_explorer import ExplorerSummary, explore


PROFILES = ("fdi_axi", "software_error", "management")


@dataclass
class EqPair:
    name: str
    committed: RunResult
    uncommitted: RunResult
    matched: dict[str, bool]


@dataclass
class WindowStats:
    closed: list[int] = field(default_factory=list)
    opened: list[int] = field(default_factory=list)

    def _avg(self, values: list[int]) -> float | None:
        if not values:
            return None
        return sum(values) / len(values)

    @property
    def closed_min(self) -> int | None:
        return min(self.closed) if self.closed else None

    @property
    def closed_max(self) -> int | None:
        return max(self.closed) if self.closed else None

    @property
    def closed_avg(self) -> float | None:
        return self._avg(self.closed)

    @property
    def open_min(self) -> int | None:
        return min(self.opened) if self.opened else None

    @property
    def open_max(self) -> int | None:
        return max(self.opened) if self.opened else None

    @property
    def open_avg(self) -> float | None:
        return self._avg(self.opened)

    @property
    def eliminated(self) -> bool:
        return not self.opened


@dataclass
class VersionRun:
    version: UcieVersion
    milestones: dict[str, RunResult]
    explorer: ExplorerSummary
    random_results: list[RunResult]
    windows: WindowStats
    unknown_scenarios: list[str]


@dataclass
class ComparisonRun:
    seed: int
    random_count: int
    v2: VersionRun
    v3: VersionRun
    only: list[RunResult]
    retry: RunResult
    pairs: list[EqPair]


def _watched(result: RunResult) -> Transaction | None:
    if not result.transactions:
        return None
    index = min(result.scenario.fault_tx, len(result.transactions) - 1)
    return result.transactions[index]


def initiator_view(tx: Transaction, profile: str) -> tuple[object, ...]:
    """Standardized initiator facts. Ground-truth commit count is not included."""

    return (
        tx.axi_accepted,
        tx.requester_received_response,
        tx.explicit_unknown,
        tx.signals.view(profile),
    )


def views_match(left: RunResult, right: RunResult, profile: str) -> bool:
    if not left.transactions or not right.transactions:
        return False
    return initiator_view(left.transactions[0], profile) == initiator_view(
        right.transactions[0], profile
    )


def _windows(results: list[RunResult]) -> WindowStats:
    stats = WindowStats()
    for result in results:
        tx = _watched(result)
        if tx is None or tx.ambiguity_window is None or tx.ground_commit_count < 1:
            continue
        if tx.ambiguity_open:
            stats.opened.append(tx.ambiguity_window)
        else:
            stats.closed.append(tx.ambiguity_window)
    return stats


def _unknown_scenarios(results: list[RunResult]) -> list[str]:
    names: list[str] = []
    for result in results:
        tx = _watched(result)
        if tx is not None and tx.fdi_class == Fate.UNKNOWN:
            names.append(result.scenario.name)
    return names


def _version_run(
    version: UcieVersion,
    seed: int,
    random_count: int,
) -> VersionRun:
    milestones = run_milestones(version)
    explorer = explore(version)
    random_results = run_randomized(seed, random_count, version)
    combined = explorer.results + random_results
    return VersionRun(
        version=version,
        milestones=milestones,
        explorer=explorer,
        random_results=random_results,
        windows=_windows(combined),
        unknown_scenarios=_unknown_scenarios(combined),
    )


def _pair(name: str, committed: RunResult, uncommitted: RunResult) -> EqPair:
    return EqPair(
        name=name,
        committed=committed,
        uncommitted=uncommitted,
        matched={
            profile: views_match(committed, uncommitted, profile) for profile in PROFILES
        },
    )


def _named(results: list[RunResult], name: str) -> RunResult:
    for result in results:
        if result.scenario.name == name:
            return result
    raise KeyError(name)


def run_comparison(seed: int, random_count: int) -> ComparisonRun:
    v2 = _version_run(UcieVersion.UCIE_2_0, seed, random_count)
    v3 = _version_run(UcieVersion.UCIE_3_0, seed, random_count)
    only = [run_scenario(scenario) for scenario in ucie3_only_suite()]
    retry = run_scenario(
        replace(milestone_unknown(), version=UcieVersion.UCIE_3_0, policy=Policy.AUTO_RETRY)
    )
    pairs = [
        _pair(
            "equivalent LinkError loss",
            v3.milestones["unknown"],
            v3.milestones["before_fdi"],
        ),
        _pair(
            "Emergency Shutdown",
            _named(only, "ucie3_emergency_after_commit"),
            _named(only, "ucie3_emergency_before_acceptance"),
        ),
        _pair(
            "priority sideband plus mainband loss",
            _named(only, "ucie3_pspt_after_commit"),
            _named(only, "ucie3_pspt_before_acceptance"),
        ),
        _pair(
            "endpoint reset",
            _named(only, "ucie3_endpoint_reset_after_commit"),
            _named(only, "ucie3_endpoint_reset_before_acceptance"),
        ),
    ]
    return ComparisonRun(
        seed=seed,
        random_count=random_count,
        v2=v2,
        v3=v3,
        only=only,
        retry=retry,
        pairs=pairs,
    )


def combined_tally(version: VersionRun) -> dict[str, int]:
    systematic = tally(version.explorer.results)
    randomized = tally(version.random_results)
    merged = dict(systematic)
    for key, value in randomized.items():
        if key == "simulations":
            merged[key] = systematic[key] + value
        else:
            merged[key] = merged.get(key, 0) + value
    merged["systematic"] = systematic["simulations"]
    merged["randomized"] = randomized["simulations"]
    return merged


def bucket_unknown_rate(version: VersionRun) -> list[tuple[str, int, int, int, int]]:
    return bucket_rates(version.random_results, "fdi")


def recovery_traces(version: VersionRun) -> int:
    count = 0
    for result in version.explorer.results + version.random_results:
        stats = result.stats
        if stats.linkerror_events or stats.linkreset_events or stats.retrain_events:
            count += 1
    return count
