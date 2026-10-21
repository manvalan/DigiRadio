import Foundation

enum DigiRadioError: LocalizedError, Equatable {
    case notConnected
    case invalidHost
    case httpStatus(Int, String?)
    case decodingFailed
    case network(Error)

    var errorDescription: String? {
        switch self {
        case .notConnected: return "Non connesso a DigiRadio."
        case .invalidHost: return "Host non valido."
        case let .httpStatus(code, reason): return "HTTP \(code): \(reason ?? "errore")"
        case .decodingFailed: return "Risposta non valida dal dispositivo."
        case let .network(error): return error.localizedDescription
        }
    }

    static func == (lhs: DigiRadioError, rhs: DigiRadioError) -> Bool {
        switch (lhs, rhs) {
        case (.notConnected, .notConnected), (.invalidHost, .invalidHost), (.decodingFailed, .decodingFailed):
            return true
        case let (.httpStatus(a, b), .httpStatus(c, d)):
            return a == c && b == d
        default:
            return false
        }
    }
}

/// High-level API for controlling DigiRadio (HTTP REST per firmware docs).
protocol DigiRadioService: AnyObject {
    var state: DigiRadioState { get }

    /// True while a long tuner scan holds the HTTP connection budget.
    var isBackgroundRefreshPaused: Bool { get }
    func pauseBackgroundRefresh()
    func resumeBackgroundRefresh()

    func connect(host: String) async throws
    func disconnect()
    func refreshHealth() async throws
    /// Probe live di `/api/health`. Se fallisce, marca non connesso e azzera i dati scheda.
    @discardableResult
    func verifyReachability() async -> Bool
    /// POST /api/wifi/reset. Restituisce i secondi al reboot (default 3).
    func resetWifi() async throws -> Int
    func refreshTunerStatus() async throws
    func refreshAudioProfile() async throws
    func refreshBluetoothStatus() async throws
    func refreshStations() async throws
    /// Sostituisce le stazioni di una banda mantenendo l'altra (max 20 totali sul device).
    func replaceStations(for band: TunerBand, with stations: [Station]) async throws
    /// Sostituisce tutti i preset hardware con l'elenco dato (max 20).
    func replaceAllStations(_ stations: [Station]) async throws
    func refreshStreaming() async throws
    func setStreaming(enabled: Bool, url: String) async throws
    func refreshPower() async throws
    func setPower(_ power: DevicePowerState) async throws
    /// Reads GET /api/tuner/af; leaves `state.firmwareAF` nil on older firmware.
    func refreshFirmwareAF() async
    func setFirmwareAF(enabled: Bool) async throws
    func applyAudioSourceRouting(forStreaming: Bool) async throws
    /// Mux ADAU su Stream, ferma la webradio, volume udibile, flag phone audio.
    func prepareForPhoneAudio() async throws
    func endPhoneAudio() async throws
    /// Disabilita streaming se attivo — best-effort, non blocca il DAB se fallisce.
    func prepareForDABOperations() async

    func tuneFM(frequencyKhz: Int, antcap: Int?) async throws
    func tuneDAB(freqIndex: Int) async throws
    func playDAB(serviceId: Int, componentId: Int) async throws
    /// Sintonizza l'ensemble e avvia un servizio DAB (flusso web: tune → verifica → play).
    func playScannedDAB(ensemble: Int, serviceId: Int, componentId: Int) async throws
    func seekFM(direction: String) async throws
    func scanFM(maxSteps: Int, name: String?) async throws -> TunerScanResponse
    func scanDAB(maxSteps: Int, name: String?) async throws -> TunerScanResponse
    func scanFullFM(progress: (@Sendable (Int, Int) -> Void)?) async throws -> [FMScanHit]
    func calibrateAntenna(antcap: Int) async throws
    func setVolume(_ volume: Int) async throws
    func listDABServices() async throws -> [DABService]

    func saveStation(_ station: Station) async throws
    func updateStation(at index: Int, name: String) async throws
    func removeStation(at index: Int) async throws
    func reorderStation(from: Int, to: Int) async throws
    func tuneStation(at index: Int) async throws

    func applyAudioProfile(_ profile: AudioProfileDTO) async throws
    /// Applies EQ + enhancements only, preserving live source/master from the device.
    func applyAudioPreset(_ preset: AudioProfileTemplate) async throws
    func setStereoEnhance(level: Int) async throws
    func setBassEnhance(level: Int) async throws
    func resetAudio() async throws
    func fetchAudioLevels() async throws -> AudioLevelsDTO

    func scanBluetooth(seconds: Int) async throws
    func setBluetoothEnabled(_ enabled: Bool) async throws
    func connectBluetooth(mac: String, name: String?, save: Bool) async throws
    func reconnectBluetooth() async throws
    func startBluetoothPairing() async throws
    func stopBluetoothPairing() async throws
    func disconnectBluetoothA2DP() async throws
    func clearBluetoothSpeaker() async throws
    func fetchA2DPCodec() async throws
    func setA2DPCodec(mask: Int) async throws -> String?

    func playAudioBeep(enabled: Bool) async throws
    func fetchDspParams() async throws -> DspParamsResponse
    func setDspParam(name: String, value: Double) async throws
    func setBluetoothAutoReconnect(times: Int) async throws
}

/// Wire DTO for PUT /api/audio/profile
struct AudioProfileDTO: Codable, Equatable, Sendable {
    var activeSource: AudioActiveSource
    var master: MasterVolumeState
    var eq: [EQBandState]
    var enhancements: EnhancementsState

    enum CodingKeys: String, CodingKey {
        case activeSource = "active_source"
        case master, eq, enhancements
    }
}

/// GET /api/audio/levels — dBFS live dal DSP (0 = fondo scala).
struct AudioLevelsDTO: Codable, Equatable, Sendable {
    var radioInLeftDb: Double
    var radioInRightDb: Double
    var bluetoothInLeftDb: Double
    var bluetoothInRightDb: Double
    var outputLeftDb: Double
    var outputRightDb: Double

    enum CodingKeys: String, CodingKey {
        case radioInLeftDb = "radio_in_left_db"
        case radioInRightDb = "radio_in_right_db"
        case bluetoothInLeftDb = "bluetooth_in_left_db"
        case bluetoothInRightDb = "bluetooth_in_right_db"
        case outputLeftDb = "output_left_db"
        case outputRightDb = "output_right_db"
    }

    /// VU di ascolto: −20 dBFS = vuoto, −6 dBFS ≈ metà, 0 dBFS ≈ 3/4
    /// (la zona alta è lo headroom di clip). Non mappare 0 dBFS sul 100%:
    /// il compressore radio sta tipicamente a −2…0 e la barra sembrerebbe inchiodata.
    static func meterFill(dbfs: Double, floorDb: Double = -20) -> Double {
        let unityFill = 0.72
        let clamped = min(0, max(floorDb, dbfs))
        return unityFill * (clamped - floorDb) / -floorDb
    }
}

extension DigiRadioService {
    func tuneFM(frequencyKhz: Int) async throws {
        try await tuneFM(frequencyKhz: frequencyKhz, antcap: nil)
    }

    func scanFullFM() async throws -> [FMScanHit] {
        try await scanFullFM(progress: nil)
    }
}
