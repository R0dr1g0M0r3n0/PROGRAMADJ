# PD-81 — Import and asset linking

## Objective
Implement the smallest persistence contract needed for import/reopen to restore source and derived asset identity without modifying original audio or DJ-library bytes.

## Scope
- Add a pure manifest helper that resolves an asset identity by ID and exposes its immutable source lineage.
- Keep source bytes external and untouched; persistence stores only identity/path/digest metadata already covered by manifest v1.
- Add round-trip and failure tests using synthetic fixture data and temporary files.

## Constraints
- Maximum three implementation files: `ProjectPersistence/include/project_store.hpp`, `ProjectPersistence/src/project_store.cpp`, `Tests/ProjectPersistence/project_store_test.cpp`.
- No media decoding, copying, or rewriting.
- Strict TDD: RED, GREEN, REFACTOR.
- Conventional commits; no AI attribution.

## Acceptance criteria
- Import/reopen can resolve a source asset and any derived asset by stable ID.
- Resolution preserves `originalPath`, `sha256`, and optional `byteSize` exactly.
- Derived resolution preserves root source identity and parent lineage.
- Missing/unknown IDs fail explicitly without touching source files.
- Existing manifest validation and atomic persistence behavior remain green.

## Checklist
- [x] Define public asset-link result and resolver API.
- [x] Implement source/derived lookup with lineage validation.
- [x] Add round-trip, unknown-ID, and source-byte-preservation tests.
- [x] Run build and CTest; record evidence.
- [ ] Commit implementation and update Jira PD-81.

## Route
- Exploration: delegated read-only attempt was unavailable; bounded local exploration used as graceful degradation.
- Implementation: direct fallback only if delegated writer remains unavailable; otherwise one bounded writer.
- Verification: focused CTest plus existing project persistence suite.

## Verification
- `cmake --build build --config Debug --target AudioForgeProjectPersistenceTests`
- `ctest --test-dir build -C Debug --output-on-failure -R AudioForgeProjectPersistenceTests`

## Evidence

- RED observed by writer: build failed before the resolver API existed.
- GREEN: `cmake --build build --config Debug --target AudioForgeProjectPersistenceTests` succeeded.
- CTest: `1/1` passed.
- `git diff --check`: passed.
- Verification risk retained: no direct loaded-manifest or multi-level derived-chain test; current manifest validation covers lineage.
