# PD-70 Recovery and Retry-Limit Tests

## Objective

Implement the smallest runtime replay boundary that proves recovery from committed task transition records and enforces the three-correction-attempt limit defined by PD-69.

## Scope and constraints

- Authorized scope: local changes on `feature/pd-69-task-state`; no remote Jira/GitHub operations.
- Do not touch or stage `.atl/` or the original audio/DJ library.
- Keep the implementation separate from project-manifest persistence; PD-69 transition records are the source of task state.
- Use strict TDD: RED → GREEN → REFACTOR with the configured CMake/CTest runner.
- Bounded scope selected by the user: three files maximum for this first implementation slice.

## Work units

- [x] **T1 — Replay committed transition records and enforce retry limits.** Added a header-only replay boundary and C++ tests for restart recovery, terminal states, unknown versions, and attempts 0–3. Focused executable and full Debug CTest pass.
- [ ] **T2 — Verify and record closure evidence.** Run build/tests, capture exact results and commit identity, update this document/mirror, and request separate Jira authorization before closure.

## Next step

T1 implementation is complete. The first focused run exposed a test-fixture construction bug: `{}` created a JSON null instead of an empty array for `prerequisiteIds`; this was corrected with `Record::array()`. Next: commit the bounded slice, then capture final T2 evidence and request separate Jira authorization before closure.
