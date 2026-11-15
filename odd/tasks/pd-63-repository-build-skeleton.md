# PD-63 Repository and Build Skeleton

## Objective

Create an independently buildable AudioForge Studio repository skeleton for App, Bridge, Engine, API, DJLibrary, Harness, Tests, fixtures, and documentation. This work is limited to Jira PD-63; it does not implement DSP or Rekordbox behavior.

## Problem and rationale

The workspace is empty. PD-63 requires a reproducible Xcode/CMake build graph and explicit module ownership before later implementation can begin. A disposable toolchain probe built a SwiftUI bundle, Objective-C++ bridge, and static C++20 engine from one CMake-generated Xcode project.

## Authorized scope and constraints

- Source root: this repository only. Do not modify existing music, Rekordbox data, or other projects.
- Xcode 26.6, CMake 4.5.0, Swift 6.3.3, and Apple Clang 21 are available.
- One CMake-owned Xcode graph; no handwritten Xcode project or duplicated Engine sources.
- Keep the C API versioned. Bridge owns Objective-C++ adaptation; Engine remains Swift- and Rekordbox-independent.
- Strict TDD is enabled by the supplied AGENTS.md instructions. Native runner: CTest; app build: `xcodebuild` on the generated project. Record RED before implementing executable code, then GREEN and REFACTOR.
- Delivery strategy: `ask-on-risk`; forecast approximately 350 authored changed lines, excluding generated build files. No push, PR, or merge is authorized.

## Tasks

- [x] **T1 — Repository boundaries:** Initialize local Git on `main`, create `feature/pd-63-build-skeleton`, establish directories and module ownership/build documentation. Route: delegated direct (multiple non-trivial files; reading prepares writing). Acceptance: documented ownership and dependency direction; no generated build output tracked. Checks: `git status --short --branch` reported `## feature/pd-63-build-skeleton` (plus this task document as untracked); `git ls-files` listed the 11 scaffold files; existence checks passed for all 8 module directories, `docs/module-boundaries.md`, `README.md`, and `.gitignore`; `git check-ignore -v build/probe` matched `.gitignore:2:/build/`; `git diff --check` passed before commit. Commit: `d027cf33872b0602b787145fb0dce0f2322616d0` (`chore(repo): establish AudioForge module boundaries`). T1 is documentation/scaffold-only, so Strict TDD is N/A; no executable behavior was added.
- [x] **T2 — Native graph:** Added the `AudioForgeEngine` C++20 static target, versioned C ABI (`API/audioforge/engine.h`) with opaque caller-owned handles and explicit status/ownership semantics, and a dependency-free C smoke test; documented the API and native build commands. Route: delegated direct (CMake, API, implementation, and test files). Strict TDD evidence: RED first — after writing the C API smoke test and initial CMake test target, `cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64 && cmake --build build --config Debug --target AudioForgeEngineTests` exited 65; compiler reported `fatal error: 'audioforge/engine.h' file not found`. GREEN — after implementation, the same required build sequence initially exposed missing `<stddef.h>` in the C test (`NULL` undeclared); adding that standard header fixed the test. Final commands all passed: `cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64` (exit 0); `cmake --build build --config Debug --target AudioForgeEngineTests` (`** BUILD SUCCEEDED **`); `ctest --test-dir build -C Debug --output-on-failure` (`1/1 Test #1: AudioForgeEngineTests ... Passed`, `100% tests passed`). `git diff --check` passed. Commit: `9ffbebb0306d04d9904249fa8a34fc2df47e11d0` (`feat(engine): add versioned native API and CMake target`). RDD status was explicitly off (global), so no review lifecycle was started.
- **T2 Release-check follow-up:** Replaced `assert`-only expectations (compiled away under `NDEBUG`) with explicit conditions that print a failing check and return `EXIT_FAILURE`. RED: intentionally inverted the major-version expectation, then `cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64 && cmake --build build --config Release --target AudioForgeEngineTests && ctest --test-dir build -C Release --output-on-failure` built successfully and CTest exited 8 with `FAIL: major version matches the API version`. Restored the correct expectation. GREEN: configure passed; Debug and Release target builds both succeeded; `ctest --test-dir build -C Debug --output-on-failure` and `ctest --test-dir build -C Release --output-on-failure` each reported `1/1` passed; `git diff --check` passed. Follow-up commit: `d673a59c54f5479a3635547218aaf76544d6d1d3` (`test(engine): keep smoke checks active in release`).
- [ ] **T3 — macOS graph:** Add SwiftUI app and Objective-C++ Bridge targets to the same CMake graph, with only C API access to Engine. Route: delegated direct (Swift, ObjC++, CMake, docs). Acceptance: direct `xcodebuild` on generated project succeeds with signing disabled; bundle identifier and linkage are inspectable. Checks: `xcodebuild -project build/AudioForgeStudio.xcodeproj -scheme AudioForge -configuration Debug -sdk macosx CODE_SIGNING_ALLOWED=NO build`, plus CTest regression.
- [ ] **T4 — Recovery and evidence:** Add minimal fixture/Harness/DJLibrary boundaries and record exact build commands and limitations. Route: delegated direct (multiple files). Acceptance: all named boundaries are present and ownership is documented, final builds pass, and Jira evidence is reviewable. Checks: directory/target readback, full CTest and Xcode build.

## Progress and next step

Task tracking created before source changes. T1 and T2 are complete; next: T3. Each completed task requires observed checks and a Conventional Commit on the feature branch; record commit IDs here. Mirror this full document to Engram topic `odd/pd-63-repository-build-skeleton/tasks` after each update.
