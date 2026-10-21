import Foundation
import Observation

@Observable
@MainActor
final class WiFiSetupViewModel {
    enum Step: Equatable {
        case findDevice
        case configureWifi
        case done(rebootSeconds: Int, expectedHost: String?)
    }

    var step: Step = .findDevice

    var isSearchingBLE = false
    var isConnecting = false
    var isScanningWifi = false
    var isProvisioning = false

    var bleDevices: [ESPBLEWiFiProvisioner.DiscoveredDevice] = []
    var selectedDeviceName: String?

    var networks: [WiFiProvisioningClient.ScannedNetwork] = []
    var selectedSSID: String?
    var password = ""

    var statusMessage: String?
    var errorMessage: String?

    private let bleProvisioner = ESPBLEWiFiProvisioner()

    func resetMessages() {
        errorMessage = nil
        statusMessage = nil
    }

    func onAppear() {
        guard step == .findDevice, bleDevices.isEmpty, !isSearchingBLE else { return }
        searchBLEDevices()
    }

    func cleanup() {
        isSearchingBLE = false
        isConnecting = false
        isScanningWifi = false
        isProvisioning = false
        bleProvisioner.stopSearch()
        bleProvisioner.disconnect()
    }

    // MARK: - BLE

    func searchBLEDevices() {
        resetMessages()
        isSearchingBLE = true
        bleDevices = []
        Task {
            defer { isSearchingBLE = false }
            do {
                bleDevices = try await bleProvisioner.searchDevices()
                if bleDevices.isEmpty {
                    statusMessage = "Nessun igiRadio trovato. Dopo un reset il device è in setup mode — riprova tra qualche secondo."
                } else if bleDevices.count == 1, let only = bleDevices.first?.name {
                    statusMessage = "igiRadio trovato — connessione in corso…"
                    await connect(deviceName: only)
                } else {
                    statusMessage = "Tocca il tuo igiRadio."
                    if selectedDeviceName == nil {
                        selectedDeviceName = bleDevices.first?.name
                    }
                }
            } catch {
                errorMessage = friendlyError(error)
            }
        }
    }

    func selectAndConnect(deviceName: String) {
        selectedDeviceName = deviceName
        Task { await connect(deviceName: deviceName) }
    }

    private func connect(deviceName: String) async {
        resetMessages()
        isConnecting = true
        defer { isConnecting = false }
        do {
            try await bleProvisioner.connect(deviceName: deviceName)
            step = .configureWifi
            statusMessage = "Connesso. Scansione reti Wi‑Fi…"
            await scanWifiNetworks(auto: true)
        } catch {
            errorMessage = friendlyError(error)
        }
    }

    // MARK: - Wi‑Fi

    func scanWifiNetworks() {
        Task { await scanWifiNetworks(auto: false) }
    }

    private func scanWifiNetworks(auto: Bool) async {
        if !auto { resetMessages() }
        isScanningWifi = true
        if !auto {
            networks = []
            selectedSSID = nil
            password = ""
        }
        defer { isScanningWifi = false }
        do {
            networks = try await bleProvisioner.scanNetworks()
            if networks.isEmpty {
                statusMessage = "Nessuna rete trovata — riprova."
            } else {
                statusMessage = "Tocca la tua rete Wi‑Fi di casa."
            }
        } catch {
            errorMessage = friendlyError(error)
        }
    }

    func selectNetwork(_ network: WiFiProvisioningClient.ScannedNetwork) {
        selectedSSID = network.ssid
        if network.auth == "open" {
            password = ""
        }
        resetMessages()
    }

    func provisionSelectedNetwork() {
        guard let ssid = selectedSSID, !ssid.isEmpty else {
            errorMessage = "Seleziona una rete Wi‑Fi."
            return
        }
        let needsPassword = networks.first(where: { $0.ssid == ssid })?.auth != "open"
        if needsPassword, password.isEmpty {
            errorMessage = "Inserisci la password della rete."
            return
        }
        resetMessages()
        isProvisioning = true
        Task {
            defer { isProvisioning = false }
            do {
                try await bleProvisioner.provision(ssid: ssid, password: password)
                let expectedHost = selectedDeviceName.flatMap {
                    DigiRadioDiscoveryService.httpHost(fromBLEName: $0)
                }
                step = .done(rebootSeconds: 3, expectedHost: expectedHost)
                statusMessage = "Fatto! Il DigiRadio si riavvia e si collega alla tua rete."
            } catch {
                errorMessage = friendlyError(error)
            }
        }
    }

    private func friendlyError(_ error: Error) -> String {
        if case let DigiRadioError.httpStatus(code, reason) = error {
            return "Errore \(code): \(reason ?? "connessione fallita")"
        }
        if case let DigiRadioError.network(underlying) = error {
            return underlying.localizedDescription
        }
        return error.localizedDescription
    }
}
