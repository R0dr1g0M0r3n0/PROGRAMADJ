import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from Harness.run_checks import evidence_json, run_check


class FailureEvidenceTests(unittest.TestCase):
    def test_deliberate_failure_serializes_exact_evidence(self):
        result = run_check(
            [sys.executable, "-c", "import sys; print('intentional failure', file=sys.stderr); sys.exit(17)"],
            timeout_seconds=2,
            baseline_exit_code=0,
        )

        evidence = json.loads(evidence_json(result))
        self.assertEqual(evidence["command"][0], sys.executable)
        self.assertEqual(evidence["exit_code"], 17)
        self.assertIn("intentional failure", evidence["stderr"])
        self.assertFalse(evidence["skipped"])
        self.assertFalse(evidence["timed_out"])
        self.assertTrue(evidence["regression"])


if __name__ == "__main__":
    unittest.main()
