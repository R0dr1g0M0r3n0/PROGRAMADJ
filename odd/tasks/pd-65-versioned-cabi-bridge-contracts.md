# PD-65 Versioned C ABI and Bridge Contracts

## Objective

Define and test the stable C ABI and Objective-C++ Bridge contracts required before asynchronous audio behavior is implemented.

## Scope

- Versioned POD request/result/error and buffer-lifetime contracts.
- Deterministic argument validation and explicit unsupported-processing status.
- Cancellation token, progress callback, real-time callback, and time-transform interfaces without implementing DSP.
- Objective-C++ adaptation with ownership and error mapping.
- Debug/Release CTest coverage, app build verification, and module-boundary documentation.

## Constraints

- Preserve original audio and DJ library files.
- Engine remains Swift- and Rekordbox-independent.
- Borrowed buffers never escape synchronous calls; caller-owned output is cleared on failure.
- No fake asynchronous processing, DSP, Rekordbox integration, or claim of real-time performance.
- Strict TDD: record RED before executable implementation, then GREEN and REFACTOR.
- Conventional Commits without AI attribution; no merge or Jira transition until evidence is complete.

## Tasks

- [x] **T1 — C ABI contract:** Add versioned POD request/result/error, explicit buffer ownership, cancellation/progress/time-transform declarations, validation, and C smoke tests.
- [x] **T2 — Objective-C++ Bridge contract:** Adapt ABI version/error/cancellation/progress/time semantics into Foundation-safe Bridge types and tests without async/DSP behavior.
- [x] **T3 — Evidence:** Run Debug/Release CTest, scoped Python/harness checks, and xcodebuild; record evidence, commits, limitations, and mirror this task document. Jira remains with the parent orchestrator.

## Verification

- `cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64`
- Debug and Release builds for all CTest targets.
- `ctest --test-dir build -C Debug --output-on-failure`
- `ctest --test-dir build -C Release --output-on-failure`
- `python3 -m unittest discover -s Tests/Harness -p 'test_*.py'`
- `python3 Harness/run_checks.py --config Debug`
- `xcodebuild -project build/AudioForgeStudio.xcodeproj -scheme AudioForge -configuration Debug -sdk macosx CODE_SIGNING_ALLOWED=NO build`
- `git diff --check`

## Verified Results

- Configure: `cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64` — passed.
- Debug: all three CTest targets built; `ctest --test-dir build -C Debug --output-on-failure` — 3/3 passed.
- Release: all three CTest targets built; `ctest --test-dir build -C Release --output-on-failure` — 3/3 passed.
- Python harness tests: `python3 -m unittest discover -s Tests/Harness -p 'test_*.py'` — 1 test passed.
- Harness: `python3 Harness/run_checks.py --config Debug` — CTest 3/3 passed.
- App: the requested Debug `xcodebuild` command succeeded. Xcode reported a non-blocking duplicate `-lobjc` warning for the Bridge tests and skipped AppIntents metadata extraction because the app does not link AppIntents.
- Diff hygiene: `git diff --check` — passed for each local slice and the final chain.
- TDD: both API test additions were observed RED before corresponding contract implementation, then GREEN. REFACTOR consolidated C API result/error/output reset and validation helpers; focused and full matrices remained GREEN.

## Local Stacked PR Boundaries

- Strategy: `stacked-to-main`, selected by the parent. Each local branch is based on the prior branch and is intended to target `main` in order after its predecessor merges; no remote branch or PR was created.
- Slice 1, `feature/pd-65-01-control-time`: `9d917ee` — caller-owned atomic cancellation, checked frame-domain mapping, declarative callback types, C smoke coverage, and boundary notes. Parent: `main` at `6233715`. Review diff: **245 authored lines** (additions + deletions). Focused Debug Engine CTest 1/1 passed. Rollback: the token/time additions in the public header, Engine, C smoke test, and boundary note.
- Slice 2, `feature/pd-65-02-processing`: `5d46b38` — versioned processing records, borrowed-buffer and deterministic failure semantics, undersized output-record correction with canary tests. Parent: slice 1 at `9d917ee`. Review diff: **236 authored lines**. Focused Debug Engine CTest 1/1 passed. Rollback: processing records and implementation plus their C tests and boundary note; slice 1 remains intact.
- Slice 3, `feature/pd-65-03-bridge`: `98f75d2` plus the documentation evidence commit — Foundation-safe Bridge adaptation, Bridge smoke test and CMake integration, final module docs and this evidence. Parent: slice 2 at `5d46b38`. Review diff: **339 authored lines**. Debug CTest 3/3 passed. Rollback: Bridge surface/implementation/test, CMake test wiring, and matching documentation; the Engine slices remain intact.
- Dependency order: `main` → **slice 1** → **slice 2** → **slice 3**. Each slice is below the 400-line review limit; final feature tree matches the original implementation and tests.

## Progress

- Route: delegated direct (multiple non-trivial files and contract design).
- Delivery: `stacked-to-main`, selected by the parent to match PD-63. The local work is divided into three semantic review slices with boundaries above. No remote PR/push/merge is in scope.
- T1 RED: Added C smoke assertions for unsupported-processing status, result/error
  metadata, output clearing, version mismatch, cancellation validation/state, and
  source-to-output frame mapping. Configure passed; the focused Debug Engine test
  build failed as expected because the contract declarations did not yet exist.
- T1 GREEN: Added the versioned POD API, deterministic validation/output clearing,
  atomic caller-owned cancellation token, checked absolute-frame transform, and
  explicit unsupported processing. Focused Debug Engine CTest passed (1/1).
- T2 RED: Added Foundation Bridge contract tests before restoring Bridge production
  code; focused Debug Bridge build failed as expected because those public types
  and methods were absent.
- T2 GREEN: Added Foundation-safe version/error mapping, owned cancellation token,
  immutable progress metadata and checked time-transform adaptation. Focused Debug
  Engine and Bridge CTests passed (2/2); `git diff --check` passed.
- Limitations: no audio processing, asynchronous execution, progress callback
  invocation, or real-time callback execution/performance is implemented.
- Next: parent orchestrator owns the Jira update. The task record is mirrored in Engram.

## Bounded ABI Correction

- Root cause: `audioforge_engine_process` initialized the complete result/error
  records before checking their caller-declared `struct_size`, overwriting
  undersized versioned records.
- Regression RED: C fixtures declared only a 4-byte size prefix and protected the
  following 64 bytes with canaries. Focused Debug CTest failed with
  `FAIL: undersized versioned result and error records are rejected`.
- Correction: validate both output pointers and declared sizes before any record
  write; reject undersized records without modifying them. For valid records,
  preserve the caller's declared size and initialize/write only the current record
  prefix. Output-buffer clearing and request validation semantics remain unchanged.
- GREEN: focused Debug Engine CTest passed 1/1; guard bytes and size fields remain
  intact and failure still clears the caller output buffer.
- Full verification after correction: Debug CTest 3/3, Release CTest 3/3, scoped
  Python unittest 1/1, Harness CTest 3/3, requested Debug xcodebuild succeeded,
  `git diff --check` passed.
- The correction is included with its canary regression in slice 2 (`5d46b38`). The prior local correction commit was `969a17e` before this unpushed history was resliced.
- Final Engram mirror: `odd/pd-65-versioned-cabi-bridge-contracts/tasks`.

## Final Progress

- Correction reverified with Debug/Release CTest, scoped Python unittest, Harness,
  xcodebuild, and diff check after the ABI fix. No implementation work remains.
- Next: parent orchestrator owns the Jira update. No code or verification work remains.
