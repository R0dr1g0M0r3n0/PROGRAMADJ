"""Run the repository's native CTest suite without touching user media."""

import argparse
from pathlib import Path
import subprocess


def build_command(build_dir: Path, config: str):
    return ["ctest", "--test-dir", str(build_dir), "-C", config, "--output-on-failure"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--config", choices=("Debug", "Release"), default="Debug")
    args = parser.parse_args()
    try:
        return subprocess.run(build_command(args.build_dir, args.config), check=False).returncode
    except FileNotFoundError:
        parser.error("ctest is unavailable; install CMake before running checks")


if __name__ == "__main__":
    raise SystemExit(main())
