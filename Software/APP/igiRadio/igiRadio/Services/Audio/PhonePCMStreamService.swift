import Foundation
import Network
import OSLog

/// Streams PCM to DigiRadio via documented PUT /api/stream/phone (chunked HTTP/1.1).
actor PhonePCMStreamService {
    static let shared = PhonePCMStreamService()

    private(set) var isStreaming = false
    private(set) var statusMessage = ""
    private var streamTask: Task<Void, Never>?
    private let logger = Logger(subsystem: "com.digiradio.igiRadio", category: "PhoneStream")

    func start(fileURL: URL, connectionHost: String) {
        stop()
        let host = Self.parseHost(connectionHost)
        guard !host.isEmpty else {
            statusMessage = "Host non valido"
            return
        }

        isStreaming = true
        statusMessage = "Avvio stream…"
        streamTask = Task {
            do {
                try await stream(fileURL: fileURL, host: host)
                statusMessage = "Stream completato"
                isStreaming = false
            } catch is CancellationError {
                statusMessage = "Stream interrotto"
                isStreaming = false
            } catch {
                statusMessage = error.localizedDescription
                isStreaming = false
                logger.error("Phone stream failed: \(error.localizedDescription, privacy: .public)")
            }
        }
    }

    func stop() {
        streamTask?.cancel()
        streamTask = nil
        isStreaming = false
    }

    private func stream(fileURL: URL, host: String) async throws {
        let chunks = LocalAudioDecoder.pcmChunks(from: fileURL)
        try await sendChunkedPUT(host: host, chunks: chunks)
    }

    private func sendChunkedPUT(host: String, chunks: AsyncThrowingStream<Data, Error>) async throws {
        let connection = NWConnection(host: NWEndpoint.Host(host), port: 80, using: .tcp)
        connection.start(queue: .global(qos: .userInitiated))
        try await waitUntilReady(connection)

        let header = """
        PUT /api/stream/phone HTTP/1.1\r
        Host: \(host)\r
        Transfer-Encoding: chunked\r
        Connection: close\r
        Content-Type: application/octet-stream\r
        \r
        """
        try await send(connection, Data(header.utf8))

        for try await chunk in chunks {
            try Task.checkCancellation()
            try await sendChunk(connection, chunk)
            statusMessage = "Streaming…"
        }

        try await send(connection, Data("0\r\n\r\n".utf8))
        _ = try await readResponse(connection)
        connection.cancel()
    }

    private func waitUntilReady(_ connection: NWConnection) async throws {
        try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Void, Error>) in
            var finished = false
            connection.stateUpdateHandler = { state in
                guard !finished else { return }
                switch state {
                case .ready:
                    finished = true
                    continuation.resume()
                case let .failed(error):
                    finished = true
                    continuation.resume(throwing: PhoneStreamError.transport(error))
                default:
                    break
                }
            }
        }
    }

    private func send(_ connection: NWConnection, _ data: Data) async throws {
        try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Void, Error>) in
            connection.send(content: data, completion: .contentProcessed { error in
                if let error {
                    continuation.resume(throwing: PhoneStreamError.transport(error))
                } else {
                    continuation.resume()
                }
            })
        }
    }

    private func sendChunk(_ connection: NWConnection, _ chunk: Data) async throws {
        var payload = Data()
        payload.append(contentsOf: "\(String(chunk.count, radix: 16, uppercase: false))\r\n".utf8)
        payload.append(chunk)
        payload.append(contentsOf: "\r\n".utf8)
        try await send(connection, payload)
    }

    private func readResponse(_ connection: NWConnection) async throws -> Int {
        try await withCheckedThrowingContinuation { continuation in
            connection.receive(minimumIncompleteLength: 1, maximumLength: 8192) { data, _, _, error in
                if let error {
                    continuation.resume(throwing: PhoneStreamError.transport(error))
                    return
                }
                guard let data, let text = String(data: data, encoding: .utf8) else {
                    continuation.resume(returning: 200)
                    return
                }
                let statusLine = text.split(separator: "\r\n").first.map(String.init) ?? ""
                if statusLine.contains("409") {
                    continuation.resume(throwing: PhoneStreamError.deviceRejected(409, "Stream occupato (web radio attiva?)"))
                } else if statusLine.contains("503") {
                    continuation.resume(throwing: PhoneStreamError.deviceRejected(503, "Sink I2S non disponibile"))
                } else if statusLine.contains("200") {
                    continuation.resume(returning: 200)
                } else if let code = Int(statusLine.split(separator: " ").dropFirst().first ?? "") {
                    continuation.resume(throwing: PhoneStreamError.deviceRejected(code, statusLine))
                } else {
                    continuation.resume(returning: 200)
                }
            }
        }
    }

    private static func parseHost(_ raw: String) -> String {
        var h = raw.trimmingCharacters(in: .whitespacesAndNewlines)
        h = h.replacingOccurrences(of: "http://", with: "")
        h = h.replacingOccurrences(of: "https://", with: "")
        if let slash = h.firstIndex(of: "/") {
            h = String(h[..<slash])
        }
        return h
    }
}
