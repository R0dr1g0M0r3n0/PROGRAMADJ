from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from Harness.run_checks import build_command
from Harness.run_checks import run_check


class RunChecksTests(unittest.TestCase):
    def test_builds_ctest_command_for_selected_configuration(self):
        self.assertEqual(
            build_command(Path("build"), "Release"),
            ["ctest", "--test-dir", "build", "-C", "Release", "--output-on-failure"],
        )

    def test_captures_successful_command_output(self):
        result = run_check([sys.executable, "-c", "print('ok')"], timeout_seconds=2)

        self.assertEqual(result.exit_code, 0)
        self.assertEqual(result.stdout.strip(), "ok")
        self.assertFalse(result.timed_out)
        self.assertFalse(result.skipped)
        self.assertFalse(result.regression)

    def test_captures_deliberate_failure_and_regression_signal(self):
        result = run_check(
            [sys.executable, "-c", "import sys; print('failure', file=sys.stderr); sys.exit(3)"],
            timeout_seconds=2,
            baseline_exit_code=0,
        )

        self.assertEqual(result.exit_code, 3)
        self.assertIn("failure", result.stderr)
        self.assertTrue(result.regression)

    def test_captures_timeout_without_hanging(self):
        result = run_check([sys.executable, "-c", "import time; time.sleep(1)"], timeout_seconds=0.01)

        self.assertIsNone(result.exit_code)
        self.assertTrue(result.timed_out)
        self.assertIn("timed out", result.stderr)

    def test_marks_missing_command_as_skipped(self):
        result = run_check(["command-that-does-not-exist"], timeout_seconds=2)

        self.assertIsNone(result.exit_code)
        self.assertTrue(result.skipped)
        self.assertIn("unavailable", result.stderr)


if __name__ == "__main__":
    unittest.main()
