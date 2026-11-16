# PD-82 — Missing-file and source-preservation tests

## Objective
Add reproducible persistence tests proving that an import manifest can reopen with a missing external source reference and that neither missing-source handling nor failed persistence modifies original audio or DJ-library bytes.

## Scope
- Extend the existing ProjectPersistence test executable only.
- Verify missing source and missing DJ-library files remain unresolved external references: manifest load and asset identity recovery do not create replacements.
- Verify successful and rejected saves preserve source audio and DJ-library bytes byte-for-byte.
- Do not add media decoding, hashing implementation, or file repair behavior; SHA verification remains the documented persistence boundary.

## Constraints
- Maximum one implementation file: `Tests/ProjectPersistence/project_store_test.cpp`.
- Strict TDD for the regression test; no production changes unless the test exposes a defect.
- Existing `.atl/` remains untouched.
- Conventional commit; no AI attribution.

## Acceptance criteria
- Missing-file case has exact assertions and reproducible output.
- Original audio and DJ-library files are unchanged after reopen and failed save.
- Existing ProjectPersistence checks remain green.

## Checklist
- [x] Add missing external source/library reopen test.
- [x] Add byte-for-byte preservation assertions for success and failure paths.
- [x] Run build and CTest; record exact evidence.
- [ ] Commit and update Jira PD-82.

## Verification
- `cmake --build build --config Debug --target AudioForgeProjectPersistenceTests`
- `ctest --test-dir build -C Debug --output-on-failure -R AudioForgeProjectPersistenceTests`
- `git diff --check`

## Evidence

- RED: initial test registration failed to compile because the new test function was not yet defined (exit 65).
- GREEN: Debug build succeeded.
- CTest: `AudioForgeProjectPersistenceTests` 1/1 passed.
- The test covers missing audio and DJ-library references, reopen identity, successful save preservation, post-reopen preservation, and rejected-save preservation.
- `git diff --check`: passed.
