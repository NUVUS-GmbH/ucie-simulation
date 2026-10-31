"""Policies, credits, schedules, and seeded random runs."""

from __future__ import annotations

import unittest
from dataclasses import replace

from simulation.experiments.randomized import run_randomized
from simulation.faults.scenarios import _base_write, milestone_unknown
from simulation.model.system import run_scenario
from simulation.model.types import Fate, FaultKind, Phase, Policy
from simulation.verification.state_explorer import explore


class PropertyTests(unittest.TestCase):
    def test_credit_fault_is_detected_and_a_clean_write_is_not(self) -> None:
        bad = run_scenario(
            _base_write("credit", FaultKind.CREDIT, Phase.REQUEST_ACCEPTED_LOCALLY)
        )
        good = run_scenario(
            _base_write("clean", FaultKind.SINGLE_BIT, Phase.REQUEST_FLIT_TRANSMITTED)
        )
        self.assertTrue(any(item.startswith("property 4") for item in bad.violations))
        self.assertEqual(good.violations, [])
        self.assertEqual(good.transactions[0].initiator_class, Fate.COMPLETED)

    def test_preserve_local_completion_diverges_from_discard(self) -> None:
        discard = run_scenario(
            _base_write(
                "discard",
                FaultKind.FORCED_LINKERROR,
                Phase.WRITE_RESPONSE_RECEIVED,
                policy=Policy.DISCARD,
            )
        )
        keep = run_scenario(
            replace(discard.scenario, name="keep", policy=Policy.PRESERVE_LOCAL)
        )
        self.assertEqual(discard.transactions[0].initiator_class, Fate.UNKNOWN)
        self.assertEqual(keep.transactions[0].initiator_class, Fate.COMPLETED)
        self.assertEqual(discard.transactions[0].ground_commit_count, 1)
        self.assertEqual(keep.transactions[0].ground_commit_count, 1)
        self.assertEqual(keep.violations, [])

    def test_explicit_unknown_is_not_a_successful_completion(self) -> None:
        result = run_scenario(
            replace(milestone_unknown(), policy=Policy.EXPLICIT_UNKNOWN, logical_id_base=3)
        )
        tx = result.transactions[0]
        self.assertTrue(tx.explicit_unknown)
        self.assertFalse(tx.axi_completed_ok)
        self.assertEqual(tx.initiator_class, Fate.UNKNOWN)
        self.assertEqual(tx.ground_commit_count, 1)
        self.assertFalse(any(item.startswith("property 3") for item in result.violations))

    def test_not_started_versus_already_completed(self) -> None:
        early = run_scenario(
            _base_write("early", FaultKind.FORCED_LINKERROR, Phase.CREATED)
        )
        done = run_scenario(
            _base_write("done", FaultKind.FORCED_LINKERROR, Phase.TRANSACTION_COMPLETED)
        )
        accepted = run_scenario(
            _base_write("accepted", FaultKind.FORCED_LINKERROR, Phase.TARGET_ACCEPTED)
        )
        self.assertEqual(early.transactions[0].initiator_class, Fate.NOT_COMPLETED)
        self.assertEqual(early.transactions[0].ground_commit_count, 0)
        self.assertEqual(done.transactions[0].initiator_class, Fate.COMPLETED)
        self.assertEqual(done.transactions[0].ground_commit_count, 1)
        # Acknowledged, accepted, not yet committed: initiator still cannot
        # prove the effect is absent.
        self.assertEqual(accepted.transactions[0].ground_commit_count, 0)
        self.assertEqual(accepted.transactions[0].fdi_class, Fate.UNKNOWN)
        self.assertEqual(accepted.transactions[0].disposition_class, Fate.UNKNOWN)

    def test_peer_transaction_is_not_started_when_schedule_is_focused(self) -> None:
        from simulation.faults.scenarios import exploration_suite

        scenario = next(
            item for item in exploration_suite() if item.name == "two_outstanding_peer_not_started"
        )
        result = run_scenario(scenario)
        first, second = result.transactions
        self.assertEqual(first.ground_commit_count, 1)
        self.assertEqual(first.fdi_class, Fate.UNKNOWN)
        self.assertEqual(second.ground_commit_count, 0)
        self.assertFalse(second.axi_accepted)
        self.assertEqual(second.fdi_class, Fate.NOT_COMPLETED)

    def test_retrain_buffer_interpretation_changes_the_outcome(self) -> None:
        preserved = run_scenario(
            _base_write(
                "keep",
                FaultKind.FORCED_RETRAIN,
                Phase.WRITE_RESPONSE_TRANSMITTED,
                retrain_preserves_retry=True,
            )
        )
        dropped = run_scenario(
            _base_write(
                "drop",
                FaultKind.FORCED_RETRAIN,
                Phase.WRITE_RESPONSE_TRANSMITTED,
                retrain_preserves_retry=False,
            )
        )
        self.assertEqual(preserved.transactions[0].initiator_class, Fate.COMPLETED)
        self.assertFalse(preserved.deadlock)
        self.assertEqual(dropped.transactions[0].initiator_class, Fate.UNKNOWN)
        self.assertTrue(dropped.deadlock)
        self.assertEqual(dropped.transactions[0].ground_commit_count, 1)

    def test_randomized_runs_repeat_for_a_seed(self) -> None:
        first = run_randomized(11, 25)
        second = run_randomized(11, 25)
        third = run_randomized(12, 25)

        def fingerprint(results):
            return [
                (
                    result.scenario.name,
                    tuple(result.events),
                    tuple(
                        (
                            tx.ground_commit_count,
                            tx.fdi_class.value,
                            tx.disposition_class.value,
                        )
                        for tx in result.transactions
                    ),
                    tuple(result.violations),
                )
                for result in results
            ]

        self.assertEqual(fingerprint(first), fingerprint(second))
        self.assertNotEqual(fingerprint(first), fingerprint(third))

    def test_exploration_finds_unknown_and_duplicate_commit(self) -> None:
        summary = explore()
        self.assertIn("fdi_unknown_after_commit", summary.minimal)
        self.assertIn("duplicate_commit", summary.minimal)
        self.assertGreater(summary.simulations, 0)
        found = summary.minimal["fdi_unknown_after_commit"]
        tx = found.transactions[0]
        self.assertGreaterEqual(tx.ground_commit_count, 1)
        self.assertEqual(tx.fdi_class, Fate.UNKNOWN)


if __name__ == "__main__":
    unittest.main()
