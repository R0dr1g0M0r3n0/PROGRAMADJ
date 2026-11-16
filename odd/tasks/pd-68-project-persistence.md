# PD-68 Project Persistence and Migration

## Objective

Implement non-destructive project persistence with round-trip guarantees and executable schema migration tests using the PD-67 manifest contract.

## Problem

PD-67 defines the manifest shape and invariants, but the repository cannot yet save/load a project, preserve original assets, or migrate manifests safely.

## Scope

- Add an isolated Project persistence module with native JSON parsing/serialization.
- Implement validated load/save round-trip for manifest v1.
- Implement versioned migration dispatch and rejection of unsupported future versions.
- Preserve source/original files and provide atomic replacement with recoverable backup semantics.
- Add CTest coverage for round-trip, semantic invariants, migration idempotence, malformed/future versions, and write-failure preservation.

## Constraints

- Do not modify source audio or DJ library files.
- Keep migration transforms copy-based and deterministic.
- Use RED -> GREEN -> REFACTOR for executable code.
- Preserve existing engine/bridge contracts and `.atl/`.
- Conventional Commits without AI attribution.

## Tasks

- [x] **T1 — Dependency and persistence boundary:** declare the native JSON dependency and add a Project persistence library/API without coupling it to DJLibrary.
- [x] **T2 — Round-trip and validation:** implement load/save, semantic invariant checks, original-file preservation and atomic write/backup behavior with tests.
- [ ] **T3 — Migration:** implement schema-version dispatch, current-version idempotence, future-version rejection and migration tests.
- [ ] **T4 — Evidence and delivery:** run CMake/CTest and harness checks, record exact limitations, and deliver via reviewable PR slice(s).

## Authorized scope

Repository source, tests, CMake metadata and this task document only. Installing build dependencies is authorized when required for PD-68 verification.

## Acceptance criteria

- A project manifest survives save/load with identifiers, relationships, metadata and original-file references unchanged.
- Original audio/library files are never rewritten by persistence operations.
- Current schema loads idempotently; supported older schemas migrate deterministically; future schemas are rejected without mutation.
- Failed writes leave the original manifest usable and a recoverable backup or temporary artifact is handled deterministically.
- Exact commands and outputs are recorded; unavailable checks remain explicit.

## Verification

- `cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64`
- `cmake --build build --config Debug`
- `ctest --test-dir build -C Debug --output-on-failure`
- `python3 -m unittest discover -s Tests/Harness -p 'test_*.py'`
- `git diff --check`

## Progress

- Route: delegated direct; exploration confirmed the repository has no existing persistence module or native JSON parser.
- Delivery strategy: `exception-ok` for the two existing T1/T2 work units only, explicitly accepted by the maintainer after the one-pass sizing audit. PR chain strategy and remote delivery remain undecided.
- TDD: Initial RED for T1/T2 was the missing `project_store.hpp`. Independent review then added regression tests; RED reproduced acceptance of an invalid digest (`invalid digest must be rejected`). GREEN implemented schema-driven v1 validation and filesystem safeguards.
- Corrected defects: validate against the checked-in JSON Schema (types, enums, patterns/digests, required fields, and extra properties); validate normalized `derived/` paths and reject symlink escapes/source collisions; enforce audio-root and history input/output lineage; use `openat`/`O_NOFOLLOW`, exclusive random temporary creation, `fsync`, and atomic `renameat`; reject symlink/non-regular targets and preserve the previous manifest as `.bak`.
- Failure tests: pre-create the former `.tmp.0` symlink and verify its target is unchanged; use `RLIMIT_FSIZE=0` to force a real temporary-write failure and verify manifest bytes remain identical; reject a directory and symlink destination without modifying their targets.
- Verification: `cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64` — passed; `cmake --build build --config Debug` — passed; `ctest --test-dir build -C Debug --output-on-failure` — 4/4 passed; `python3 -m unittest discover -s Tests/Harness -p 'test_*.py'` — 1 test passed; `git diff --check` — passed.
- T1/T2 work-unit commit: `1230ca6263816065f9bdd5e823f7760536aba6dc` (`feat(project-persistence): add validated manifest storage`).
- Independent-review correction commit: `29a5b5d6e244bc485eb76191def062cab81760ac` (`fix(project-persistence): validate schema and secure writes`).
- Next: commit the verified T3 work unit, then complete T4 delivery when the PR chain and remote authorization are decided.
- T3 implementation pending work-unit commit: `migrate_manifest` returns an unchanged validated v1 copy, rejects unsupported older/future/malformed versions, and `load_manifest` uses the dispatcher. Direct and file-load regression tests cover rejection without modifying manifest, backup, source audio, or related files. No older on-disk contract exists in PD-67, so no legacy transform is invented. Build, CTest (4/4), Python harness (1/1), and `git diff --check` passed. Independent verification requested file-load regression proof, which was added and rechecked. Delivery sizing and branch strategy must be resolved before the next commit: the existing branch diff is 995 authored lines against main, and the two source commits are each over 400.
- T4 local check: Xcode arm64 CMake configuration and Debug build passed; CTest passed 4/4, Python harness passed 1/1, and `git diff --check` passed. The build emitted a duplicate `-lobjc` linker warning but no failure. The untracked `.atl/` directory was excluded from source changes and remains untouched.
- Delivery sizing audit: existing T1/T2 behavior commits contain 554 and 530 authored changed lines respectively; keeping each commit with its tests produces PR slices 154 and 130 lines above the 400-line budget. T3's pre-approval uncommitted diff was 111 authored lines. A smaller split would require rewriting and independently validating intermediate history. The maintainer explicitly accepted `size:exception` for these two existing cuts; no chain strategy, remote delivery, or history rewrite has been authorized.
