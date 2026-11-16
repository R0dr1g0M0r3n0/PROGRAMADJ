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
- [x] **T2 — Round-trip and validation:** implement load/save, semantic invariant checks, original-file preservation and atomic write/backup behavior with tests. Reopened after review reproduced that saving `project.json` from the project cwd accepted a symlinked derived asset that loading the same manifest rejected; fixed by validating against the same canonical directory for save/load. Also reject derived-output collisions through a symlinked original-source alias.
- [x] **T3 — Migration:** implement schema-version dispatch, current-version idempotence, future-version rejection and migration tests.
- [x] **T4 — Evidence and delivery:** run CMake/CTest and harness checks, record exact limitations, and deliver via reviewable PR slices. Child PRs #9, #10, and #11 are now merged into the integration branch; final delivery through tracker PR #12 remains pending the final merge gate.

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
- Delivery strategy: Feature Branch Chain. The maintainer accepted the child-slice `size:exception` on PRs #9 and #10 and later accepted an aggregate `size:exception` for final delivery through tracker PR #12. PR #9, #10, and #11 were based on the integration branch, the preceding child branch, and the preceding child branch, respectively; they are now integrated into `feature/pd-68-integration`. PR #12 is the final delivery mechanism to `main`; its live GitHub status and exact aggregate diff count are authoritative. Authorized remote operations were limited to this repo, these branches, and these PRs via the active HTTPS `gh` session.
- TDD: Initial RED for T1/T2 was the missing `project_store.hpp`. Independent review then added regression tests; RED reproduced acceptance of an invalid digest (`invalid digest must be rejected`). GREEN implemented schema-driven v1 validation and filesystem safeguards.
- Corrected defects: validate against the checked-in JSON Schema (types, enums, patterns/digests, required fields, and extra properties); validate normalized `derived/` paths and reject symlink escapes/source collisions; enforce audio-root and history input/output lineage; use `openat`/`O_NOFOLLOW`, exclusive random temporary creation, `fsync`, and atomic `renameat`; reject symlink/non-regular targets and preserve the previous manifest as `.bak`.
- Failure tests: pre-create the former `.tmp.0` symlink and verify its target is unchanged; use `RLIMIT_FSIZE=0` to force a real temporary-write failure and verify manifest bytes remain identical; reject a directory and symlink destination without modifying their targets.
- Verification: `cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64` — passed; `cmake --build build --config Debug` — passed; `ctest --test-dir build -C Debug --output-on-failure` — 4/4 passed; `python3 -m unittest discover -s Tests/Harness -p 'test_*.py'` — 1 test passed; `git diff --check` — passed.
- T1/T2 work-unit commit: `1230ca6263816065f9bdd5e823f7760536aba6dc` (`feat(project-persistence): add validated manifest storage`).
- Independent-review correction commit: `29a5b5d6e244bc485eb76191def062cab81760ac` (`fix(project-persistence): validate schema and secure writes`).
- Correction commits pushed to PR #11: `d8a749c` (`fix(project-persistence): align save and load path validation`) and `010d757` (`docs(odd): record PD-68 correction evidence`). The independent verifier passed the full checks for both correction commits. No merge, rebase, or release was performed.
- Correction evidence: the focused test reproduced that `save_manifest("project.json", manifest)` accepted a symlinked derived asset while load rejected it, caused by save validating against an empty parent path instead of the canonical project directory. Save/load now share canonical directory validation; symlinked source aliases that collide with derived outputs are rejected. Focused test passed 1/1; CMake configure/build passed; CTest 4/4 and Python harness 1/1 passed.
- T3 work-unit commit: `392cb00fda3547a51782927162e4ac411392c50a` (`feat(project-persistence): dispatch manifest schema versions`, 117 authored lines). `migrate_manifest` returns an unchanged validated v1 copy, rejects unsupported older/future/malformed versions, and `load_manifest` uses the dispatcher. Direct and file-load regression tests cover rejection without modifying manifest, backup, source audio, or related files. No older on-disk contract exists in PD-67, so no legacy transform is invented. Independent verification requested file-load regression proof, which was added and rechecked. CMake/Debug build passed, CTest passed 4/4, Python harness passed 1/1, and `git diff --check` passed before commit.
- Final local `git diff --check` passed. `.atl/` remains untracked and untouched.
- Historical child-slice sizing: PR #9 had 554 changed lines and PR #10 had 533; both were accepted with `size:exception`. PR #11 had no exception. The maintainer accepted an aggregate `size:exception` for final PR #12 after the children were integrated; consult the live PR #12 view for its exact current aggregate count.


## PR Delivery Slices

| Position | Historical branch / commit boundary | Historical base | PR | Authored changed lines | Scope / outcome |
|---|---|---|---|---:|---|
| Final delivery | `feature/pd-68-integration`, based on `main` at `63cac7bef8f4700cfe60df005092429b38ca11dd` | `main` | [#12](https://github.com/R0dr1g0M0r3n0/PROGRAMADJ/pull/12) | See live GitHub PR #12 for the exact aggregate count; aggregate `size:exception` accepted | Final integrated persistence feature, tests, and this evidence document; final merge remains pending the gate |
| Historical slice 1 of 3 | `feature/pd-68-01-manifest`: `main..1230ca6263816065f9bdd5e823f7760536aba6dc` | `feature/pd-68-integration` | [#9](https://github.com/R0dr1g0M0r3n0/PROGRAMADJ/pull/9) | 554 — accepted `size:exception` | T1/T2 validated v1 persistence boundary and round-trip; merged to integration as `7682d4f` |
| Historical slice 2 of 3 | `feature/pd-68-02-hardening`: `1230ca6263816065f9bdd5e823f7760536aba6dc..2bf113de3c44b751f6d0efbdce503efd0cdc887a` | `feature/pd-68-01-manifest` | [#10](https://github.com/R0dr1g0M0r3n0/PROGRAMADJ/pull/10) | 533 — accepted `size:exception` | Schema validation, path/lineage checks, secure atomic write corrections; merged to integration as `0aabad7` |
| Historical slice 3 of 3 | `feature/pd-68-03-migration`, including correction and evidence commits | `feature/pd-68-02-hardening` | [#11](https://github.com/R0dr1g0M0r3n0/PROGRAMADJ/pull/11) | See its final PR body for the historical exact count; no `size:exception` requested | Version dispatch, file-load rejection preservation, correction, and delivery evidence; merged to integration as `1f763c0` |

Historical dependency order was tracker PR #12 → PR #9 → PR #10 → PR #11. PRs #9, #10, and #11 are now merged into the integration branch in that order: merge commits `7682d4f542e00b96b27c507262314c799d56277a`, `0aabad7fe7d768998414d52990f7c4ff698efd81`, and `1f763c0353ea241ea6d9eeec49079c0da5a8bc3e`. The tested post-child-merge tree is the #11 merge commit `1f763c0353ea241ea6d9eeec49079c0da5a8bc3e`; no subsequent source or test changes were made. PR #12 is the final delivery mechanism to `main`; consult GitHub for its live state and exact current aggregate count. Its final merge remains pending this gate.

Full post-child-merge checks on the exact tested tree passed:

- `cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64` — passed.
- `cmake --build build --config Debug` — passed.
- `ctest --test-dir build -C Debug --output-on-failure` — 4/4 passed.
- `python3 -m unittest discover -s Tests/Harness -p 'test_*.py'` — 1 test passed.
- `git diff --check` — passed.

These are local checks only. No CI or native review result is claimed. PR #12 remains draft and open; do not mark it ready or merge before this final gate. `main` remains at `63cac7bef8f4700cfe60df005092429b38ca11dd`. No force push, rebase, release, Jira operation, or `.atl/` change was performed.
