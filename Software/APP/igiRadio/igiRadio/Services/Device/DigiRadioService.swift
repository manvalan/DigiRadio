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

    func connect(host: String) async throws
    func disconnect()
    func refreshHealth() async throws
    func refreshTunerStatus() async throws
    func refreshAudioProfile() async throws
    func refreshBluetoothStatus() async throws
    func refreshStations() async throws
    func refreshStreaming() async throws

    func tuneFM(frequencyKhz: Int) async throws
    func tuneDAB(freqIndex: Int) async throws
    func playDAB(serviceId: Int, componentId: Int) async throws
    func seekFM(direction: String) async throws
    func scanFM(maxSteps: Int, name: String?) async throws -> TunerScanResponse
    func scanFullFM() async throws -> [FMScanHit]
    func setVolume(_ volume: Int) async throws
    func listDABServices() async throws -> [DABService]

    func saveStation(_ station: Station) async throws
    func removeStation(at index: Int) async throws
    func reorderStation(from: Int, to: Int) async throws
    func tuneStation(at index: Int) async throws

    func applyAudioProfile(_ profile: AudioProfileDTO) async throws
    func setStereoEnhance(level: Int) async throws
    func setBassEnhance(level: Int) async throws
    func resetAudio() async throws

    func scanBluetooth(seconds: Int) async throws
    func connectBluetooth(mac: String, name: String?, save: Bool) async throws
    func reconnectBluetooth() async throws
}

/// Wire DTO for PUT /api/audio/profile
struct AudioProfileDTO: Codable, Equatable, Sendable {
    var mixer: MixerState
    var master: MasterVolumeState
    var eq: [EQBandState]
    var enhancements: EnhancementsState
}
