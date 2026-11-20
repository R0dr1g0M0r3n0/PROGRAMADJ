# Module Boundaries and Build Graph

## Build ownership

CMake 3.29 or later is required for Xcode XPC embedding. CMake is the sole owner of the project graph and generates the Xcode project under `build/`. Do not add a handwritten `.xcodeproj` or duplicate target/source declarations in Xcode. The generated project, CMake intermediates, and other build products remain untracked through the root `.gitignore`.

The graph builds separate C++20 Engine and DJLibrary static libraries, an Objective-C++ Bridge, and a SwiftUI macOS app in one generated Xcode project. CTest exercises the Engine's C ABI, the Bridge's Objective-C surface, and the independently linked DJLibrary boundary. The app imports the public Bridge and AgentControl headers; the Bridge calls only the versioned C API. DJLibrary does not link Engine.
All targets share a macOS 14.0 deployment baseline. The app embeds a private [agent-control XPC service](agent-control-xpc.md); it is separate from the Engine and audio path. The tested app packaging path uses the Xcode generator. The non-Xcode packaging fallback is unverified.

## Native Engine API

`API/audioforge/engine.h` is the versioned C-compatible public boundary. The Engine exports its API version and an opaque, caller-owned handle. Creation reports `OK`, `INVALID_ARGUMENT`, or `OUT_OF_MEMORY`; a valid output slot is cleared before allocation, and callers release a successful handle exactly once with `audioforge_engine_destroy`. Destroy accepts `NULL`. No engine implementation types cross the ABI.

Processing request, result, and error values are versioned POD records with explicit struct sizes. Input/output sample buffers and callback contexts are borrowed only for the synchronous call and are never retained. The caller asserts writable capacity; the current contract clears the full representable output capacity on failure. `audioforge_engine_process` validates the API version, buffer/pointer combinations, cancellation token, and optional time transform, then returns `UNSUPPORTED` with a structured error. It does not process audio or invoke callbacks.

Cancellation tokens are caller-owned, initialized with `AUDIOFORGE_CANCELLATION_TOKEN_INIT`, and may be cancelled concurrently through the API's atomic state helpers. The caller must keep a token alive for every call that references it. Progress callback metadata is declarative: if a future synchronous processor invokes it, it runs on the calling control thread, not an audio thread. `AudioForgeRealtimeCallback` is also only a declaration for future work; any implementation must avoid allocation, locks, blocking, logging, and exceptions. No real-time behavior or timing guarantee exists today.

Time transforms use absolute frame coordinates in distinct source and output domains. Their source interval is half-open; mapping uses integer floor scaling and rejects invalid domains, out-of-range frames, and arithmetic overflow. The Objective-C++ Bridge maps API versions and C error codes into Foundation values and owns its cancellation token and time-transform snapshot; it does not expose borrowed sample pointers or add asynchronous behavior.

Configure the generated Xcode project for Apple Silicon and run the native smoke test with:

```sh
cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --config Debug --target AudioForgeEngineTests AudioForgeBridgeTests AudioForgeDJLibraryTests
ctest --test-dir build -C Debug --output-on-failure
xcodebuild -project build/AudioForgeStudio.xcodeproj -scheme AudioForge -configuration Debug -sdk macosx CODE_SIGNING_ALLOWED=NO build
python3 -m unittest discover -s Tests/Harness -p 'test_*.py'
python3 Harness/run_checks.py --config Debug
```

Repeat the native test build and CTest with `--config Release` and `-C Release` to validate optimized builds. The harness is a thin CTest CLI; it does not configure or build targets. Run its Python unit test separately. `TestFixtures/synthetic_collection.json` contains no audio paths or personal metadata and is not consumed by production code.

DJLibrary currently exports only a build marker to prove isolated compilation and linkage. It does not import, parse, export, or modify Rekordbox data. The fixture is a placeholder contract, not evidence of library compatibility. GUI launch, signed packaging, and real-library round trips are outside PD-63.

## Module ownership

| Boundary | Ownership | Allowed dependencies |
| --- | --- | --- |
| `App/` | SwiftUI application shell, app lifecycle, and presentation | `Bridge/` and `AgentControl/` public surfaces; platform frameworks |
| `AgentControl/` | Private XPC discovery and peer identity checks | Foundation, Security, libxpc; no playback or Engine dependency |
| `Bridge/` | Objective-C++ adaptation between Swift/Objective-C and the stable C ABI | `API/`; Apple runtime/frameworks required for adaptation |
| `Engine/` | Reusable audio/domain implementation and static library | C++ standard library and explicitly approved engine internals; no Swift, AppKit, or Rekordbox dependency |
| `API/` | Versioned, C-compatible public boundary shared by the bridge, engine, and native tests | C language/ABI types only; no Swift or Objective-C++ declarations |
| `DJLibrary/` | Isolated library/catalog module; currently a build-only marker, with import behavior reserved for later work | C++ standard library only; no Engine dependency or ownership of Engine internals |
| `Harness/` | Python CLI that invokes the configured CTest suite | Python standard library and CTest; must not become a second engine implementation |
| `Tests/` | Native and app-level automated tests | Public APIs by default; focused AgentControl tests may include private headers when exercising an internal security boundary |
| `TestFixtures/` | Small, synthetic test-only inputs; currently an inert empty collection manifest | No production code; never store a user's music library or Rekordbox database here |
| `docs/` | Architecture, build, and repository guidance | No runtime dependencies |

## Dependency direction

The app uses Bridge for Engine access and AgentControl for private XPC discovery. The bridge calls the versioned C API; Engine implements that API. C++ implementation types remain behind the C ABI and are not exposed to Swift. Tests use public boundaries by default, with focused AgentControl tests permitted to include private headers to exercise internal security behavior. DJLibrary remains separate from the audio Engine so library/import behavior cannot become an implicit engine dependency.

No module may depend upward on `App/`. Avoid cycles: shared contracts belong in `API/`, while implementation stays in its owning module.

## Repository safety

This repository is an application-code scaffold, not a destination for personal audio assets. Keep user music, Rekordbox databases, exports, and credentials outside the repository and outside `TestFixtures/`.
