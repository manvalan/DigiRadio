import Foundation
import Observation

@Observable
@MainActor
final class BluetoothViewModel {
    var isScanning = false
    var isConnecting = false
    var isReconnecting = false
    var isBusy = false
    var isSavingCodec = false
    var statusMessage: String?
    var errorMessage: String?
    var manualMac = ""
    var manualName = ""
    var enabledCodecOptions: Set<A2DPCodecMask> = A2DPCodecMask.defaultEnabled

    private let service: any DigiRadioService

    init(service: any DigiRadioService) {
        self.service = service
    }

    var bluetooth: BluetoothState {
        service.state.bluetooth
    }

    func refresh() async {
        guard service.state.connection.isConnected else {
            return
        }
        do {
            try await service.refreshBluetoothStatus()
            try await service.fetchA2DPCodec()
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }

    func saveCodecPreferences() async {
        resetMessages()
        isSavingCodec = true
        defer { isSavingCodec = false }
        do {
            let mask = A2DPCodecMask.mask(from: enabledCodecOptions)
            if let reason = try await service.setA2DPCodec(mask: mask) {
                errorMessage = reason
                return
            }
            let isConnected = service.state.bluetooth.a2dpState == .connected
                || service.state.bluetooth.a2dpState == .streaming
            statusMessage = isConnected
                ? "Codec salvati. Disconnetti e ricollega l’altoparlante per applicarli."
                : "Codec salvati — validi alla prossima connessione."
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }

    func toggleCodecOption(_ option: A2DPCodecMask) {
        if enabledCodecOptions.contains(option) {
            enabledCodecOptions.remove(option)
        } else {
            enabledCodecOptions.insert(option)
        }
    }

    func scan() async {
        resetMessages()
        isScanning = true
        defer { isScanning = false }
        do {
            statusMessage = "Ricerca altoparlanti (~45 s)… tieni il Bose in pairing. I soundbar spesso non compaiono: usa il MAC."
            try await service.scanBluetooth(seconds: 45)
            let count = service.state.bluetooth.nearbyDevices.count
            statusMessage = count == 0
                ? "Nessun altoparlante in lista — collega col MAC (tipico dei Bose)."
                : "Trovati \(count) — tocca per collegare."
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }

    func connect(device: BluetoothDevice) async {
        resetMessages()
        isConnecting = true
        defer { isConnecting = false }
        do {
            statusMessage = "Connessione a \(device.name)… può richiedere fino a un minuto."
            try await service.connectBluetooth(mac: device.mac, name: device.name, save: true)
            statusMessage = "Altoparlante connesso."
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }

    func reconnect() async {
        resetMessages()
        isReconnecting = true
        defer { isReconnecting = false }
        do {
            statusMessage = "Riconnessione all’altoparlante salvato…"
            try await service.reconnectBluetooth()
            statusMessage = "Riconnessione completata."
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }

    func connectManual() async {
        resetMessages()
        guard let mac = BtMacNormalizer.forApi(manualMac) else {
            errorMessage = "MAC non valido. Servono 12 cifre esadecimali (es. 000C8A78AB62)."
            return
        }
        isConnecting = true
        defer { isConnecting = false }
        do {
            let name = manualName.trimmingCharacters(in: .whitespacesAndNewlines)
            statusMessage = "Connessione all’altoparlante… tieni il Bose in pairing."
            try await service.connectBluetooth(
                mac: mac,
                name: name.isEmpty ? "Speaker" : name,
                save: true
            )
            statusMessage = "Altoparlante connesso."
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }

    func setModuleEnabled(_ enabled: Bool) async {
        resetMessages()
        isBusy = true
        defer { isBusy = false }
        statusMessage = enabled ? "Accensione modulo audio…" : "Spegnimento modulo audio…"
        do {
            try await service.setBluetoothEnabled(enabled)
            statusMessage = enabled
                ? "Modulo audio (BT1035) acceso."
                : "Modulo audio spento."
        } catch {
            statusMessage = nil
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }

    func disconnect() async {
        await runAction("Sessione A2DP disconnessa.") {
            try await service.disconnectBluetoothA2DP()
        }
    }

    func clearSavedSpeaker() async {
        await runAction("Altoparlante rimosso.") {
            try await service.clearBluetoothSpeaker()
        }
    }

    private func runAction(_ success: String, _ action: () async throws -> Void) async {
        resetMessages()
        isBusy = true
        defer { isBusy = false }
        do {
            try await action()
            statusMessage = success
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }

    private func resetMessages() {
        errorMessage = nil
        statusMessage = nil
    }
}
