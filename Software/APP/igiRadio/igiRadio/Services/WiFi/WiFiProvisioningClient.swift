import Foundation
import OSLog

/// HTTP client for DigiRadio setup APIs (`POST /api/wifi/scan`, `POST /api/wifi`).
final class WiFiProvisioningClient {
    struct ScannedNetwork: Identifiable, Equatable, Hashable {
        var id: String { ssid }
        var ssid: String
        var rssiDbm: Int
        var auth: String
        var channel: Int?
    }

    private let session: URLSession
    private let logger = Logger(subsystem: "com.digiradio.igiRadio", category: "WiFiSetup")
    private var baseURL: URL?

    init(session: URLSession? = nil) {
        if let session {
            self.session = session
        } else {
            let config = URLSessionConfiguration.default
            config.timeoutIntervalForRequest = 20
            config.timeoutIntervalForResource = 30
            config.waitsForConnectivity = false
            self.session = URLSession(configuration: config)
        }
    }

    func setHost(_ host: String) throws {
        let normalized = HostNormalizer.httpURL(from: host)
        guard !normalized.isEmpty, let url = URL(string: normalized) else {
            throw DigiRadioError.invalidHost
        }
        baseURL = url
    }

    func checkHealth() async throws -> HealthResponse {
        try await get("/api/health")
    }

    func scanNetworks() async throws -> [ScannedNetwork] {
        let response: WiFiScanResponse = try await postEmpty("/api/wifi/scan")
        if response.status == "error" {
            throw DigiRadioError.httpStatus(500, response.reason)
        }
        return (response.networks ?? []).map {
            ScannedNetwork(
                ssid: $0.ssid,
                rssiDbm: $0.rssiDbm ?? -100,
                auth: $0.auth ?? "unknown",
                channel: $0.channel
            )
        }
        .sorted { $0.rssiDbm > $1.rssiDbm }
    }

    func provision(ssid: String, password: String) async throws -> Int {
        let response: WiFiProvisionResponse = try await post(
            "/api/wifi",
            body: WiFiProvisionRequest(ssid: ssid, password: password)
        )
        if response.status == "error" {
            throw DigiRadioError.httpStatus(400, response.reason)
        }
        return response.rebootInSec ?? 3
    }

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

    private func postEmpty<T: Decodable>(_ path: String) async throws -> T {
        let url = try url(for: path)
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = Data("{}".utf8)
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

    private func perform<T: Decodable>(_ request: URLRequest) async throws -> T {
        logger.debug("WiFi setup \(request.httpMethod ?? "?") \(request.url?.absoluteString ?? "")")
        do {
            let (data, response) = try await session.data(for: request)
            guard let http = response as? HTTPURLResponse else {
                throw DigiRadioError.decodingFailed
            }
            guard (200 ... 299).contains(http.statusCode) else {
                let reason = String(data: data, encoding: .utf8)
                throw DigiRadioError.httpStatus(http.statusCode, reason)
            }
            return try JSONDecoder.api.decode(T.self, from: data)
        } catch let error as DigiRadioError {
            throw error
        } catch {
            throw DigiRadioError.network(error)
        }
    }
}

private struct WiFiScanResponse: Codable {
    var networks: [WiFiScannedNetworkDTO]?
    var status: String?
    var reason: String?
}

private struct WiFiScannedNetworkDTO: Codable {
    var ssid: String
    var rssiDbm: Int?
    var auth: String?
    var channel: Int?

    enum CodingKeys: String, CodingKey {
        case ssid, auth, channel
        case rssiDbm = "rssi_dbm"
    }
}

private struct WiFiProvisionRequest: Codable {
    var ssid: String
    var password: String
}

private struct WiFiProvisionResponse: Codable {
    var status: String
    var rebootInSec: Int?
    var reason: String?

    enum CodingKeys: String, CodingKey {
        case status, reason
        case rebootInSec = "reboot_in_sec"
    }
}
