"""Run the repository's native CTest suite without touching user media."""

import argparse
from dataclasses import asdict, dataclass
import json
from pathlib import Path
import subprocess
import sys
from typing import Optional, Sequence


@dataclass(frozen=True)
class CheckResult:
    command: list[str]
    exit_code: Optional[int]
    stdout: str
    stderr: str
    timed_out: bool
    skipped: bool
    regression: bool


def build_command(build_dir: Path, config: str):
    return ["ctest", "--test-dir", str(build_dir), "-C", config, "--output-on-failure"]


def _text(value) -> str:
    if value is None:
        return ""
    if isinstance(value, bytes):
        return value.decode(errors="replace")
    return str(value)


def run_check(
    command: Sequence[str],
    *,
    timeout_seconds: float,
    cwd: Optional[Path] = None,
    baseline_exit_code: Optional[int] = None,
) -> CheckResult:
    if not command:
        raise ValueError("command must not be empty")
    command = [str(part) for part in command]
    try:
        completed = subprocess.run(
            command,
            cwd=cwd,
            capture_output=True,
            text=True,
            timeout=timeout_seconds,
            check=False,
        )
        exit_code = completed.returncode
        return CheckResult(
            command=command,
            exit_code=exit_code,
            stdout=_text(completed.stdout),
            stderr=_text(completed.stderr),
            timed_out=False,
            skipped=False,
            regression=baseline_exit_code is not None and exit_code != baseline_exit_code,
        )
    except subprocess.TimeoutExpired as error:
        return CheckResult(
            command=command,
            exit_code=None,
            stdout=_text(error.stdout),
            stderr=f"command timed out after {timeout_seconds} seconds\n{_text(error.stderr)}",
            timed_out=True,
            skipped=False,
            regression=baseline_exit_code is not None,
        )
    except FileNotFoundError as error:
        return CheckResult(
            command=command,
            exit_code=None,
            stdout="",
            stderr=f"command unavailable: {error.filename}\n",
            timed_out=False,
            skipped=True,
            regression=False,
        )


def evidence_json(result: CheckResult) -> str:
    return json.dumps(asdict(result), sort_keys=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--config", choices=("Debug", "Release"), default="Debug")
    parser.add_argument("--timeout-seconds", type=float, default=300)
    args = parser.parse_args()
    result = run_check(build_command(args.build_dir, args.config), timeout_seconds=args.timeout_seconds)
    if result.stdout:
        print(result.stdout, end="")
    if result.stderr:
        print(result.stderr, end="", file=sys.stderr)
    print(evidence_json(result), file=sys.stderr)
    return result.exit_code if result.exit_code is not None else 1


if __name__ == "__main__":
    raise SystemExit(main())
