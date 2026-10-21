import SwiftUI

@main
struct igiRadioApp: App {
    @State private var environment = AppEnvironment()
    @Environment(\.scenePhase) private var scenePhase

    init() {
        IGITheme.applyGlobalAppearance()
    }

    var body: some Scene {
        WindowGroup {
            RootView()
                .environment(environment)
                .onOpenURL { url in
                    consumeShare(IncomingShare.parse(openURL: url))
                }
                .onChange(of: scenePhase) { _, phase in
                    guard phase == .active else {
                        environment.stopAutoReconnect()
                        return
                    }
                    consumeShare(IncomingShare.consumePendingShare())
                    environment.startAutoReconnect()
                }
                .task {
                    #if DEBUG
                    if environment.useMockDevice {
                        try? await environment.digiRadio.connect(host: "mock.local")
                    } else {
                        await environment.autoConnectIfNeeded()
                    }
                    #else
                    await environment.autoConnectIfNeeded()
                    #endif
                    environment.startAutoReconnect()
                }
        }
    }

    private func consumeShare(_ share: IncomingShare?) {
        guard let share else { return }
        Task { await environment.handleIncomingShare(share) }
    }
}
