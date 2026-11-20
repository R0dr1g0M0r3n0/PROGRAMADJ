# Local agent-control discovery

Cross Music Editor embeds `com.audioforge.studio.agent-control.xpc` under
`Contents/XPCServices`. The app connects by that fixed name using its bundle's
private XPC service namespace; launchd starts the helper on demand. There is no
network listener, global Mach service, agent adapter, or playback routing in
this work unit. MCP over stdio remains the external agent boundary.

The helper is an application-scoped XPC service, not a process managed by the
app. `launchd` owns its on-demand start and idle/crash lifecycle; the app does
not track a helper PID or expose start/stop controls. A client request creates
one connection and sends one discovery message. If that request times out or
the connection becomes unavailable, the caller receives the existing typed
status and the connection is cancelled; there is no automatic retry. A later
API call executes discovery afresh, creates a new XPC connection, and lets
`launchd` activate the service as needed. Peer signing requirements are
revalidated for each new connection.

## Local build and verification

Use CMake 3.29 or later to generate the Xcode project (see
[build graph](module-boundaries.md)). The regular unsigned build can check
packaging and protocol validation but cannot perform authenticated discovery.

For a local signed Debug build, supply an existing Apple Development identity
through command-line Xcode settings for the app and helper. Keep automatic
provisioning off; no team identifier belongs in the repository. Verify the
app with `codesign --verify --deep --strict` and check the signing identifier
and TeamIdentifier of both bundles. Run the Debug-only probe with the app
executable and `--agent-control-discovery-probe`. Status 0 means the app
received an authenticated v1 reply. The flag is not an external agent API.

### Signed live peer tests

Run the opt-in Debug suite in a separate Xcode build directory. Set
`DEVELOPMENT_TEAM` to the team of an existing local Apple Development
identity before running these commands. Do not enable automatic provisioning.

```sh
cmake -S . -B build/f1-signed -G Xcode -DAUDIOFORGE_ENABLE_SIGNED_XPC_TESTS=ON
xcodebuild -project build/f1-signed/AudioForgeStudio.xcodeproj \
  -target AudioForgeAgentControlLivePeerTests -configuration Debug -sdk macosx \
  DEVELOPMENT_TEAM="${DEVELOPMENT_TEAM:?Set the existing local signing team}" \
  CODE_SIGNING_ALLOWED=YES CODE_SIGNING_REQUIRED=YES \
  CODE_SIGN_IDENTITY='Apple Development' PROVISIONING_PROFILE_SPECIFIER='' build
ctest --test-dir build/f1-signed -C Debug --output-on-failure -V \
  -R '^AudioForgeAgentControlLivePeerTests$'
```

The fixtures keep the normal bundle lookup names but use different signing
identifiers under the same team. The client first reaches the fixture helper
with its test identity pinned, then rejects it with the production requirement
(status 4). The real helper answers the matching client but drops the mismatched
client's request; on the tested macOS host that client sees an XPC interruption,
not a v1 discovery response. The live test also requires a newly emitted helper
unified-log record stating that the message was forbidden by the code-signing
requirement. An interruption without that fresh denial is a failure, not proof
of peer authentication. This correlation depends on macOS unified logging
retaining and exposing the helper record. The default unsigned CTest suite
cannot substitute for this check.

## Trust and protocol

- Each process checks its own bundle and dynamic code identity against its
  expected identifier, an Apple trust anchor, and the team from its own
  signature. On each connection it requires the other fixed signing identifier
  under that same team and Apple trust anchor **before** enabling XPC delivery.
  No peer-supplied team, PID lookup, ad-hoc fallback, or trust on first use.
- The v1 request carries exactly two unsigned fields: `version` (1) and
  `operation` (1, discovery). Replies carry `version` and a typed `status`;
  success also carries an empty `capabilities` array. No media, paths, or
  unavailable product capabilities cross this boundary.
- The helper handles at most one request per connection. The app-side
  discovery call waits at most two seconds for a reply, cancels the connection,
  and does not retry. Connection interruption or other XPC transport errors map
  to `Unavailable`; a failed peer signing requirement maps to `Unauthorized`,
  and expiration of the two-second reply deadline maps to `TimedOut`. A later
  call is a new attempt, not an automatic retry. Call from a control worker,
  never the UI or audio thread. Persistent lifecycle and live application state
  are later work.

Apple documents that `launchd` starts XPC services on demand, shuts them down
when idle, and restarts them after a crash; the app therefore delegates helper
process lifecycle to the system rather than supervising it itself. See
[XPC overview](https://developer.apple.com/documentation/xpc) and
[Creating XPC services](https://developer.apple.com/documentation/xpc/creating-xpc-services).

Apple references: [Creating XPC services](https://developer.apple.com/documentation/xpc/creating-xpc-services),
[XPC peer requirements](https://developer.apple.com/documentation/xpc/xpc_connection_set_peer_code_signing_requirement),
[Code signing requirements](https://developer.apple.com/documentation/technotes/tn3127-inside-code-signing-requirements).
