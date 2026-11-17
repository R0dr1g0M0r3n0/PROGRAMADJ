import SwiftUI

@main
struct AudioForgeApp: App {
    var body: some Scene {
        WindowGroup("Cross Music Editor") {
            VStack(spacing: 12) {
                Text("Cross Music Editor")
                    .font(.title)
                Text("Engine API \(AudioForgeBridge.engineVersion())")
            }
            .padding(24)
        }
    }
}
