# PD-75 Audio Buffer and I/O Contracts

## Objective

Define a versioned C-compatible audio buffer contract with channel/sample-rate validation and deterministic import/export primitives for synthetic data without touching user media.

## Scope and constraints

- Authorized scope: local changes on `feature/pd-69-task-state`; Jira status update only after verified completion.
- Do not touch or stage `.atl/` or original audio/DJ library files.
- Bounded slice: three files maximum, including this task document.
- Strict TDD: RED → GREEN → REFACTOR.
- This slice provides in-memory import/export primitives; filesystem codecs and real-library round trips remain future work.

## Work units

- [x] **T1 — Define the buffer contract.** Added format fields, validation rules, and deterministic copy primitives to the public C API.
- [x] **T2 — Add contract tests.** Covered valid round trips, invalid formats, capacity limits, and protected input behavior.
- [x] **T3 — Verify and record closure evidence.** Native, harness, and diff checks passed; closure evidence is recorded below.

## Route

- Route: direct inline, bounded to three files because the public header and existing C smoke test provide the established API/test boundary.
- Trigger evidence: existing `API/audioforge/engine.h` C ABI and `Tests/Engine/engine_api_smoke_test.c` contract test.

## Acceptance criteria

- Channel count and sample rate are validated before copying samples.
- Synthetic interleaved samples round-trip deterministically within exact floating-point equality.
- Null pointers, zero formats, overflow, and insufficient output capacity fail without writing output.
- Original audio and DJ library files remain untouched.

## Checks

- `cmake --build build --config Debug --target AudioForgeEngineTests`
- `ctest --test-dir build -C Debug --output-on-failure`
- `python3 -m unittest discover -s Tests/Harness -p 'test_*.py'`
- `git diff --check`

## Verification evidence

- RED: `cmake --build build --config Debug --target AudioForgeEngineTests` failed because the new audio buffer types and functions were not yet defined.
- GREEN: Debug engine target built successfully.
- `ctest --test-dir build -C Debug --output-on-failure`: PASS, **4/4**.
- Python harness discovery: PASS, **16/16**.
- `git diff --check`: PASS.
- Independent verification: delegated verifier reproduced all checks with no failures or skips.
- `.atl/`, `TestFixtures/`, original audio, and DJ library paths were untouched.
- Work-unit commit: `c8410d9` (`feat(audio): add buffer import export contract`).
