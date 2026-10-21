import Foundation
import Observation

@Observable
final class ConnectionViewModel {
    private let environment: AppEnvironment
    var hostInput: String
    var isBusy = false
    var errorMessage: String?
    var wifiResetMessage: String?
    let discovery = DigiRadioDiscoveryService()

    init(environment: AppEnvironment) {
        self.environment = environment
        if environment.useMockDevice {
            hostInput = "mock.local"
        } else if let saved = ConnectionPreferences.lastHost {
            hostInput = HostNormalizer.displayHost(from: saved)
        } else {
            hostInput = ""
        }
    }

    var state: DigiRadioState { environment.state }
    var recentHosts: [String] { ConnectionPreferences.recentHosts }

    func connect() async {
        isBusy = true
        errorMessage = nil
        defer { isBusy = false }

        let primary = HostNormalizer.httpURL(from: hostInput)
        guard !primary.isEmpty else {
            errorMessage = String(localized: "Inserisci l'host dell'igiRadio (es. igiradio-cc4db4.local o 192.168.1.56).")
            return
        }

        var seen = Set<String>()
        var candidates = DeviceIdentity.connectionCandidates(from: primary)
        if primary.contains(".local") {
            candidates.append(contentsOf: DeviceIdentity.connectionCandidates(
                from: primary.replacingOccurrences(of: ".local", with: "")
            ))
        }
        for host in ConnectionPreferences.recentHosts {
            candidates.append(contentsOf: DeviceIdentity.connectionCandidates(from: host))
        }
        candidates = candidates.filter { seen.insert($0).inserted }

        var lastError: Error?
        for (index, host) in candidates.prefix(8).enumerated() {
            do {
                try await environment.digiRadio.connect(host: host)
                hostInput = HostNormalizer.displayHost(from: host)
                errorMessage = nil
                return
            } catch {
                lastError = error
                if index < min(2, candidates.count - 1) {
                    try? await Task.sleep(for: .milliseconds(600))
                }
            }
        }

        errorMessage = friendlyMessage(for: lastError)
    }

    func connect(to host: String) async {
        hostInput = HostNormalizer.displayHost(from: host)
        await connect()
    }

    func disconnect() {
        environment.digiRadio.disconnect()
        wifiResetMessage = nil
    }

    func verifyReachability() async {
        _ = await environment.digiRadio.verifyReachability()
    }

    func resetWifi() async {
        isBusy = true
        errorMessage = nil
        wifiResetMessage = nil
        defer { isBusy = false }
        do {
            let reboot = try await environment.digiRadio.resetWifi()
            hostInput = "192.168.4.1"
            wifiResetMessage = """
            Credenziali Wi‑Fi cancellate. La scheda si riavvia (~\(reboot) s) \
            in modalità setup: rete igiRadio-seriale, IP 192.168.4.1, \
            oppure hostname.local.
            """
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }

    func toggleMock(_ enabled: Bool) {
        environment.setUseMock(enabled)
        if enabled {
            hostInput = "mock.local"
            discovery.stop()
            Task { try? await environment.digiRadio.connect(host: hostInput) }
        } else {
            environment.digiRadio.disconnect()
            hostInput = ConnectionPreferences.lastHost.map { HostNormalizer.displayHost(from: $0) } ?? ""
            startDiscovery()
        }
    }

    func startDiscovery() {
        discovery.start()
        if !hostInput.isEmpty {
            discovery.addManualHost(hostInput)
        }
        for host in ConnectionPreferences.recentHosts {
            discovery.addManualHost(host)
        }
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

    private func friendlyMessage(for error: Error?) -> String {
        guard let error else {
            return String(localized: "Connessione non riuscita. Verifica Wi‑Fi, rete locale e host.")
        }
        let text = error.localizedDescription.lowercased()
        if text.contains("local network") || text.contains("rete locale") {
            return String(localized: "Autorizza l'accesso alla rete locale in Impostazioni → igiRadio.")
        }
        if text.contains("timed out") || text.contains("timeout") {
            return String(localized: "Timeout: il DigiRadio non risponde. Stessa rete Wi‑Fi? Prova l'IP LAN (es. 192.168.1.x) invece di .local.")
        }
        if text.contains("could not connect") || text.contains("network") {
            return String(localized: "Rete non raggiungibile. Controlla che iPhone e DigiRadio siano sulla stessa Wi‑Fi.")
        }
        return error.localizedDescription
    }
}
