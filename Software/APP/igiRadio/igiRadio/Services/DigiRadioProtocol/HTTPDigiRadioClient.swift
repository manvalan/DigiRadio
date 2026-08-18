import Foundation
import OSLog

/// HTTP REST client for DigiRadio firmware API (ch-api.tex).
final class HTTPDigiRadioClient {
    private let session: URLSession
    private let logger = Logger(subsystem: "com.digiradio.igiRadio", category: "HTTP")
    private var baseURL: URL?

    init(session: URLSession = .shared) {
        self.session = session
    }

    func setHost(_ host: String) throws {
        let trimmed = host.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { throw DigiRadioError.invalidHost }
        var normalized = trimmed
        if !normalized.hasPrefix("http://") && !normalized.hasPrefix("https://") {
            normalized = "http://\(normalized)"
        }
        guard let url = URL(string: normalized) else { throw DigiRadioError.invalidHost }
        baseURL = url
    }

    func clearHost() {
        baseURL = nil
    }

    var isConfigured: Bool { baseURL != nil }

    // MARK: - Health

    func fetchHealth() async throws -> HealthResponse {
        try await get("/api/health")
    }

    // MARK: - Tuner

    func fetchTunerStatus() async throws -> TunerStatusResponse {
        try await get("/api/tuner/status")
    }

    func fetchDABServices() async throws -> DABServicesResponse {
        try await get("/api/tuner/services")
    }

    func tune(body: TuneRequest) async throws -> TunerStatusResponse {
        try await post("/api/tuner/tune", body: body)
    }

    func playDAB(body: PlayRequest) async throws {
        let _: StatusResponse = try await post("/api/tuner/play", body: body)
    }

    func seekFM(direction: String) async throws -> SeekResponse {
        try await post("/api/tuner/seek", body: SeekRequest(direction: direction))
    }

    func scanStation(body: TunerScanRequest) async throws -> TunerScanResponse {
        try await post("/api/tuner/scan", body: body)
    }

    func scanFullFM() async throws -> FMScanFullResponse {
        try await post("/api/tuner/scan/full", body: EmptyBody())
    }

    // MARK: - Audio

    func fetchAudioProfile() async throws -> AudioProfileDTO {
        try await get("/api/audio/profile")
    }

    func putAudioProfile(_ profile: AudioProfileDTO) async throws {
        let _: StatusResponse = try await put("/api/audio/profile", body: profile)
    }

    func resetAudio() async throws {
        let _: StatusResponse = try await post("/api/audio/reset", body: EmptyBody())
    }

    func setEnhance(path: String, level: Int) async throws {
        let _: StatusResponse = try await post(path, body: EnhanceRequest(level: level))
    }

    // MARK: - Bluetooth

    func fetchBluetoothStatus() async throws -> BluetoothStatusResponse {
        try await get("/api/bluetooth/status")
    }

    func fetchSavedSpeaker() async throws -> SavedSpeakerResponse {
        try await get("/api/bluetooth/speaker")
    }

    func scanBluetooth(seconds: Int) async throws -> BluetoothScanResponse {
        try await post("/api/bluetooth/scan", body: ScanRequest(seconds: seconds))
    }

    func connectBluetooth(mac: String, name: String?, save: Bool) async throws {
        let _: StatusResponse = try await post(
            "/api/bluetooth/connect",
            body: ConnectRequest(mac: mac, name: name, save: save)
        )
    }

    func reconnectBluetooth() async throws {
        let _: StatusResponse = try await post("/api/bluetooth/reconnect", body: EmptyBody())
    }

    // MARK: - Stations

    func fetchStations() async throws -> StationListResponse {
        try await get("/api/stations")
    }

    func addStation(_ station: Station) async throws {
        let _: StatusResponse = try await post("/api/stations", body: station)
    }

    func removeStation(index: Int) async throws {
        let _: StatusResponse = try await post("/api/stations/remove", body: IndexRequest(index: index))
    }

    func reorderStation(from: Int, to: Int) async throws {
        let _: StatusResponse = try await post(
            "/api/stations/reorder",
            body: ReorderRequest(from: from, to: to)
        )
    }

    func tuneStation(index: Int) async throws {
        let _: StatusResponse = try await post("/api/stations/tune", body: IndexRequest(index: index))
    }

    // MARK: - Streaming

    func fetchStreaming() async throws -> StreamingState {
        try await get("/api/streaming")
    }

    // MARK: - HTTP helpers

    private func url(for path: String) throws -> URL {
        guard let base = baseURL else { throw DigiRadioError.notConnected }
        guard let url = URL(string: path, relativeTo: base) else { throw DigiRadioError.invalidHost }
        return url
    }

    private func get<T: Decodable>(_ path: String) async throws -> T {
        let url = try url(for: path)
        var request = URLRequest(url: url)
        request.httpMethod = "GET"
        return try await perform(request)
    }

    private func post<T: Decodable, B: Encodable>(_ path: String, body: B) async throws -> T {
        let url = try url(for: path)
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = try JSONEncoder.api.encode(body)
        return try await perform(request)
    }

    private func put<T: Decodable, B: Encodable>(_ path: String, body: B) async throws -> T {
        let url = try url(for: path)
        var request = URLRequest(url: url)
        request.httpMethod = "PUT"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = try JSONEncoder.api.encode(body)
        return try await perform(request)
    }

    private func perform<T: Decodable>(_ request: URLRequest) async throws -> T {
        logger.debug("HTTP \(request.httpMethod ?? "?") \(request.url?.absoluteString ?? "")")
        do {
            let (data, response) = try await session.data(for: request)
            guard let http = response as? HTTPURLResponse else {
                throw DigiRadioError.decodingFailed
            }
            guard (200 ... 299).contains(http.statusCode) else {
                let reason = String(data: data, encoding: .utf8)
                throw DigiRadioError.httpStatus(http.statusCode, reason)
            }
            do {
                return try JSONDecoder.api.decode(T.self, from: data)
            } catch {
                logger.error("Decode failed: \(error.localizedDescription)")
                throw DigiRadioError.decodingFailed
            }
        } catch let error as DigiRadioError {
            throw error
        } catch {
            throw DigiRadioError.network(error)
        }
    }
}

// MARK: - API DTOs

struct EmptyBody: Codable {}

struct HealthResponse: Codable {
    var status: String
    var fw: String
    var serialNumber: String
    var chips: ChipHealthResponse
}

struct ChipHealthResponse: Codable {
    var si4684: Bool
    var adau1701: Bool
    var bt1035: Bool
}

struct TunerStatusResponse: Codable {
    var booted: Bool
    var band: String
    var locked: Bool
    var volume: Int
    var fm: FMTunerState?
    var dab: DABTunerState?

    enum CodingKeys: String, CodingKey {
        case booted, band, locked, volume, fm, dab
    }
}

extension TunerStatusResponse {
    func dabDecoded() -> DABTunerState? {
        guard let dab else { return nil }
        return DABTunerState(
            freqIndex: dab.freqIndex,
            ficQuality: dab.ficQuality,
            cnrDb: dab.cnrDb,
            playingServiceId: dab.playingServiceId,
            playingComponentId: dab.playingComponentId,
            dynamicLabel: dab.dynamicLabel
        )
    }
}

struct DABServicesResponse: Codable {
    var services: [DABService]
}

struct TuneRequest: Codable {
    var band: String
    var freqIndex: Int?
    var frequencyKhz: Int?

    enum CodingKeys: String, CodingKey {
        case band
        case freqIndex = "freq_index"
        case frequencyKhz = "frequency_khz"
    }
}

struct PlayRequest: Codable {
    var serviceId: Int
    var componentId: Int

    enum CodingKeys: String, CodingKey {
        case serviceId = "service_id"
        case componentId = "component_id"
    }
}

struct SeekRequest: Codable {
    var direction: String
}

struct SeekResponse: Codable {
    var frequencyKhz: Int?

    enum CodingKeys: String, CodingKey {
        case frequencyKhz = "frequency_khz"
    }
}

struct EnhanceRequest: Codable {
    var level: Int
}

struct StatusResponse: Codable {
    var status: String
}

struct BluetoothStatusResponse: Codable {
    var booted: Bool
    var pairing: Bool
    var a2dp: String
    var deviceName: String
    var autoReconnect: Int

    enum CodingKeys: String, CodingKey {
        case booted, pairing, a2dp
        case deviceName = "device_name"
        case autoReconnect = "auto_reconnect"
    }
}

struct SavedSpeakerResponse: Codable {
    var configured: Bool
    var mac: String?
    var name: String?
}

struct ScanRequest: Codable {
    var seconds: Int
}

struct BluetoothScanResponse: Codable {
    var devices: [BluetoothScanDevice]
}

struct BluetoothScanDevice: Codable {
    var index: Int
    var mac: String
    var name: String
    var rssiDbm: Int

    enum CodingKeys: String, CodingKey {
        case index, mac, name
        case rssiDbm = "rssi_dbm"
    }
}

struct ConnectRequest: Codable {
    var mac: String
    var name: String?
    var save: Bool?
}

struct StationListResponse: Codable {
    var stations: [Station]
}

struct IndexRequest: Codable {
    var index: Int
}

struct ReorderRequest: Codable {
    var from: Int
    var to: Int
}

struct TunerScanRequest: Codable {
    var band: String
    var maxSteps: Int?
    var name: String?

    enum CodingKeys: String, CodingKey {
        case band
        case maxSteps = "max_steps"
        case name
    }
}

struct TunerScanResponse: Codable {
    var status: String
    var band: String?
    var steps: Int?
    var frequencyKhz: Int?
    var stationName: String?
    var freqIndex: Int?
    var serviceId: Int?
    var componentId: Int?

    enum CodingKeys: String, CodingKey {
        case status, band, steps
        case frequencyKhz = "frequency_khz"
        case stationName = "station_name"
        case freqIndex = "freq_index"
        case serviceId = "service_id"
        case componentId = "component_id"
    }
}

struct FMScanHit: Codable, Identifiable, Equatable {
    var id: Int { frequencyKhz }
    var frequencyKhz: Int
    var rssiDbuv: Int?
    var snrDb: Int?
    var stationName: String?

    enum CodingKeys: String, CodingKey {
        case frequencyKhz = "frequency_khz"
        case rssiDbuv = "rssi_dbuv"
        case snrDb = "snr_db"
        case stationName = "station_name"
    }
}

struct FMScanFullResponse: Codable {
    var stations: [FMScanHit]
}

extension JSONEncoder {
    static let api: JSONEncoder = {
        let e = JSONEncoder()
        return e
    }()
}

extension JSONDecoder {
    static let api: JSONDecoder = {
        let d = JSONDecoder()
        return d
    }()
}
