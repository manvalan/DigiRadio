import Foundation
import Observation

@Observable
final class DiagnosticsViewModel {
    private let service: any DigiRadioService

    var isBusy = false
    var statusMessage: String?
    var errorMessage: String?
    var dspParamCount: Int?
    var autoReconnectTimes = 3

    init(service: any DigiRadioService) {
        self.service = service
        autoReconnectTimes = service.state.bluetooth.autoReconnect
    }

    func refreshSnapshot() async {
        guard service.state.connection.isConnected else {
            errorMessage = String(localized: "Non connesso a DigiRadio.")
            return
        }
        isBusy = true
        errorMessage = nil
        defer { isBusy = false }
        do {
            try await service.refreshHealth()
            try await service.refreshTunerStatus()
            try await service.refreshBluetoothStatus()
            autoReconnectTimes = service.state.bluetooth.autoReconnect
            statusMessage = String(localized: "Snapshot aggiornato.")
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }

    func testBeep(enabled: Bool) async {
        guard service.state.connection.isConnected else {
            errorMessage = String(localized: "Non connesso a DigiRadio.")
            return
        }
        isBusy = true
        errorMessage = nil
        defer { isBusy = false }
        do {
            try await service.playAudioBeep(enabled: enabled)
            statusMessage = enabled ? "Beep attivo sul DSP." : "Beep disattivato."
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }

    func loadDspParams() async {
        guard service.state.connection.isConnected else {
            errorMessage = String(localized: "Non connesso a DigiRadio.")
            return
        }
        isBusy = true
        errorMessage = nil
        defer { isBusy = false }
        do {
            let response = try await service.fetchDspParams()
            dspParamCount = response.params.count
            statusMessage = String(localized: "Trovati \(response.params.count) parametri DSP.")
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }

    func saveAutoReconnect() async {
        guard service.state.connection.isConnected else {
            errorMessage = String(localized: "Non connesso a DigiRadio.")
            return
        }
        isBusy = true
        errorMessage = nil
        defer { isBusy = false }
        do {
            try await service.setBluetoothAutoReconnect(times: autoReconnectTimes)
            statusMessage = String(localized: "Auto-riconnessione BT impostata a \(autoReconnectTimes).")
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }
}
