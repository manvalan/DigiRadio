import Foundation
import OSLog

/// HTTP REST client for DigiRadio firmware API (ch-api.tex, fw 0.9.0).
final class HTTPDigiRadioClient {
    private let session: URLSession
    private let logger = Logger(subsystem: "com.digiradio.igiRadio", category: "HTTP")
    private var baseURL: URL?

    init(session: URLSession? = nil) {
        if let session {
            self.session = session
        } else {
            let config = URLSessionConfiguration.default
            config.timeoutIntervalForRequest = 15
            config.timeoutIntervalForResource = 180
            config.waitsForConnectivity = true
            self.session = URLSession(configuration: config)
        }
    }

    func setHost(_ host: String) throws {
        let normalized = HostNormalizer.httpURL(from: host)
        guard !normalized.isEmpty else { throw DigiRadioError.invalidHost }
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

    // MARK: - Power

    func fetchPower() async throws -> PowerStateResponse {
        try await get("/api/system/power")
    }

    func setPower(_ power: DevicePowerState) async throws -> PowerStateResponse {
        try await post("/api/system/power", body: PowerStateResponse(state: power))
    }

    // MARK: - RDS AF follow

    func fetchAfFollow() async throws -> FirmwareAFState {
        try await get("/api/tuner/af")
    }

    func setAfFollow(enabled: Bool) async throws -> FirmwareAFState {
        try await post("/api/tuner/af", body: AfFollowRequest(enabled: enabled))
    }

    // MARK: - Tuner

    func fetchTunerStatus() async throws -> TunerStatusResponse {
        try await get("/api/tuner/status")
    }

    func fetchDABServices() async throws -> DABServicesResponse {
        try await get("/api/tuner/services")
    }

    func tune(body: TuneRequest, requestTimeout: TimeInterval = 15) async throws -> TunerStatusResponse {
        // Come il pannello web: la risposta POST è già lo stato tuner completo.
        try await post("/api/tuner/tune", body: body, requestTimeout: requestTimeout)
    }

    func playDAB(body: PlayRequest) async throws {
        try await postAck("/api/tuner/play", body: body, requestTimeout: 30)
    }

    func seekFM(direction: String) async throws -> SeekResponse {
        try await post("/api/tuner/seek", body: SeekRequest(direction: direction))
    }

    func scanStation(body: TunerScanRequest, requestTimeout: TimeInterval = 15) async throws -> TunerScanResponse {
        try await post("/api/tuner/scan", body: body, requestTimeout: requestTimeout)
    }

    func scanDAB(maxSteps: Int = 38, name: String? = nil) async throws -> TunerScanResponse {
        try await scanStation(
            body: TunerScanRequest(band: "dab", maxSteps: maxSteps, name: name),
            requestTimeout: 90
        )
    }

    func scanFullFM() async throws -> FMScanFullResponse {
        try await post("/api/tuner/scan/full", body: EmptyBody(), requestTimeout: 120)
    }

    func calibrateAntenna(antcap: Int) async throws -> AntennaCalibrateResponse {
        try await post(
            "/api/tuner/calibrate-antenna",
            body: AntennaCalibrateRequest(antcap: antcap),
            requestTimeout: 30
        )
    }

    // MARK: - Audio

    func fetchAudioProfile() async throws -> AudioProfileDTO {
        try await get("/api/audio/profile")
    }

    func fetchAudioLevels() async throws -> AudioLevelsDTO {
        try await get("/api/audio/levels")
    }

    func putAudioProfile(_ profile: AudioProfileDTO, requestTimeout: TimeInterval = 45) async throws {
        let _: StatusResponse = try await put(
            "/api/audio/profile",
            body: profile,
            requestTimeout: requestTimeout
        )
    }

    func resetAudio() async throws {
        let _: StatusResponse = try await post("/api/audio/reset", body: EmptyBody(), requestTimeout: 45)
    }

    func setEnhance(path: String, level: Int) async throws {
        let _: StatusResponse = try await post(
            path,
            body: EnhanceRequest(level: level),
            requestTimeout: 30
        )
    }

    // MARK: - Diagnostics / configuration

    func setAudioBeep(enabled: Bool) async throws {
        let _: StatusResponse = try await post(
            "/api/audio/beep",
            body: BeepRequest(enabled: enabled),
            requestTimeout: 15
        )
    }

    func fetchDspParams() async throws -> DspParamsResponse {
        try await get("/api/dsp/params")
    }

    func putDspParam(name: String, value: Double) async throws {
        let _: StatusResponse = try await put(
            "/api/dsp/param",
            body: DspParamPutRequest(name: name, value: value),
            requestTimeout: 30
        )
    }

    func setBluetoothAutoReconnect(times: Int) async throws {
        let _: StatusResponse = try await post(
            "/api/bluetooth/auto-reconnect",
            body: AutoReconnectRequest(times: times),
            requestTimeout: 15
        )
    }

    func fetchA2DPCodec() async throws -> A2DPCodecResponse {
        try await get("/api/bluetooth/a2dp-codec")
    }

    func setA2DPCodec(mask: Int) async throws -> A2DPCodecSetResponse {
        try await post(
            "/api/bluetooth/a2dp-codec",
            body: A2DPCodecSetRequest(codecMask: mask),
            requestTimeout: 15
        )
    }

    // MARK: - Bluetooth

    func fetchBluetoothStatus() async throws -> BluetoothStatusResponse {
        try await get("/api/bluetooth/status")
    }

    func fetchSavedSpeaker() async throws -> SavedSpeakerResponse {
        try await get("/api/bluetooth/speaker")
    }

    func fetchBluetoothPaired() async throws -> BluetoothPairedResponse {
        try await get("/api/bluetooth/paired")
    }

    func startBluetoothPairing() async throws {
        let _: StatusResponse = try await post("/api/bluetooth/pair", body: EmptyBody())
    }

    func stopBluetoothPairing() async throws {
        let _: StatusResponse = try await post("/api/bluetooth/pair/stop", body: EmptyBody())
    }

    func disconnectBluetooth() async throws {
        let _: StatusResponse = try await post("/api/bluetooth/disconnect", body: EmptyBody())
    }

    func saveBluetoothSpeaker(mac: String, name: String?) async throws {
        let _: StatusResponse = try await post(
            "/api/bluetooth/speaker",
            body: SaveSpeakerRequest(mac: mac, name: name)
        )
    }

    func clearBluetoothSpeaker() async throws {
        let _: StatusResponse = try await delete("/api/bluetooth/speaker")
    }

    func scanBluetooth(seconds: Int) async throws -> BluetoothScanResponse {
        let timeout = TimeInterval(max(seconds + 20, 30))
        return try await post(
            "/api/bluetooth/scan",
            body: ScanRequest(seconds: seconds),
            requestTimeout: timeout
        )
    }

    func connectBluetooth(mac: String, name: String?, save: Bool) async throws {
        let _: StatusResponse = try await post(
            "/api/bluetooth/connect",
            body: ConnectRequest(mac: mac, name: name, save: save),
            requestTimeout: 90
        )
    }

    func reconnectBluetooth() async throws {
        let _: StatusResponse = try await post(
            "/api/bluetooth/reconnect",
            body: EmptyBody(),
            requestTimeout: 90
        )
    }

    /// POST /api/bluetooth/enable — power-cycle hardware del BT1035.
    func enableBluetooth() async throws {
        let _: StatusResponse = try await post(
            "/api/bluetooth/enable",
            body: EmptyBody(),
            requestTimeout: 60
        )
    }

    /// POST /api/bluetooth/disable — spegne il modulo BT1035.
    func disableBluetooth() async throws {
        let _: StatusResponse = try await post(
            "/api/bluetooth/disable",
            body: EmptyBody(),
            requestTimeout: 30
        )
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

    func setStreaming(_ config: StreamingState) async throws -> StreamingState {
        try await post("/api/streaming", body: config)
    }

    // MARK: - Wi-Fi

    /// POST /api/wifi/reset — cancella le credenziali STA e riavvia in SoftAP.
    func resetWifi() async throws -> WiFiResetResponse {
        try await post("/api/wifi/reset", body: EmptyBody())
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

    private func post<T: Decodable, B: Encodable>(
        _ path: String,
        body: B,
        requestTimeout: TimeInterval = 15
    ) async throws -> T {
        let url = try url(for: path)
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.timeoutInterval = requestTimeout
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = try JSONEncoder.api.encode(body)
        return try await perform(request)
    }

    /// POST senza decodifica del body — usato quando la risposta può essere JSON minimale.
    private func postAck<B: Encodable>(
        _ path: String,
        body: B,
        requestTimeout: TimeInterval = 15
    ) async throws {
        let url = try url(for: path)
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.timeoutInterval = requestTimeout
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = try JSONEncoder.api.encode(body)
        logger.debug("HTTP POST \(url.absoluteString)")
        do {
            let (data, response) = try await session.data(for: request)
            guard let http = response as? HTTPURLResponse else {
                throw DigiRadioError.decodingFailed
            }
            guard (200 ... 299).contains(http.statusCode) else {
                let reason = String(data: data, encoding: .utf8)
                throw DigiRadioError.httpStatus(http.statusCode, reason)
            }
        } catch let error as DigiRadioError {
            throw error
        } catch {
            throw DigiRadioError.network(error)
        }
    }

    private func put<T: Decodable, B: Encodable>(
        _ path: String,
        body: B,
        requestTimeout: TimeInterval = 15
    ) async throws -> T {
        let url = try url(for: path)
        var request = URLRequest(url: url)
        request.httpMethod = "PUT"
        request.timeoutInterval = requestTimeout
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = try JSONEncoder.api.encode(body)
        return try await perform(request)
    }

    private func delete<T: Decodable>(_ path: String, requestTimeout: TimeInterval = 15) async throws -> T {
        let url = try url(for: path)
        var request = URLRequest(url: url)
        request.httpMethod = "DELETE"
        request.timeoutInterval = requestTimeout
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

/// Body and response of /api/system/power: {"state":"on"|"standby"}.
struct AfFollowRequest: Codable {
    let enabled: Bool
}

struct PowerStateResponse: Codable {
    let state: DevicePowerState
}

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
    var antcap: Int?

    enum CodingKeys: String, CodingKey {
        case band
        case freqIndex = "freq_index"
        case frequencyKhz = "frequency_khz"
        case antcap
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

struct BeepRequest: Codable {
    var enabled: Bool
}

struct AutoReconnectRequest: Codable {
    var times: Int
}

struct A2DPCodecResponse: Codable {
    var codec: String
}

struct A2DPCodecSetRequest: Codable {
    var codecMask: Int

    enum CodingKeys: String, CodingKey {
        case codecMask = "codec_mask"
    }
}

struct A2DPCodecSetResponse: Codable {
    var status: String
    var reason: String?
}

struct DspParamEntry: Codable, Identifiable {
    var id: String { name }
    var name: String
    var address: Int
}

struct DspParamsResponse: Codable {
    var params: [DspParamEntry]
}

struct AntennaCalibrateRequest: Codable {
    var antcap: Int
}

struct AntennaCalibrateResponse: Codable {
    var status: String
    var antcap: Int?
}

struct DspParamPutRequest: Codable {
    var name: String
    var value: Double
}

struct SaveSpeakerRequest: Codable {
    var mac: String
    var name: String?
}

struct BluetoothPairedResponse: Codable {
    var devices: [BluetoothPairedDeviceDTO]
}

struct BluetoothPairedDeviceDTO: Codable {
    var index: Int
    var mac: String
    var name: String
}

struct StatusResponse: Codable {
    var status: String
}

struct WiFiResetResponse: Codable {
    var status: String
    var rebootInSec: Int?
    var reason: String?

    enum CodingKeys: String, CodingKey {
        case status, reason
        case rebootInSec = "reboot_in_sec"
    }
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
