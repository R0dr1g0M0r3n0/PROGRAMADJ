# PD-79 DSP Quality and Safety Tests

## Objective

Write first-failing tests for a bounded gain operation over the PD-75 audio buffer contract, covering impulse response, silence preservation, format/capacity safety, and non-finite input rejection.

## Scope and constraints

- Authorized scope: local changes on `feature/pd-69-task-state`; Jira status update only after the failing-test contract is recorded.
- Do not touch or stage `.atl/` or original audio/DJ library files.
- Bounded slice: one test source file plus this task record; no DSP implementation belongs here.
- Strict TDD: this task intentionally stops at RED; PD-80 owns GREEN implementation and measurement.

## Work units

- [x] **T1 — Define the gain contract in tests.** Declared the expected bounded operation over `AudioForgeAudioBuffer` and `AudioForgeAudioBufferOutput`.
- [x] **T2 — Add quality and safety cases.** Covered impulse gain, silence, output capacity, invalid gain, and format preservation.
- [x] **T3 — Verify RED and record closure evidence.** Syntax compilation passed; linking against the current engine failed as expected with the missing symbol.

## Route

- Route: direct inline, bounded to one test source plus this task record; no implementation or CMake wiring is allowed until PD-80.
- Future implementation target: `audioforge_gain_apply` in the public C API, with deterministic output and explicit limits.

## Acceptance criteria

- The test contract covers impulse output within exact tolerance, silence preservation, output format preservation, insufficient capacity rejection, and non-finite gain rejection.
- The test fails against the current engine because the operation is not implemented.
- Existing repository CTest remains green because the future DSP test is intentionally not wired into CMake yet.
- Original audio and DJ library files remain untouched.

## Checks

- `clang++ -std=c++20 -fsyntax-only Tests/DSP/gain_quality_safety_test.cpp -I API`
- Link/run attempt against `build/Debug/libAudioForgeEngine.a` must fail with the missing `audioforge_gain_apply` symbol.
- `ctest --test-dir build -C Debug --output-on-failure`
- `python3 -m unittest discover -s Tests/Harness -p 'test_*.py'`
- `git diff --check`

## Verification evidence

- Syntax check: `clang++ -std=c++20 -fsyntax-only Tests/DSP/gain_quality_safety_test.cpp -I API`: PASS.
- RED: linking the standalone test against `build/Debug/libAudioForgeEngine.a` failed with undefined symbol `_audioforge_gain_apply` (exit code `1`).
- Existing CTest remained green: **4/4**.
- Python harness passed **16/16**.
- `git diff --check`: PASS.
- Independent verifier reproduced CTest, harness, and diff checks; the parent recorded the expected RED link failure separately.
- `.atl/` and original audio/DJ library paths were untouched.
- No DSP implementation or CMake wiring was added; PD-80 owns GREEN.
- Work-unit commit: `b5822ff` (`test(dsp): define gain quality and safety contract`).
