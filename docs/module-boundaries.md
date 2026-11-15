# Module Boundaries and Build Graph

## Build ownership

CMake is the sole owner of the project graph and generates the Xcode project under `build/`. Do not add a handwritten `.xcodeproj` or duplicate target/source declarations in Xcode. The generated project, CMake intermediates, and other build products remain untracked through the root `.gitignore`.

The graph builds a static C++20 Engine, an Objective-C++ Bridge, and a SwiftUI macOS app in one generated Xcode project. CTest exercises the Engine's C ABI and the Bridge's Objective-C surface. The app imports only the Bridge header; the Bridge calls only the versioned C API.
All targets share a macOS 14.0 deployment baseline.

## Native Engine API

`API/audioforge/engine.h` is the versioned C-compatible public boundary. The Engine exports its API version and an opaque, caller-owned handle. Creation reports `OK`, `INVALID_ARGUMENT`, or `OUT_OF_MEMORY`; a valid output slot is cleared before allocation, and callers release a successful handle exactly once with `audioforge_engine_destroy`. Destroy accepts `NULL`. No engine implementation types cross the ABI.

Configure the generated Xcode project for Apple Silicon and run the native smoke test with:

```sh
cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --config Debug --target AudioForgeEngineTests
ctest --test-dir build -C Debug --output-on-failure
xcodebuild -project build/AudioForgeStudio.xcodeproj -scheme AudioForge -configuration Debug -sdk macosx CODE_SIGNING_ALLOWED=NO build
```

## Module ownership

| Boundary | Ownership | Allowed dependencies |
| --- | --- | --- |
| `App/` | SwiftUI application shell, app lifecycle, and presentation | `Bridge/` public surface; platform frameworks |
| `Bridge/` | Objective-C++ adaptation between Swift/Objective-C and the stable C ABI | `API/`; Apple runtime/frameworks required for adaptation |
| `Engine/` | Reusable audio/domain implementation and static library | C++ standard library and explicitly approved engine internals; no Swift, AppKit, or Rekordbox dependency |
| `API/` | Versioned, C-compatible public boundary shared by the bridge, engine, and native tests | C language/ABI types only; no Swift or Objective-C++ declarations |
| `DJLibrary/` | Future Rekordbox/library metadata import and catalog boundary | Its own data/domain dependencies; no ownership of Engine internals |
| `Harness/` | Development and integration entry points used to exercise the native boundary | `API/` and test tooling; must not become a second engine implementation |
| `Tests/` | Native and app-level automated tests | Public APIs of the component under test and approved test frameworks |
| `TestFixtures/` | Small, non-sensitive test-only inputs and expected data | No production code; never store a user's music library or Rekordbox database here |
| `docs/` | Architecture, build, and repository guidance | No runtime dependencies |

## Dependency direction

The app calls the bridge. The bridge calls the versioned C API; Engine implements that API. C++ implementation types remain behind the C ABI and are not exposed to Swift. Native tests and the harness exercise public boundaries rather than reaching into implementation details. DJLibrary remains separate from the audio Engine so library/import behavior cannot become an implicit engine dependency.

No module may depend upward on `App/`. Avoid cycles: shared contracts belong in `API/`, while implementation stays in its owning module.

## Repository safety

This repository is an application-code scaffold, not a destination for personal audio assets. Keep user music, Rekordbox databases, exports, and credentials outside the repository and outside `TestFixtures/`.
