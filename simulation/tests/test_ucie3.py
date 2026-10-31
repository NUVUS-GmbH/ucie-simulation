"""UCIe 3.0 keeps the 2.0 ambiguity unless a standardized signal names the commit."""

from __future__ import annotations

import unittest
from dataclasses import replace

from simulation.experiments.compare import initiator_view, views_match
from simulation.experiments.deterministic import run_milestones
from simulation.faults.scenarios import milestone_unknown, ucie3_only_suite
from simulation.model.spec30 import commit_knowledge_defined
from simulation.model.system import run_scenario
from simulation.model.types import Fate, Policy, UcieVersion


def _only(name: str):
    scenario = next(item for item in ucie3_only_suite() if item.name == name)
    return run_scenario(scenario)


class CatalogTests(unittest.TestCase):
    def test_no_mechanism_provides_commit_knowledge(self) -> None:
        self.assertFalse(commit_knowledge_defined())


class CounterexampleTests(unittest.TestCase):
    def test_ucie3_replays_the_committed_loss_as_unknown(self) -> None:
        runs = run_milestones(UcieVersion.UCIE_3_0)
        committed = runs["unknown"].transactions[0]
        missed = runs["before_fdi"].transactions[0]
        self.assertEqual(committed.ground_commit_count, 1)
        self.assertEqual(missed.ground_commit_count, 0)
        self.assertFalse(committed.requester_received_response)
        self.assertFalse(missed.requester_received_response)
        for tx in (committed, missed):
            self.assertEqual(tx.fdi_class, Fate.UNKNOWN)
            self.assertEqual(tx.software_class, Fate.UNKNOWN)
            self.assertEqual(tx.management_class, Fate.UNKNOWN)
            self.assertFalse(tx.signals.management_reports_axi_commit)
            self.assertFalse(tx.signals.header_log_has_axi_id)
            self.assertFalse(tx.signals.link_health_has_axi_id)
        self.assertTrue(committed.signals.linkerror)
        self.assertTrue(committed.signals.crc_error)
        self.assertTrue(views_match(runs["unknown"], runs["before_fdi"], "fdi_axi"))
        self.assertTrue(views_match(runs["unknown"], runs["before_fdi"], "software_error"))
        self.assertTrue(views_match(runs["unknown"], runs["before_fdi"], "management"))
        self.assertEqual(
            initiator_view(committed, "management"),
            initiator_view(missed, "management"),
        )

    def test_ambiguity_window_is_not_shorter_than_ucie2(self) -> None:
        v2 = run_scenario(milestone_unknown())
        v3 = run_scenario(replace(milestone_unknown(), version=UcieVersion.UCIE_3_0))
        self.assertTrue(v2.transactions[0].ambiguity_open)
        self.assertTrue(v3.transactions[0].ambiguity_open)
        self.assertEqual(
            v2.transactions[0].ambiguity_window,
            v3.transactions[0].ambiguity_window,
        )
        self.assertGreater(v3.transactions[0].ambiguity_window or 0, 0)

    def test_automatic_retry_still_duplicates(self) -> None:
        result = run_scenario(
            replace(
                milestone_unknown(),
                version=UcieVersion.UCIE_3_0,
                policy=Policy.AUTO_RETRY,
            )
        )
        tx = result.transactions[0]
        self.assertGreaterEqual(tx.ground_commit_count, 2)
        self.assertTrue(any(item.startswith("property 2") for item in result.violations))
        self.assertTrue(any(item.startswith("property 1") for item in result.violations))


class Ucie3MechanismTests(unittest.TestCase):
    def test_emergency_shutdown_pair_is_observationally_equivalent(self) -> None:
        committed = _only("ucie3_emergency_after_commit")
        missed = _only("ucie3_emergency_before_acceptance")
        self.assertEqual(committed.transactions[0].ground_commit_count, 1)
        self.assertEqual(missed.transactions[0].ground_commit_count, 0)
        self.assertTrue(committed.transactions[0].signals.emergency_shutdown)
        self.assertTrue(missed.transactions[0].signals.emergency_shutdown)
        self.assertFalse(committed.transactions[0].signals.linkerror)
        for profile in ("fdi_axi", "software_error", "management"):
            self.assertTrue(views_match(committed, missed, profile), profile)
        self.assertEqual(committed.transactions[0].management_class, Fate.UNKNOWN)
        self.assertEqual(missed.transactions[0].management_class, Fate.UNKNOWN)

    def test_priority_sideband_does_not_name_the_transaction(self) -> None:
        committed = _only("ucie3_pspt_after_commit")
        missed = _only("ucie3_pspt_before_acceptance")
        self.assertTrue(committed.transactions[0].signals.pspt_opcode_only)
        self.assertEqual(committed.transactions[0].ground_commit_count, 1)
        self.assertEqual(missed.transactions[0].ground_commit_count, 0)
        self.assertTrue(views_match(committed, missed, "management"))
        self.assertEqual(committed.transactions[0].management_class, Fate.UNKNOWN)

    def test_fast_throttle_parity_and_recalibration_leave_the_write_intact(self) -> None:
        for name, flag in (
            ("ucie3_fast_throttle_during_response", "fast_throttle"),
            ("ucie3_parity_during_response", "runtime_parity"),
            ("ucie3_recalibration_during_response", "runtime_recalibration"),
        ):
            result = _only(name)
            tx = result.transactions[0]
            self.assertTrue(getattr(tx.signals, flag), name)
            self.assertEqual(tx.ground_commit_count, 1, name)
            self.assertTrue(tx.requester_received_response, name)
            self.assertEqual(tx.fdi_class, Fate.COMPLETED, name)
            self.assertFalse(tx.ambiguity_open, name)

    def test_endpoint_reset_pair_stays_unknown(self) -> None:
        committed = _only("ucie3_endpoint_reset_after_commit")
        missed = _only("ucie3_endpoint_reset_before_acceptance")
        self.assertNotEqual(
            committed.transactions[0].ground_commit_count,
            missed.transactions[0].ground_commit_count,
        )
        self.assertTrue(views_match(committed, missed, "management"))
        self.assertEqual(committed.transactions[0].fdi_class, Fate.UNKNOWN)


if __name__ == "__main__":
    unittest.main()
