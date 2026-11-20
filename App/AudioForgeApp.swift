import SwiftUI

@main
struct AudioForgeApp: App {
#if AF_XPC_DISCOVERY_PROBE
    init() {
        // A local build probe exercises the same signed app-to-helper path without opening a window.
        if CommandLine.arguments.dropFirst().elementsEqual(["--agent-control-discovery-probe"]) {
            let result = AFDiscoverAgentControl()
            print("XPC discovery status: \(result.rawValue)")
            exit(result == .success ? 0 : 1)
        }
    }
#endif

    var body: some Scene {
        WindowGroup("AudioForge Studio") {
            VStack(spacing: 12) {
                Text("AudioForge Studio")
                    .font(.title)
                Text("Engine API \(AudioForgeBridge.engineVersion())")
            }
            .padding(24)
        }
    }
}
