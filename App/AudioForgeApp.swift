import SwiftUI

@main
struct AudioForgeApp: App {
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
