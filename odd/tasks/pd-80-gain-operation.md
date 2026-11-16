# PD-80 Gain Operation

## Objective

Implement the bounded gain DSP operation specified by PD-79, wire its standalone quality/safety test into CTest, and record deterministic behavior and limits.

## Scope and constraints

- Authorized scope: local changes on `feature/pd-69-task-state`; Jira status update only after verified completion.
- Do not touch or stage `.atl/` or original audio/DJ library files.
- Bounded source scope: three files maximum — `API/audioforge/engine.h`, `Engine/src/engine.cpp`, and `CMakeLists.txt`; the PD-79 test remains unchanged.
- Strict TDD: RED → GREEN → REFACTOR.
- Operation is bounded, synchronous, interleaved float gain over caller-owned buffers; no allocation, file I/O, clipping policy, or real-time guarantee is added.

## Work units

- [x] **T1 — Add the public gain contract.** Declared `audioforge_gain_apply` in the versioned C API.
- [x] **T2 — Implement and wire the operation.** Validated format/finite gain/capacity, copied samples deterministically, and added the DSP test target to CMake.
- [x] **T3 — Measure and verify.** RED → GREEN, native tests, harness, and diff checks passed; exact results and limits are recorded below.

## Route

- Route: delegated direct writer because the implementation spans three non-trivial source/build files.
- Future changes outside the three authorized files require a new task decision.

## Acceptance criteria

- PD-79 impulse and silence tests pass exactly.
- Invalid gain, format mismatch, and insufficient capacity fail without modifying destination samples.
- The DSP test is wired into CTest and the full native suite remains green.
- No unsupported latency or real-audio quality claim is made; this slice measures correctness only.
- Original audio and DJ library files remain untouched.

## Checks

- `cmake --build build --config Debug --target AudioForgeDSPTests`
- `ctest --test-dir build -C Debug --output-on-failure`
- `python3 -m unittest discover -s Tests/Harness -p 'test_*.py'`
- `git diff --check`

## Verification evidence

- RED carried forward from PD-79: standalone gain test link failed with undefined `_audioforge_gain_apply` (exit code `1`).
- GREEN: `cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64` regenerated the project; `AudioForgeDSPTests` built successfully.
- CTest passed **5/5**, including `AudioForgeDSPTests`.
- Python harness passed **16/16**.
- `git diff --check`: PASS.
- The first read-only verifier found the generated project stale; regeneration resolved that build-state issue.
- No source changes were made outside the three authorized PD-80 files; PD-79 test remained unchanged.
- `.atl/` and original audio/DJ library paths were untouched.
- Limitation: correctness is measured on synthetic buffers only; no latency, clipping, or real-audio quality claim is made.
- Work-unit commit: `eba9b31` (`feat(dsp): implement bounded gain operation`).
