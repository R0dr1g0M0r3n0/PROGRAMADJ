# PD-72 Deliberate Failure Evidence

## Objective

Exercise the bounded command runner with a deliberate failure and prove that the recorded evidence preserves the exact command, exit code, logs, skipped state, timeout state, and regression signal.

## Scope and constraints

- Authorized scope: local changes on `feature/pd-69-task-state`; no remote Jira/GitHub operations.
- Do not touch or stage `.atl/` or the original audio/DJ library.
- Reuse PD-71's runner; this slice adds evidence serialization and an explicit failure-path test.
- Bounded scope selected for this slice: three files maximum.
- Strict TDD: RED → GREEN → REFACTOR.

## Work units

- [x] **T1 — Serialize deliberate failure evidence.** Added a stable JSON evidence envelope and an explicit exact-failure capture test.
- [x] **T2 — Verify and record closure evidence.** Focused and full harness checks passed; exact results and commit identity are recorded below.

## Next step

Verification evidence:

- RED: `python3 -m unittest Tests/Harness/test_failure_evidence.py` failed because `evidence_json` was not yet implemented.
- GREEN: focused evidence test passed **1/1**.
- Full Python harness discovery passed **13/13**.
- `python3 Harness/run_checks.py --build-dir build --config Debug --timeout-seconds 30`: PASS, CTest **4/4**, exit code `0`.
- `git diff --check`: PASS.
- Work-unit commit: to be recorded after the bounded three-file commit.
