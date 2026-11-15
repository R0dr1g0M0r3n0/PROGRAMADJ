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
- Delivery strategy: ask-on-risk; split into stacked PRs if the accumulated diff exceeds the 400-line review budget.
- TDD: RED observed when `cmake --build build --config Debug --target AudioForgeProjectPersistenceTests` failed to compile because `project_store.hpp` did not exist; GREEN followed by adding the persistence boundary, implementation, and tests.
- Verification: `cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64` — passed; `cmake --build build --config Debug` — passed; `ctest --test-dir build -C Debug --output-on-failure` — 4/4 passed; `python3 -m unittest discover -s Tests/Harness -p 'test_*.py'` — 1 test passed; `git diff --check` — passed.
- T1/T2 work-unit commit: `1230ca6263816065f9bdd5e823f7760536aba6dc` (`feat(project-persistence): add validated manifest storage`).
- Next: implement T3 migration dispatch and tests, then complete T4 evidence and delivery.
