# Cross Music Editor Agent Control API

## Objective
Deliver a secure, local API that lets an authorized agent inspect Cross Music Editor and control implemented application capabilities, beginning with media access and playback. Keep the API extensible for future library, editing, warp, and restoration capabilities without advertising unsupported behavior.

## Problem and motivation
The app currently has no external agent-control channel. `PlaybackController` owns live transport and selection; `AudioForgeBridge` exposes Engine capability/analysis, not live app state, and `PlaybackOutput` is only an internal test seam. The user requires the assistant to operate the application through an API while they only listen and provide yes/no validation.

## Scope
- Jira epic PD-187 and its work in order: PD-188, PD-191, PD-194, PD-197, PD-200, PD-203.
- Approved high-level boundary: MCP over stdio for agent tools; local macOS XPC between the companion and GUI app; no network listener.
- Initial capabilities must reflect the real app: authorized media access, playback, pause, seek, selection preview, cancellation, and observable state.
- Future capabilities (library, sample editing, warp, restoration) are added only when their owning product work exists and is verified.
- Preserve `PlaybackController` ownership and macOS security-scoped file access.

## Constraints
- Preserve all existing audio and Rekordbox/library data; no destructive work without explicit, scoped authorization.
- Deployment is for this Mac only; do not add multi-Mac distribution, notarization, or a network service unless separately requested.
- No network-exposed API, arbitrary agent-supplied file paths, or weakened identity checks for convenience.
- Keep user changes in the original worktree untouched. The original worktree retains unrelated UI prototype changes; implementation is isolated in `/private/tmp/agent-control-api` on `feature/agent-control-api`. The current clean stack is based on local `origin/main` at `ad57c1f`; PR3 is measured against PR2 tip `04b7d65`.
- CMake owns native targets. No XPC/MCP target, helper packaging, signing, or XPC tests existed at exploration time. The local app build is documented with code signing disabled; runtime peer authentication must fail closed until a signed helper identity is actually available. Xcode shows the user's Personal Team; this is local-development signing context, not a distribution identity.
- Strict TDD is enabled by the workspace instructions: RED → GREEN → REFACTOR. Source: user-provided AGENTS.md instructions. Test runner: CMake/CTest, with applicable Xcode and Python harness checks.
- Publication of `feature/agent-control-api` was explicitly authorized for the then-current GitHub session; the PR1 push was rejected with HTTP 403 for `InformaticaAcre`. This task is local-only: do not retry without new explicit authorization for a writable session, and do not create a PR or merge.

## Acceptance criteria
- An external agent client can discover and call the local API; calls reach the real application process and results reflect its live state.
- The API has versioned schemas, stable typed errors, capability discovery, validation, cancellation, and deterministic concurrency behavior.
- XPC peer authorization is verifiable; no unsigned/unverified client is accepted as trusted and no network listener exists.
- Media access is restricted to user-authorized resources; absolute paths are not exposed by default.
- Current capabilities are tested end to end; unsupported future domains are not advertised.
- User can validate audible behavior by listening; do not ask them to click or manipulate controls.

## Checklist
- [x] T0 — PD-188: Define and approve the versioned API/security contract (PD-189 inventory and PD-190 MCP/XPC contract). User approved the high-level design on 2026-11-19.
- [x] T0.1 — PD-189: Inventory available operations and observable playback state.
- [x] T0.2 — PD-190: Specify versioning, tool contract, authorization, and local transport.
- [x] T1 — PD-191: Implement local channel and service lifecycle. Both subtasks are complete and evidence is recorded in Jira.
- [x] T1.1 — PD-192: Implement XPC discovery and secure transport. Signed live rejection passed in both directions; delivery remains with the parent.
- [x] T1.2 — PD-193: Handle helper/app lifecycle, permissions, shutdown, and recovery. Jira marked `Listo` after evidence was recorded; lifecycle is delegated to launchd and each API call performs one bounded discovery attempt, with no automatic retry loop.
- [ ] T2 — PD-194: Connect approved agent commands to application state.
- [ ] T2.1 — PD-195: Expose approved playback/navigation commands.
- [ ] T2.2 — PD-196: Validate media loading, selection, and operation authorization.
- [ ] T3 — PD-197: Expose live state queries and updates.
- [ ] T3.1 — PD-198: Implement playback/session status query.
- [ ] T3.2 — PD-199: Synchronize state changes, failures, and cancellation.
- [ ] T4 — PD-200: Secure and verify the API end to end.
- [ ] T4.1 — PD-201: Add contract, permission, and failure tests.
- [ ] T4.2 — PD-202: Verify a real external-agent-to-app workflow.
- [ ] T5 — PD-203: Integrate agent usage and document the API.
- [ ] T5.1 — PD-204: Implement the agreed agent adapter/client.
- [ ] T5.2 — PD-205: Document setup, tools, permissions, and examples.

## Route and delivery
- Route: delegated direct. The architecture/build map required reading 4+ files; PD-192 spans the XPC contract, app integration, CMake wiring, and tests, triggering the bounded writer route.
- One writer at a time; no parallel source writers.
- Delivery strategy: `ask-on-risk`; user selected `stacked-to-main` on 2026-11-19. The original combined work was split into PR1 (foundation), PR2 (app embedding and focused tests), and PR3 (signed live-peer tests). Each slice is measured against its predecessor, and each future PR targets `main` only after its predecessors are merged. No remote operation is authorized for the current task.
- Work-unit commits use Conventional Commits; do not add Co-Authored-By or AI attribution.

## Verification
- Focused: `cmake --build build --config Debug --target AudioForgeAgentControlTests`
- Focused: `ctest --test-dir build -C Debug --output-on-failure -R '^AudioForgeAgentControlTests$'`
- Full native: `cmake --build build --config Debug`
- Full native: `ctest --test-dir build -C Debug --output-on-failure`
- App build: `xcodebuild -project build/AudioForgeStudio.xcodeproj -scheme AudioForge -configuration Debug -sdk macosx CODE_SIGNING_ALLOWED=NO build`
- Python harness: `python3 -m unittest discover -s Tests/Harness -p 'test_*.py'`
- Project checks: `python3 Harness/run_checks.py --config Debug`
- Hygiene: `git diff --check`
- The focused XPC/API test target now exists. Record exact RED, GREEN, REFACTOR, signed app/helper identity checks, and unavailable runtime checks. Do not treat the historical Bridge 4/4 result as proof for XPC.

## Current status and evidence
- PD-188 and PD-190 are Listo after user approval; PD-189 is Listo.
- PD-191 and both subtasks PD-192/PD-193 are marked `Listo` in Jira. The isolated PD-192 foundation is committed as `aa4c852` (`feat(agent-control): add signed XPC service foundation`); the commit also carries the PD-193 lifecycle documentation/evidence.
- Read-only exploration found CMake as build owner, macOS 14.0 minimum, no existing XPC/MCP helper/signing target, and no focused XPC test. Existing playback state is MainActor-owned; security-scoped access is in the App layer.
- At initial feasibility discovery, no source code had been changed. PD-192 implementation now exists only in this isolated worktree; the original worktree's unrelated dirty/untracked UI prototype files remain preserved and excluded.
- Historical initial writer handoff: inspection found CMake defined only the app target and no XPC helper/service target, signing configuration, entitlements, or generated Xcode project existed in this worktree. At that point no RED/GREEN/REFACTOR checks or source changes had occurred. This was superseded by the implementation and verification recorded below.
- RDD status and risk tier must be read before committing the first source work unit; do not toggle RDD.

### PD-193 authorized scope and checklist

- Jira status: `Listo`; PD-192 is `Listo`; parent PD-191 is `Listo` after the PD-193 result was recorded.
- User-approved recovery policy: one discovery request per API call, bounded to two seconds; no automatic retry loop. Return the existing typed `Unavailable`, `TimedOut`, or `Unauthorized` status. A later caller request may create a fresh XPC connection and let launchd activate the helper again.
- Process ownership: the embedded application-scoped XPC service is managed by macOS launchd. Do not add PID tracking, an app-owned process supervisor, manual start/stop controls, or a new permission prompt. Preserve fail-closed signed-peer authorization on every new connection.
- Do not expand into playback/media operations, state management, or broader API work assigned to later Jira items.
- [x] P193.1 — Verify the existing `Application` XPC service packaging and launchd-managed on-demand lifecycle against Apple documentation; record evidence and platform limits.
- [x] P193.2 — Verify one-shot discovery, timeout/error mapping, and safe later-call recovery by source readback and focused protocol/status tests; no production or test-code change was needed.
- [x] P193.3 — Reconcile the XPC lifecycle documentation and record exact checks; do not claim manual app shutdown or process cleanup beyond launchd's documented service lifecycle.
- TDD mode: strict; source `AGENTS.md` instructions; test runner CMake/CTest. Run `ctest --test-dir build -C Debug --output-on-failure -R '^AudioForgeAgentControlTests$'`; run signed live peer tests only if the relevant lifecycle change requires them.
- Evidence: `AgentControl/ServiceInfo.plist.in` declares `XPCService.ServiceType=Application`; `Tests/AgentControl/agent_control_test.mm` checks the embedded bundle, application scope, and executable. `AgentControl/src/AgentControlClient.mm` verifies the app identity before creating one connection, sets the signed-peer requirement on that connection, sends one request, waits at most two seconds, cancels, and returns `TimedOut` on deadline; each later API call runs this function anew and creates a new connection. `AgentControl/src/AgentControlWire.mm` maps XPC transport errors to `Unavailable` and peer-signing rejection to `Unauthorized`; the focused test checks those mappings. The helper also verifies its signed app peer in `AgentControl/src/AgentControlService.mm` on each connection.
- Apple documents that launchd starts XPC services on demand, shuts them down when idle, and restarts them after a crash ([XPC overview](https://developer.apple.com/documentation/xpc)); the app does not need a PID/process supervisor. Documentation updated in `docs/agent-control-xpc.md` to describe this system ownership, per-call retry boundary, timeout, and existing typed statuses.
- Verification: `ctest --test-dir build -C Debug --output-on-failure -R '^AudioForgeAgentControlTests$'` — exit 0, 1/1 passed. `git diff --check` — exit 0. No signed live-peer rerun was needed because peer-authentication code and fixtures were not changed. No deliberately hung-helper integration test or live idle/crash lifecycle test was run; the two-second and one-attempt guarantees were verified by source readback plus the focused protocol/status tests, and launchd lifecycle behavior by Apple documentation.
- Jira evidence: PD-193 comment 10063 records the result and limitations; PD-191 comment 10064 records both child outcomes before the parent was transitioned to `Listo`.
- Route: inline evidence review and documentation/tracker updates only; no source, production, or regression-test change was necessary. Strict TDD RED/GREEN/REFACTOR was not applicable because no behavior gap was found. The lifecycle documentation/evidence is included in the isolated publication slice `aa4c852`.

### PD-192 initial signing feasibility (historical; superseded)

- In the isolated worktree on `feature/agent-control-api` at `408c720`, CMake generated a disposable Xcode project. No embedded XPC service or `AudioForgeAgentControlTests` target exists in the generated graph.
- Xcode 26.6 resolved no development team for `AudioForge` with `CODE_SIGN_STYLE=Automatic`, `CODE_SIGNING_ALLOWED=YES`, `CODE_SIGNING_REQUIRED=YES`, and `CODE_SIGN_IDENTITY=Apple Development`. The generic macOS build omitted provisioning flags and failed with exit 65: `Signing for "AudioForge" requires a development team. Select a development team in the Signing & Capabilities editor.` No signed helper or peer validation was produced. Xcode reported `PROVISIONING_PROFILE_REQUIRED=NO` for the app in this probe; the reported zero provisioned devices was not the observed failure. This does not establish that Personal Team signing is unsupported.
- Focused build: exit 1, `Error: /private/tmp/agent-control-api/build is not a directory`.
- Focused CTest: exit 1, `Failed to change working directory to "/private/tmp/agent-control-api/build": No such file or directory`.
- This first probe occurred before Xcode target signing was configured. It did not include RED/GREEN/REFACTOR, a signed runtime check, or source changes; the failures below were subsequently resolved by selecting the Personal Team for the local app target and required dependencies.
- Unblock by providing a verified, locally signed app/helper pair or authorizing explicit local target team selection for a signing probe. Keep provisioning flags off and verify the expected app/helper signature requirements before editing transport code.
- The user explicitly authorized assigning the existing Personal Team to the local target for a signing probe, without device registration or distribution. In the isolated generated project `build/AudioForgeStudio.xcodeproj`, Xcode showed `AudioForge` Team = Personal Team, Automatically manage signing = off, Provisioning Profile = None, and Signing Certificate = Apple Development. No device provisioning, automatic profile management, or distribution setting was enabled. The earlier ScreenCaptureKit -10005/-3811 error was transient; the separate Desktop `CROSS.xcodeproj` was not modified. Subsequent signed build and runtime results are recorded below; this is historical feasibility context.
- Initial signing build exited 65 because the dependency targets needed a team. After the user's explicit authorization, Xcode Build Settings show the same Personal Team for AudioForge, AudioForgeBridge, and AudioForgeEngine, with automatic management and provisioning disabled. The separate Desktop `CROSS.xcodeproj` remains untouched.
- The retry of `xcodebuild -showBuildSettings` and the app-only local signing build succeeded; its signature is superseded by the app+helper signature evidence in the implementation section. No Keychain prompt occurred.

### PD-192 implementation and verification

- [x] PD-192 follow-up F1 — Add live negative XPC coverage proving the client rejects a service with the wrong signed identity and the service rejects a live client with the wrong signed identity; reconcile module-boundary documentation; then rerun focused and applicable checks. Keep timeout/cancellation behavior scoped to the existing v1 contract; do not add PD-193/199 lifecycle behavior.
- Route: existing delegated-direct writer handoff; no nested agents. Worktree `/private/tmp/agent-control-api`, branch `feature/agent-control-api`, base `408c720`. At the implementation handoff, no commit or Jira change had occurred; the source does not contain the local Team ID. Later evidence and Jira status are recorded above and below.
- CMake packages the private `com.audioforge.studio.agent-control.xpc`. The local discovery handshake verifies both peer identifiers under the same verified Apple signing team. Discovery v1 has a two-second reply deadline, no retry, and an empty application-capability list. Playback, media permissions, persistent lifecycle, and MCP wiring are not part of PD-192.
- RED: the first CMake build ran against the stale generated graph and exited 65 (target not found); CTest had no tests. After CMake regeneration, the focused build exited 0 and focused CTest exited 8 (0/1): the helper bundle identifier, application-scoped configuration, and executable were all absent. With protocol/identity tests added, the focused build exited 65 on missing `AgentControlInternal.h` before production implementation.
- GREEN: intermediate builds exposed the missing app-to-helper Xcode build dependency and the `xpc_main` function-pointer signature; both were fixed in CMake/source. Focused build then exited 0 and focused CTest exited 0 (1/1).
- REFACTOR: encode replies directly into the XPC reply dictionary and extend strict wire/team validation coverage. Focused build exited 0 and focused CTest exited 0 (1/1).

| Final verification command | Result |
| --- | --- |
| `cmake --build build --config Debug --target AudioForgeAgentControlTests` | Exit 0 |
| `ctest --test-dir build -C Debug --output-on-failure -R '^AudioForgeAgentControlTests$'` | Exit 0, 1/1 |
| `cmake --build build --config Debug` | Exit 0 |
| `ctest --test-dir build -C Debug --output-on-failure` | Exit 0, 8/8 |
| `xcodebuild -project build/AudioForgeStudio.xcodeproj -scheme AudioForge -configuration Debug -sdk macosx CODE_SIGNING_ALLOWED=NO build` | Exit 0 |
| `python3 -m unittest discover -s Tests/Harness -p 'test_*.py'` | Exit 0, 16/16 |
| `python3 Harness/run_checks.py --config Debug` | Exit 0, 8/8 |
| `git diff --check` | Exit 0; untracked files checked separately |

- The signing-disabled Debug app probe exited 1 with `XPC discovery status: 4` (unauthorized). The signed Xcode 26.6 build with the authorized local Xcode override exited 0 without a Keychain prompt or provisioning flags. Deep strict `codesign` verification exited 0. The app and embedded helper had signing identifiers `com.audioforge.studio` and `com.audioforge.studio.agent-control`, respectively; both had Apple Development authority and the same approved local TeamIdentifier. For each artifact, the exact requirement passed (exit 0); the swapped identifier and wrong team each failed (exit 3).
- The signed Debug app probe exited 0 with `XPC discovery status: 0` in 0.35 s on macOS 26.4. This proves the actual packaged app/helper discovery round trip, not a live connection using a mismatched signed peer (negative ID/team evidence is from static `codesign` checks). No live playback, external MCP flow, or other generator/macOS-version check was run.
- Interim independent verification found no authentication bypass in the reviewed paths, but identified a medium completion gap: tests did not establish that both client and service reject a live mismatched signed peer. That gap was addressed by F1 below. Do not expand into playback, persistent lifecycle, or cancellation work owned by later tickets.
- Low finding: the non-Xcode CMake packaging branch is not verified because the Swift bridging-header configuration is Xcode-specific. Document this as unsupported/unverified rather than claiming multi-generator support; do not broaden PD-192 just to add another build backend.
- F1 reconciled `docs/module-boundaries.md`: the app imports both Bridge and AgentControl headers, and only the Xcode app packaging path has been verified.

### F1 signed live peer verification

- Test-first (RED): built the two signed fixtures with matching peer identifiers as negative controls, without changing production. Signed CTest exited 8 (0/1; both scenarios failed as intended, 1.04 s total). The client returned `discovery: 0` instead of `discovery: 4`; the production helper returned `reply: 0` instead of refusing the client.
- GREEN on macOS 26.4: changed only fixture signing identifiers under the existing local team, keeping bundle lookup names unchanged. The client returned `discovery: 4` (unauthorized) for the live wrong-ID helper, after a control pinned that fixture's actual identity and got `reply: 0`. The matching client got `reply: 0` from a byte-identical copy of the production helper; the wrong-ID client received `XPC_ERROR_CONNECTION_INTERRUPTED`, not a v1 reply. The production helper's local XPC log for that connection confirmed `Received message forbidden due to code signing requirement`. The signed production app/helper probe still reported `XPC discovery status: 0`. Signed CTest exited 0 (1/1 CTest entry, 2/2 scenarios, 0.94 s).
- REFACTOR: excluded the opt-in fixtures from the default build, documented how to run the signed test, and corrected the module boundary and Xcode support notes. No production transport code was changed.
- Signed build used the existing approved local Apple Development identity with signing allowed and required and an empty provisioning profile specifier. No Keychain prompt, credentials inspection, provisioning flag, network listener, or additional signing identity.

| F1 check | Observed result |
| --- | --- |
| `cmake --build build --config Debug --target AudioForgeAgentControlTests` | Exit 0; `** BUILD SUCCEEDED **` |
| `ctest --test-dir build -C Debug --output-on-failure -R '^AudioForgeAgentControlTests$'` | Exit 0; 1/1 passed (0.27 s total) |
| Signed Debug Xcode build of `AudioForgeAgentControlLivePeerTests` | Exit 0; `** BUILD SUCCEEDED **` |
| `ctest --test-dir build/f1-signed -C Debug --output-on-failure -V -R '^AudioForgeAgentControlLivePeerTests$'` | Exit 0; 1/1 CTest entry and 2/2 live scenarios passed after refactor (0.45 s total) |
| `git diff --check` | Exit 0; no output |
| Full native suite, non-Xcode app packaging, other macOS versions | Not run for F1 |

### PD-192 F1 follow-up — distinguish signing denial from interruption

- RED: tightened the wrong-client expectation to require XPC's explicit peer-signing error. The signed live test failed because macOS reports `connection interrupted`, not `peer requirement rejected`; the sender-side result alone cannot identify the cause.
- GREEN/REFACTOR: accept the observed interruption only when a new helper unified-log denial appears, comparing denial records before and after the wrong-client request. This rejects bare interruptions and requires receiver-side signing evidence. Independent verification passed the live suite; a medium residual caveat remains because the helper-wide log record is not uniquely correlated to that request, and delayed log visibility can fail closed. Production transport and signing configuration are unchanged.
- Corrected the module-boundary assertion: tests use public APIs by default, while focused AgentControl security tests may include private headers. Updated signed-test documentation to describe the log correlation and its unified-logging dependency.

| Follow-up verification command | Result |
| --- | --- |
| `ctest --test-dir build/f1-signed -C Debug --output-on-failure -V -R '^AudioForgeAgentControlLivePeerTests$'` | Exit 0; 1/1 CTest entry, 2/2 live scenarios passed; wrong-client interruption correlated with a new helper signing-denial log; independent parent spot-check passed (4.272 s for live scenarios) |
| `git diff --check` | Exit 0; no output |

- The test requires macOS unified logging to retain and expose the helper denial; environments where that record is unavailable fail closed. No broader suite/build was run.

## Next step
- PR1 remains local as `aa4c852` (397 authored lines). An earlier push, explicitly authorized for the current GitHub session, was rejected with HTTP 403 for `InformaticaAcre`; no remote branch or PR was created. Do not retry without new explicit authorization for a writable session.
- PR2 consists of `d66b86d` (`feat(agent-control): embed discovery service in app`) and tracker-only `04b7d65`; the combined slice is 299 authored lines (+298/-1) against `aa4c852`. Verification on 2026-11-20: `cmake -S . -B build -G Xcode` succeeded; focused build succeeded; focused CTest passed 1/1; `git diff --check` succeeded.
- PR3 is locally verified and committed as recorded below. Publication remains pending and was not attempted. No Jira or original-worktree operation was performed for PR3.

### PR3 signed live-peer test slice — 2026-11-20

- Route: delegated direct, with the existing untracked fixtures preserved and the root hook registered under `BUILD_TESTING`. Strict TDD remains enabled by workspace instructions; the runner is CMake/CTest. RDD was off by global setting and was not toggled.
- Source/test commit: `7e308e8cc122cd0bd1be9d589a5738b6ccccedb3` (`test(agent-control): verify signed XPC peer rejection`), 239 additions against `04b7d65`. The complete PR3 slice including the tracker is 275 authored changed lines (+267/-8); the whole local stack against `ad57c1f` is not the PR3 review diff.
- RED (current registration): after fresh signed Xcode configure, `ctest --test-dir build/f1-signed -C Debug --output-on-failure --no-tests=error -R '^AudioForgeAgentControlLivePeerTests$'` exited 8 with `No tests were found!!!` before adding the root hook. Historical F1 behavioral RED is recorded above but was not replayed. The previous local `LastTest.log` corroborated historical GREEN/REFACTOR, including both passing scenarios and the helper denial (4.272 s for the scenarios).
- GREEN: after adding the hook, the signed suite registered and both asymmetric identity checks passed. The wrong helper replied under its test requirement (`reply: 0`) but production discovery returned `discovery: 4`. The signed production app probe returned `XPC discovery status: 0`; the matching client got `reply: 0`; the wrong client was interrupted and a new helper signing-denial log was observed.
- REFACTOR verification: the default configuration did not register the live suite. The existing opt-in fixtures are excluded from the default build and the signed CTest registration is Debug-only. No further fixture or production refactor was needed.

| Verification command | Result |
| --- | --- |
| `cmake --build build --config Debug --target AudioForgeAgentControlTests` | Exit 0; `** BUILD SUCCEEDED **` |
| `ctest --test-dir build -C Debug --output-on-failure -R '^AudioForgeAgentControlTests$'` | Exit 0; 1/1 passed, 0.26 s |
| `cmake -S . -B build/f1-signed -G Xcode -DAUDIOFORGE_ENABLE_SIGNED_XPC_TESTS=ON` | Exit 0; signed test registered after wiring |
| `xcodebuild -project build/f1-signed/AudioForgeStudio.xcodeproj -target AudioForgeAgentControlLivePeerTests -configuration Debug -sdk macosx DEVELOPMENT_TEAM="$DEVELOPMENT_TEAM" CODE_SIGNING_ALLOWED=YES CODE_SIGNING_REQUIRED=YES CODE_SIGN_IDENTITY='Apple Development' PROVISIONING_PROFILE_SPECIFIER='' build` | Exit 0; `** BUILD SUCCEEDED **`; team from the verified existing app signature, no credential inspection or provisioning change |
| `ctest --test-dir build/f1-signed -C Debug --output-on-failure -V -R '^AudioForgeAgentControlLivePeerTests$'` | Exit 0; 1/1 CTest entry and 2/2 live scenarios passed (5.414 s for scenarios, 5.50 s total) |
| `git diff --check` / `git diff --cached --check` | Exit 0; included all four initially untracked fixtures via no-index whitespace checks before staging and via the staged check afterward |

- Residual risk: the macOS helper log correlation is helper-wide rather than request-unique and depends on timely log visibility. A bare interruption is not accepted. Full native tests, other macOS versions, non-Xcode packaging, and playback were not run for PR3.
- Rollback boundary: remove the PR3 root CMake hook and four signed test fixture files without changing PR1/PR2 behavior; this tracker-only evidence is separate.

### Authorized publication recovery

- Historical authorization covered the configured GitHub origin and then-current session, push of `feature/agent-control-api`, PR creation to `main`, and merge. This PR3 task is local-only; that prior authorization does not permit a retry.
- [x] PUB.1 — Rebase/isolate only the PD-192/PD-193 work onto `origin/main` (`ad57c1f`); preserve the old branch under local backup `backup/agent-control-api-pre-isolation`. The single CMake conflict was resolved by retaining main's app/test graph and adding only AgentControl targets. A fresh Xcode configure, focused AgentControl build/test (1/1), base CTest (4/4), and `git diff --check` passed. The initial stale generated build failed on missing prior-branch Swift files; reconfiguring `cmake -S . -B build -G Xcode` fixed the stale graph.
- [x] PUB.2 — The historical 925-line aggregate estimate was split into three cohesive direct-to-main slices. Actual authored counts against each predecessor are PR1 397 (`aa4c852`), PR2 299 (`d66b86d` plus tracker-only `04b7d65`, +298/-1), and PR3 275 including its tracker update (+267/-8; source commit `7e308e8` adds 239). The complete current stack is 955 authored lines against clean `origin/main` `ad57c1f`; no size exception is approved.
- [ ] PUB.3 — Publish PR1, wait for required CI/review, merge it, then publish/verify/merge PR2 and PR3 in sequence. The current GitHub session cannot push; exact error is recorded above. Do not retry with another credential/session unless explicitly authorized.
- CMake 3.29 minimum is required for `XCODE_EMBED_XPC_SERVICES`, introduced in CMake 3.29 ([official CMake documentation](https://cmake.org/cmake/help/latest/prop_tgt/XCODE_EMBED_type.html)).

## Relevant files
- `App/AudioForgeApp.swift` — SwiftUI app, media importer, and app-owned controller.
- `App/PlaybackController.swift` — MainActor playback/selection state and security-scoped media lifetime.
- `Bridge/include/AudioForgeBridge.h` — Engine capability/analysis API; not app control.
- `CMakeLists.txt` — native target/test ownership and CMake/CTest wiring.
- `docs/module-boundaries.md` — ownership and build conventions.
- `AgentControl/` — typed v1 discovery, peer identity policy, helper listener, and XPC bundle metadata.
- `Tests/AgentControl/` — focused packaging/wire tests and opt-in signed live peer tests.
- `docs/agent-control-xpc.md` — local signing probe and transport boundaries.
