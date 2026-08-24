import AVFoundation
import Foundation

/// Decodes local audio files to interleaved PCM s16le stereo @ 48 kHz for PUT /api/stream/phone.
enum LocalAudioDecoder {
    static let outputSampleRate: Double = 48_000
    static let chunkFrames: AVAudioFrameCount = 2048

    static func pcmChunks(from fileURL: URL) -> AsyncThrowingStream<Data, Error> {
        AsyncThrowingStream { continuation in
            Task {
                do {
                    let accessed = fileURL.startAccessingSecurityScopedResource()
                    defer { if accessed { fileURL.stopAccessingSecurityScopedResource() } }

                    let inputFile = try AVAudioFile(forReading: fileURL)
                    guard let outputFormat = AVAudioFormat(
                        commonFormat: .pcmFormatInt16,
                        sampleRate: outputSampleRate,
                        channels: 2,
                        interleaved: true
                    ) else {
                        throw PhoneStreamError.formatUnsupported
                    }

                    guard let converter = AVAudioConverter(from: inputFile.processingFormat, to: outputFormat) else {
                        throw PhoneStreamError.formatUnsupported
                    }

                    let inputBuffer = AVAudioPCMBuffer(
                        pcmFormat: inputFile.processingFormat,
                        frameCapacity: chunkFrames
                    )!
                    let outputBuffer = AVAudioPCMBuffer(
                        pcmFormat: outputFormat,
                        frameCapacity: chunkFrames
                    )!

                    while inputFile.framePosition < inputFile.length {
                        try Task.checkCancellation()
                        try inputFile.read(into: inputBuffer, frameCount: chunkFrames)
                        if inputBuffer.frameLength == 0 { break }

                        var inputProvided = false
                        var convertError: NSError?
                        let status = converter.convert(to: outputBuffer, error: &convertError) { _, outStatus in
                            if inputProvided {
                                outStatus.pointee = .noDataNow
                                return nil
                            }
                            inputProvided = true
                            outStatus.pointee = .haveData
                            return inputBuffer
                        }

                        if status == .error {
                            throw convertError ?? PhoneStreamError.decodeFailed
                        }
                        if outputBuffer.frameLength == 0 { continue }

                        let byteCount = Int(outputBuffer.frameLength) * Int(outputFormat.streamDescription.pointee.mBytesPerFrame)
                        guard let channelData = outputBuffer.int16ChannelData else { continue }
                        continuation.yield(Data(bytes: channelData[0], count: byteCount))
                    }
                    continuation.finish()
                } catch {
                    continuation.finish(throwing: error)
                }
            }
        }
    }
}

enum PhoneStreamError: LocalizedError {
    case notConnected
    case formatUnsupported
    case decodeFailed
    case deviceRejected(Int, String?)
    case transport(Error)

    var errorDescription: String? {
        switch self {
        case .notConnected: return "DigiRadio non connesso."
        case .formatUnsupported: return "Formato audio non supportato."
        case .decodeFailed: return "Impossibile decodificare il file."
        case let .deviceRejected(code, body): return "Device HTTP \(code): \(body ?? "")"
        case let .transport(error): return error.localizedDescription
        }
    }
}
