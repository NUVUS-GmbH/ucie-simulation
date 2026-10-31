"""Milestone traces and the safety invariants around them."""

from __future__ import annotations

import unittest
from dataclasses import replace

from simulation.experiments.deterministic import run_milestones
from simulation.faults.scenarios import milestone_before_acceptance, milestone_unknown
from simulation.model.system import run_scenario
from simulation.model.types import Fate, FaultKind, Phase, Policy
from simulation.verification.invariants import check_invariants


class MilestoneTests(unittest.TestCase):
    def test_unknown_after_commit(self) -> None:
        result = run_scenario(milestone_unknown())
        tx = result.transactions[0]
        self.assertEqual(tx.ground_commit_count, 1)
        self.assertFalse(tx.requester_received_response)
        self.assertEqual(tx.initiator_class, Fate.UNKNOWN)
        self.assertEqual(tx.fdi_class, Fate.UNKNOWN)
        self.assertEqual(tx.disposition_class, Fate.UNKNOWN)
        self.assertEqual(result.counter_end, 8)
        self.assertTrue(result.link_converged)
        self.assertFalse(result.deadlock)
        self.assertEqual(result.violations, [])
        text = "\n".join(result.events)
        self.assertIn("Target commits operation", text)
        self.assertIn("Counter changes from 7 to 8", text)
        self.assertIn("Retry threshold exceeded", text)
        self.assertIn("LinkError entered", text)
        self.assertIn("Response is discarded during cleanup", text)

    def test_not_completed_before_acceptance(self) -> None:
        result = run_scenario(milestone_before_acceptance())
        tx = result.transactions[0]
        self.assertEqual(tx.ground_commit_count, 0)
        self.assertFalse(tx.target_accepted)
        self.assertFalse(tx.requester_received_response)
        self.assertEqual(tx.disposition_class, Fate.NOT_COMPLETED)
        self.assertEqual(tx.initiator_class, Fate.NOT_COMPLETED)
        self.assertEqual(tx.fdi_class, Fate.UNKNOWN)
        self.assertEqual(result.counter_end, 7)
        self.assertEqual(check_invariants(result.transactions, result.scenario.policy), [])
        # The safe label is safe only because the effect count is zero.
        self.assertEqual(tx.oracle_class, Fate.NOT_COMPLETED)

    def test_fdi_cannot_distinguish_the_two_traces(self) -> None:
        runs = run_milestones()
        committed = runs["unknown"].transactions[0]
        missed = runs["before_fdi"].transactions[0]
        self.assertEqual(committed.fdi_class, Fate.UNKNOWN)
        self.assertEqual(missed.fdi_class, Fate.UNKNOWN)
        self.assertNotEqual(committed.ground_commit_count, missed.ground_commit_count)
        self.assertFalse(committed.requester_received_response)
        self.assertFalse(missed.requester_received_response)

    def test_calling_the_committed_trace_safe_to_retry_is_caught(self) -> None:
        result = run_scenario(milestone_unknown())
        tx = result.transactions[0]
        tx.initiator_class = Fate.NOT_COMPLETED
        problems = check_invariants([tx], Policy.DISCARD)
        self.assertTrue(any(item.startswith("property 1") for item in problems))

    def test_automatic_retry_of_the_committed_write_duplicates_the_effect(self) -> None:
        result = run_scenario(replace(milestone_unknown(), policy=Policy.AUTO_RETRY))
        tx = result.transactions[0]
        self.assertGreaterEqual(tx.ground_commit_count, 2)
        self.assertEqual(result.counter_end, 9)
        self.assertTrue(any(item.startswith("property 2") for item in result.violations))
        self.assertTrue(any(item.startswith("property 1") for item in result.violations))

    def test_automatic_retry_before_acceptance_commits_once(self) -> None:
        from simulation.model.types import Observation

        scenario = replace(
            milestone_before_acceptance(),
            policy=Policy.AUTO_RETRY,
            observation=Observation.FDI_AXI,
        )
        result = run_scenario(scenario)
        tx = result.transactions[0]
        self.assertEqual(tx.ground_commit_count, 1)
        self.assertEqual(result.counter_end, 8)
        self.assertFalse(any(item.startswith("property 2") for item in result.violations))


class TransparentRecoveryTests(unittest.TestCase):
    def test_one_two_and_three_bit_errors_are_replayed_once(self) -> None:
        from simulation.faults.scenarios import _base_write

        for fault, bits in (
            (FaultKind.SINGLE_BIT, 1),
            (FaultKind.TWO_BIT, 2),
            (FaultKind.THREE_BIT, 3),
        ):
            result = run_scenario(
                _base_write(
                    name=fault.value,
                    fault=fault,
                    phase=Phase.REQUEST_FLIT_TRANSMITTED,
                    counter_start=7,
                )
            )
            tx = result.transactions[0]
            self.assertEqual(tx.ground_commit_count, 1, fault)
            self.assertEqual(tx.initiator_class, Fate.COMPLETED, fault)
            self.assertEqual(result.counter_end, 8, fault)
            self.assertGreaterEqual(result.stats.transparent_retries, 1, fault)
            self.assertEqual(result.stats.linkerror_events, 0, fault)
            self.assertEqual(result.violations, [], fault)
            self.assertIn(str(bits), "\n".join(result.events))

    def test_replay_does_not_commit_twice(self) -> None:
        from simulation.faults.scenarios import _base_write

        result = run_scenario(
            _base_write(
                name="timeout",
                fault=FaultKind.RETRY_TIMEOUT,
                phase=Phase.WRITE_RESPONSE_TRANSMITTED,
                counter_start=7,
            )
        )
        tx = result.transactions[0]
        self.assertEqual(tx.ground_commit_count, 1)
        self.assertEqual(tx.initiator_class, Fate.COMPLETED)
        self.assertFalse(tx.duplicate_completion)
        self.assertIn("Duplicate", "\n".join(result.events))
        self.assertEqual(result.counter_end, 8)


if __name__ == "__main__":
    unittest.main()
