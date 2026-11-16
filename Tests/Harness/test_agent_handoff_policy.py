from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
POLICY_PATH = ROOT / "docs" / "agent-handoff-policy.md"


class AgentHandoffPolicyTests(unittest.TestCase):
    def test_policy_declares_required_handoff_controls(self):
        policy = POLICY_PATH.read_text(encoding="utf-8")
        required_sections = (
            "## Required handoff",
            "## Acceptance gate",
            "## Protected paths",
            "## Evidence record",
        )
        for section in required_sections:
            self.assertIn(section, policy)

        required_controls = (
            "authorized files",
            "inputs",
            "outputs",
            "acceptance criteria",
            "independent verification",
            "acceptance tests",
            "audio",
            "DJ library",
            "skipped checks",
        )
        normalized = policy.lower()
        for control in required_controls:
            self.assertIn(control.lower(), normalized)

    def test_policy_blocks_acceptance_without_reproducible_proof(self):
        policy = POLICY_PATH.read_text(encoding="utf-8").lower()
        self.assertIn("unsafe or missing proof", policy)
        self.assertIn("must be rejected", policy)
        self.assertIn("cannot silently change", policy)


if __name__ == "__main__":
    unittest.main()
