# PD-69 Task State Records

## Objective

Define the task lifecycle and durable transition-record contract for PD-69, separate from project-manifest history, then implement and verify that contract in the follow-up work units.

## Problem and why

Project manifest history records project-level audit intent and evidence; it does not establish the current state or successful execution of an individual task. PD-69 needs a minimal task-state contract that can be implemented and proven without confusing those records or claiming recovery behavior before it exists.

## Scope and constraints

- Authorized scope: local changes on `feature/pd-69-task-state` only; no remote Jira/GitHub operations, push, PR, or transition.
- Leave `.atl/` and the original audio/DJ library untouched; do not stage `.atl/`.
- T1 records the architectural contract only. T2 implements the versioned record schema with fixture and structural tests. T3 records verification evidence and closes the local Jira work only when separately authorized.
- No runtime implementation, crash recovery, or retry-limit behavior is claimed by T1.
- Strict TDD is enabled by the user instructions. Runner: `cmake --build build && ctest --test-dir build --output-on-failure`. T1 is passive documentation-only, so no build/test run is applicable.
- Chosen route: T1 direct, single-file documentation change; no 4-file mapping or multi-file writer trigger. Route applies to this bounded work unit, not future T2.
- Delivery strategy: `ask-on-risk`; estimated feature total ~300 authored changed lines (planning estimate, not a cap).

## Work units

- [x] **T1 — Define task lifecycle contract** (`docs/task-state-machine.md`). Acceptance: explicit states/transitions/invariants, attempt bound, prerequisite and input-commit binding, execution evidence, append-only durable record authority, and fail-closed recovery boundaries. Checks: structural readback PASS; `git diff --check` PASS. Route: direct documentation-only.
- [ ] **T2 — Add versioned transition records and structural proof.** Implement the record schema and fixture/tests against T1; preserve append-only authority and reject corruption/unknown versions. Checks: RED → GREEN → REFACTOR with the configured CMake/CTest runner.
- [ ] **T3 — Verify and record closure evidence.** Run applicable project checks, capture exact results and commit identity, and update this document/mirror. Jira closure is not authorized in this work unit.

## Progress and evidence

- Initial state: branch `feature/pd-69-task-state` at `ad57c1f`, matching `origin/main`; unrelated untracked `.atl/` is preserved.
- T1 complete: structural readback confirmed the declared lifecycle, attempts 0–3, preconditions, input-commit binding, evidence requirements, durable append-only record authority, fail-closed replay, and crash non-success boundary. `git diff --check` passed.
- Runtime implementation/recovery and behavior were not verified; T1 is documentation-only, so no build/test was run.
- Next step: T2 versioned transition-record implementation and structural tests, authorized as separate follow-on work.
