from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from Harness.run_checks import build_command


class RunChecksTests(unittest.TestCase):
    def test_builds_ctest_command_for_selected_configuration(self):
        self.assertEqual(
            build_command(Path("build"), "Release"),
            ["ctest", "--test-dir", "build", "-C", "Release", "--output-on-failure"],
        )


if __name__ == "__main__":
    unittest.main()
