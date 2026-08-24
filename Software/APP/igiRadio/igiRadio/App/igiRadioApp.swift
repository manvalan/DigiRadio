import SwiftUI

@main
struct igiRadioApp: App {
    @State private var environment = AppEnvironment()

    var body: some Scene {
        WindowGroup {
            RootView()
                .environment(environment)
                .task {
                    #if DEBUG
                    if environment.useMockDevice, !environment.state.connection.isConnected {
                        try? await environment.digiRadio.connect(host: "mock.local")
                    }
                    #endif
                }
        }
    }
}
