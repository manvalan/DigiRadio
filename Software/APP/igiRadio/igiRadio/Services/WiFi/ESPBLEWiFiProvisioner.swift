import ESPProvision
import Foundation

/// BLE Wi‑Fi provisioning (Espressif ESPProvision, Security0 — allineato al firmware).
@MainActor
final class ESPBLEWiFiProvisioner: NSObject, ESPDeviceConnectionDelegate {
    struct DiscoveredDevice: Identifiable, Equatable {
        var id: String { name }
        var name: String
    }

    typealias ScannedNetwork = WiFiProvisioningClient.ScannedNetwork

    private var discoveredDevices: [ESPDevice] = []
    private var connectedDevice: ESPDevice?

    func searchDevices() async throws -> [DiscoveredDevice] {
        let current = try await searchDevices(prefix: DeviceIdentity.blePrefix)
        if !current.isEmpty { return current }
        return try await searchDevices(prefix: DeviceIdentity.legacyBlePrefix)
    }

    private func searchDevices(prefix: String) async throws -> [DiscoveredDevice] {
        try await self.withTimeout(seconds: 12) {
            try await self.withOnceContinuation { continuation in
                ESPProvisionManager.shared.searchESPDevices(
                    devicePrefix: prefix,
                    transport: .ble,
                    security: .unsecure
                ) { devices, error in
                    if let error {
                        continuation.fail(Self.bleError("Ricerca Bluetooth fallita", error))
                        return
                    }
                    let list = devices ?? []
                    self.discoveredDevices = list
                    continuation.succeed(list.map { DiscoveredDevice(name: $0.name) })
                }
            }
        }
    }

    func stopSearch() {
        ESPProvisionManager.shared.stopESPDevicesSearch()
    }

    func connect(deviceName: String) async throws {
        let device: ESPDevice
        if let cached = discoveredDevices.first(where: { $0.name == deviceName }) {
            device = cached
        } else {
            device = try await resolveDevice(named: deviceName)
        }
        try await self.withTimeout(seconds: 20) {
            try await self.withOnceContinuation { continuation in
                device.connect(delegate: self) { status in
                    switch status {
                    case .connected:
                        continuation.succeed(())
                    case let .failedToConnect(error):
                        continuation.fail(Self.bleError(
                            "Impossibile collegarsi al DigiRadio. Verifica che sia in setup mode (dopo reset).",
                            error
                        ))
                    case .disconnected:
                        continuation.fail(DigiRadioError.network(
                            NSError(domain: "igiRadio.ble", code: -1, userInfo: [
                                NSLocalizedDescriptionKey: "Connessione Bluetooth interrotta",
                            ])
                        ))
                    }
                }
            }
        }
        connectedDevice = device
    }

    private func resolveDevice(named deviceName: String) async throws -> ESPDevice {
        try await withOnceContinuation { continuation in
            ESPProvisionManager.shared.createESPDevice(
                deviceName: deviceName,
                transport: .ble,
                security: .unsecure
            ) { device, error in
                if let error {
                    continuation.fail(Self.bleError("DigiRadio non trovato", error))
                } else if let device {
                    continuation.succeed(device)
                } else {
                    continuation.fail(DigiRadioError.notConnected)
                }
            }
        }
    }

    func scanNetworks() async throws -> [ScannedNetwork] {
        guard let device = connectedDevice else { throw DigiRadioError.notConnected }
        return try await self.withTimeout(seconds: 30) {
            try await self.withOnceContinuation { continuation in
                device.scanWifiList { networks, error in
                    if let error {
                        continuation.fail(Self.bleError("Scansione Wi‑Fi fallita", error))
                        return
                    }
                    let mapped = (networks ?? []).map { network in
                        ScannedNetwork(
                            ssid: network.ssid,
                            rssiDbm: Int(network.rssi),
                            auth: Self.authLabel(for: network),
                            channel: network.channel > 0 ? Int(network.channel) : nil
                        )
                    }
                    .sorted { $0.rssiDbm > $1.rssiDbm }
                    continuation.succeed(mapped)
                }
            }
        }
    }

    func provision(ssid: String, password: String) async throws {
        guard let device = connectedDevice else { throw DigiRadioError.notConnected }
        // ESPProvision può invocare il completion due volte (.configApplied poi .success).
        // Il DigiRadio si riavvia subito: se non risponde entro 20 s, consideriamo inviato.
        try await self.withProvisionTimeout(seconds: 20) {
            try await self.withOnceContinuation { continuation in
                device.provision(ssid: ssid, passPhrase: password) { status in
                    switch status {
                    case .configApplied, .success:
                        continuation.succeed(())
                    case let .failure(error):
                        continuation.fail(Self.bleError("Invio credenziali fallito", error))
                    }
                }
            }
        }
        disconnect()
    }

    func disconnect() {
        connectedDevice?.disconnect()
        connectedDevice = nil
        discoveredDevices = []
        stopSearch()
    }

    // MARK: - ESPDeviceConnectionDelegate

    func getProofOfPossesion(forDevice: ESPDevice, completionHandler: @escaping (String) -> Void) {
        completionHandler("")
    }

    func getUsername(forDevice: ESPDevice, completionHandler: @escaping (String?) -> Void) {
        completionHandler(nil)
    }

    // MARK: - Async helpers

    private func withTimeout<T>(
        seconds: TimeInterval,
        operation: @escaping () async throws -> T
    ) async throws -> T {
        try await withThrowingTaskGroup(of: T.self) { group in
            group.addTask { try await operation() }
            group.addTask {
                try await Task.sleep(for: .seconds(seconds))
                throw DigiRadioError.network(
                    NSError(domain: "igiRadio.ble", code: -4, userInfo: [
                        NSLocalizedDescriptionKey: "Operazione scaduta — riprova.",
                    ])
                )
            }
            defer { group.cancelAll() }
            guard let result = try await group.next() else {
                throw DigiRadioError.notConnected
            }
            return result
        }
    }

    /// Timeout sul provisioning = credenziali probabilmente inviate, device in reboot.
    private func withProvisionTimeout(
        seconds: TimeInterval,
        operation: @escaping () async throws -> Void
    ) async throws {
        try await withThrowingTaskGroup(of: Bool.self) { group in
            group.addTask {
                try await operation()
                return true
            }
            group.addTask {
                try await Task.sleep(for: .seconds(seconds))
                return false
            }
            defer { group.cancelAll() }
            guard let finishedNormally = try await group.next() else { return }
            if finishedNormally { return }
            // Timeout: tipico quando il DigiRadio reboota prima della risposta Wi‑Fi connected.
        }
    }

    private final class OnceContinuation<T> {
        private var continuation: CheckedContinuation<T, Error>?
        private var finished = false

        init(_ continuation: CheckedContinuation<T, Error>) {
            self.continuation = continuation
        }

        func succeed(_ value: T) {
            guard !finished, let continuation else { return }
            finished = true
            self.continuation = nil
            continuation.resume(returning: value)
        }

        func fail(_ error: Error) {
            guard !finished, let continuation else { return }
            finished = true
            self.continuation = nil
            continuation.resume(throwing: error)
        }
    }

    private func withOnceContinuation<T>(
        _ body: @escaping (OnceContinuation<T>) -> Void
    ) async throws -> T {
        try await withCheckedThrowingContinuation { continuation in
            body(OnceContinuation(continuation))
        }
    }

    // MARK: - Helpers

    private static func authLabel(for network: ESPWifiNetwork) -> String {
        let raw = String(describing: network.auth).lowercased()
        if raw.contains("open") { return "open" }
        if raw.contains("wpa3") { return "wpa3" }
        if raw.contains("wpa2") { return "wpa2" }
        if raw.contains("wpa") { return "wpa" }
        if raw.contains("wep") { return "wep" }
        return raw.isEmpty ? "unknown" : raw
    }

    private static func bleError(_ prefix: String, _ error: Any) -> DigiRadioError {
        .network(NSError(domain: "igiRadio.ble", code: -3, userInfo: [
            NSLocalizedDescriptionKey: "\(prefix): \(error)",
        ]))
    }
}
