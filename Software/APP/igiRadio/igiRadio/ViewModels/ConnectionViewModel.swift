import Foundation
import Observation

@Observable
final class ConnectionViewModel {
    private let environment: AppEnvironment
    var hostInput = "digiradio-cc4db4.local"
    var isBusy = false
    var errorMessage: String?
    let discovery = DigiRadioDiscoveryService()

    init(environment: AppEnvironment) {
        self.environment = environment
        if environment.useMockDevice {
            hostInput = "mock.local"
        }
    }

    var state: DigiRadioState { environment.state }

    func connect() async {
        isBusy = true
        errorMessage = nil
        defer { isBusy = false }
        do {
            try await environment.digiRadio.connect(host: hostInput)
        } catch {
            errorMessage = error.localizedDescription
        }
    }

    func connect(to host: String) async {
        hostInput = host.replacingOccurrences(of: "http://", with: "").replacingOccurrences(of: "https://", with: "")
        await connect()
    }

    func disconnect() {
        environment.digiRadio.disconnect()
    }

    func toggleMock(_ enabled: Bool) {
        environment.setUseMock(enabled)
        if enabled {
            hostInput = "mock.local"
            discovery.stop()
            Task { try? await environment.digiRadio.connect(host: hostInput) }
        } else {
            environment.digiRadio.disconnect()
        }
    }

    func startDiscovery() {
        discovery.start()
        discovery.addManualHost(hostInput)
    }

    func stopDiscovery() {
        discovery.stop()
    }

    func pollDiscovery() {
        discovery.refreshFromBLE()
        if !hostInput.isEmpty {
            discovery.addManualHost(hostInput)
        }
    }
}
