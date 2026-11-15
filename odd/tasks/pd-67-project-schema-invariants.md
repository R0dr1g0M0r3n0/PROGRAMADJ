# PD-67 Project Schema and Invariants

## Objective

Design the versioned, non-destructive AudioForge project schema and its invariants before persistence is implemented in PD-68.

## Scope

- Versioned project manifest for source/derived assets, tracks, regions, history, cues, loops, beatgrids, phrases, profiles, and validity provenance.
- Stable identifiers, ownership/reference rules, source-file preservation and derived-asset relationships.
- Explicit migration strategy and compatibility policy for future schema versions.
- Representative schema fixture and invariant documentation; no persistence engine yet.

## Constraints

- Preserve original audio, DJ library files and user metadata; never rewrite source assets.
- Keep derived edits non-destructive and provenance-linked to source identifiers.
- Use deterministic UTC timestamps and explicit schema versioning.
- Do not implement save/load or migration execution here; those belong to PD-68.
- Conventional Commits without AI attribution; strict TDD applies if executable validation is added.

## Tasks

- [x] **T1 — Schema and invariants:** Add the versioned project manifest schema, invariant documentation and representative fixture.
- [x] **T2 — Design evidence:** Validate schema structure, identifier/provenance rules and migration strategy; update this document and mirror it to Engram.
- [x] **T3 — Jira:** Record exact evidence and limitations in Jira PD-67, then transition only if acceptance is honestly met.

## Verification

- JSON/schema syntax and invariant checks.
- `git diff --check`.
- Preserve existing CMake/CTest and app build behavior.

## Progress

- Jira PD-67 updated with the public evidence comment `10021` and transitioned to `Listo` after both delivery PRs merged. PR #7 merged as `90e82e8d40a9ea4c695e12c0f560db7e03766206`; PR #8 merged as `e3cceb1f4d69465585398071bfe00558d226c269`.

- Route: delegated direct (new schema and documentation across multiple files).
- Files: `docs/project-manifest-v1.schema.json` (Draft 2020-12, exact v1 shape), `docs/project-schema.md` (semantic invariants and migration policy), `TestFixtures/project_manifest_v1.json` (synthetic relationships).
- Structural verification: `jq empty` passed on both JSON files; Python standard-library check passed for all 66 local schema references, 14 globally unique fixture records, asset lineage, typed references, UTC ordering, spans, beatgrid ordering and validity claim chain. `git diff --cached --check` passed after staging.
- Delivery strategy: `stacked-to-main`. Slice 1 is `feature/pd-67-01-schema-contract` from `main`, commit `c6619e7cdf26a700e107b010b3d7bdc07e7b37bb` (`docs(project): define manifest schema and invariants`), 347 authored lines. It defines the contract without claiming the fixture already exists.
- Slice 2 is `feature/pd-67-project-schema` based on slice 1, starting with commit `ced1c91e0adbed19f5843d2f0f875ae479c4c145` (`docs(project): illustrate manifest relationships`), which adds the fixture and restores its documentation link. Commit `6f21492760582096b97d84a306b551b269104010` records ODD evidence in this slice. The slice is 195 authored lines (`193` additions, `2` deletions). It targets `main` only after slice 1 is merged there; no remote PR is created here.
- Limitation: a Draft 2020-12 validation library is not installed, so full JSON Schema meta-validation and instance validation were not run. Cross-record semantic validation, safe file resolution, save/load, migration execution, actual audio processing and DJ library round trips remain unimplemented. CMake/CTest and app build are not run because this task changes only inert documentation and fixture data.
- Rollback boundary: remove the three new schema/design/fixture files and this PD-67 work record; no runtime behavior is changed.
- Next: PD-68 may implement persistence and executable semantic validation only after this contract is reviewed.
