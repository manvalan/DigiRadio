import Foundation
import Observation
import OSLog

/// Maps BLE setup names (DigiRadio-XXXX) to HTTP hosts and verifies reachability.
@Observable
final class DigiRadioDiscoveryService {
    struct DiscoveredDevice: Identifiable, Equatable {
        var id: String { host }
        var name: String
        var host: String
        var isReachable: Bool
        var firmware: String?
    }

    private(set) var devices: [DiscoveredDevice] = []
    private(set) var isSearching = false

    private let ble = BLEProvisioningService()
    private let logger = Logger(subsystem: "com.digiradio.igiRadio", category: "Discovery")
    private var probeTasks: [Task<Void, Never>] = []

    func start() {
        guard !isSearching else { return }
        isSearching = true
        devices = []
        ble.startScan()
        logger.info("Discovery started")
    }

    func stop() {
        isSearching = false
        ble.stopScan()
        probeTasks.forEach { $0.cancel() }
        probeTasks = []
    }

    func refreshFromBLE() {
        let candidates = ble.discoveredDevices.compactMap { entry -> DiscoveredDevice? in
            guard let host = Self.httpHost(fromBLEName: entry.name) else { return nil }
            return DiscoveredDevice(name: entry.name, host: host, isReachable: false, firmware: nil)
        }

        for candidate in candidates {
            guard !devices.contains(where: { $0.host == candidate.host }) else { continue }
            devices.append(candidate)
            probe(host: candidate.host)
        }
    }

    func addManualHost(_ raw: String) {
        let trimmed = raw.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return }
        var host = trimmed
        if !host.hasPrefix("http://") && !host.hasPrefix("https://") {
            host = "http://\(host)"
        }
        guard let url = URL(string: host), let schemeHost = url.host else { return }
        let normalized = "http://\(schemeHost)"
        guard !devices.contains(where: { $0.host == normalized }) else { return }
        let name = schemeHost
        devices.append(DiscoveredDevice(name: name, host: normalized, isReachable: false, firmware: nil))
        probe(host: normalized)
    }

    static func httpHost(fromBLEName name: String) -> String? {
        // SoftAP / BLE name: DigiRadio-CC4DB4 → digiradio-cc4db4.local
        guard name.hasPrefix("DigiRadio-") else { return nil }
        let suffix = String(name.dropFirst("DigiRadio-".count)).lowercased()
        guard !suffix.isEmpty else { return nil }
        return "http://digiradio-\(suffix).local"
    }

    private func probe(host: String) {
        let task = Task {
            let client = HTTPDigiRadioClient()
            do {
                try client.setHost(host)
                let health = try await client.fetchHealth()
                await MainActor.run {
                    if let index = devices.firstIndex(where: { $0.host == host }) {
                        devices[index].isReachable = health.status == "ok"
                        devices[index].firmware = health.fw
                    }
                }
            } catch {
                await MainActor.run {
                    if let index = devices.firstIndex(where: { $0.host == host }) {
                        devices[index].isReachable = false
                    }
                }
                logger.debug("Probe failed for \(host, privacy: .public): \(error.localizedDescription)")
            }
        }
        probeTasks.append(task)
    }
}
