import AVFoundation
import CoreMedia
import Foundation
import ReplayKit

enum AppAudioCaptureError: LocalizedError {
    case unavailable
    case startFailed(String)

    var errorDescription: String? {
        switch self {
        case .unavailable:
            return "Questo iPad/iPhone non può catturare l’audio delle altre app."
        case let .startFailed(message):
            return message
        }
    }
}

/// Cattura l’audio delle altre app (ReplayKit, senza microfono) e lo converte
/// in PCM s16le stereo 48 kHz per PUT /api/stream/phone.
final class AppAudioCapture: @unchecked Sendable {
    static let shared = AppAudioCapture()

    private let recorder = RPScreenRecorder.shared()
    private let lock = NSLock()
    private var converter: AVAudioConverter?
    private var inputFormat: AVAudioFormat?
    private var continuation: AsyncThrowingStream<Data, Error>.Continuation?

    private let outputFormat = AVAudioFormat(
        commonFormat: .pcmFormatInt16,
        sampleRate: LocalAudioDecoder.outputSampleRate,
        channels: 2,
        interleaved: true
    )!

    var isAvailable: Bool { recorder.isAvailable }

    func start() -> AsyncThrowingStream<Data, Error> {
        stop()
        activateSession()
        return AsyncThrowingStream { continuation in
            self.continuation = continuation
            continuation.onTermination = { [weak self] _ in
                self?.stopRecorder()
            }

            guard self.recorder.isAvailable else {
                continuation.finish(throwing: AppAudioCaptureError.unavailable)
                return
            }

            self.recorder.isMicrophoneEnabled = false
            self.recorder.startCapture { [weak self] sampleBuffer, type, error in
                guard let self else { return }
                if let error {
                    continuation.finish(throwing: error)
                    return
                }
                guard type == .audioApp else { return }
                if let data = self.convert(sampleBuffer), !data.isEmpty {
                    continuation.yield(data)
                }
            } completionHandler: { error in
                if let error {
                    continuation.finish(
                        throwing: AppAudioCaptureError.startFailed(error.localizedDescription)
                    )
                }
            }
        }
    }

    func stop() {
        continuation?.finish()
        continuation = nil
        stopRecorder()
        lock.lock()
        converter = nil
        inputFormat = nil
        lock.unlock()
    }

    private func activateSession() {
        let session = AVAudioSession.sharedInstance()
        try? session.setCategory(.playback, mode: .default, options: [.mixWithOthers])
        try? session.setActive(true)
    }

    private func stopRecorder() {
        guard recorder.isRecording else { return }
        recorder.stopCapture { _ in }
    }

    private func convert(_ sampleBuffer: CMSampleBuffer) -> Data? {
        guard let formatDescription = CMSampleBufferGetFormatDescription(sampleBuffer) else { return nil }
        let inFormat = AVAudioFormat(cmAudioFormatDescription: formatDescription)
        let frames = AVAudioFrameCount(CMSampleBufferGetNumSamples(sampleBuffer))
        guard frames > 0,
              let inputBuffer = AVAudioPCMBuffer(pcmFormat: inFormat, frameCapacity: frames)
        else { return nil }
        inputBuffer.frameLength = frames
        guard fill(inputBuffer, from: sampleBuffer) else { return nil }

        lock.lock()
        defer { lock.unlock() }
        if inputFormat?.sampleRate != inFormat.sampleRate
            || inputFormat?.channelCount != inFormat.channelCount
            || inputFormat?.commonFormat != inFormat.commonFormat {
            inputFormat = inFormat
            converter = AVAudioConverter(from: inFormat, to: outputFormat)
        }
        guard let converter else { return nil }

        let ratio = outputFormat.sampleRate / inFormat.sampleRate
        let outFrames = AVAudioFrameCount(Double(frames) * ratio) + 64
        guard let outputBuffer = AVAudioPCMBuffer(pcmFormat: outputFormat, frameCapacity: outFrames)
        else { return nil }

        var consumed = false
        var convertError: NSError?
        let status = converter.convert(to: outputBuffer, error: &convertError) { _, outStatus in
            if consumed {
                outStatus.pointee = .noDataNow
                return nil
            }
            consumed = true
            outStatus.pointee = .haveData
            return inputBuffer
        }
        guard status != .error, outputBuffer.frameLength > 0,
              let samples = outputBuffer.int16ChannelData
        else { return nil }

        let byteCount = Int(outputBuffer.frameLength)
            * Int(outputFormat.streamDescription.pointee.mBytesPerFrame)
        return Data(bytes: samples[0], count: byteCount)
    }

    private func fill(_ buffer: AVAudioPCMBuffer, from sampleBuffer: CMSampleBuffer) -> Bool {
        var needed = 0
        CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
            sampleBuffer,
            bufferListSizeNeededOut: &needed,
            bufferListOut: nil,
            bufferListSize: 0,
            blockBufferAllocator: nil,
            blockBufferMemoryAllocator: nil,
            flags: kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment,
            blockBufferOut: nil
        )
        let channelCount = max(Int(buffer.format.channelCount), 1)
        let ablSize = max(
            needed,
            MemoryLayout<AudioBufferList>.size + MemoryLayout<AudioBuffer>.size * max(channelCount - 1, 0)
        )
        let raw = UnsafeMutableRawPointer.allocate(byteCount: ablSize, alignment: MemoryLayout<AudioBufferList>.alignment)
        defer { raw.deallocate() }
        raw.initializeMemory(as: UInt8.self, repeating: 0, count: ablSize)
        let audioBufferList = raw.bindMemory(to: AudioBufferList.self, capacity: 1)

        var blockBuffer: CMBlockBuffer?
        let status = CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
            sampleBuffer,
            bufferListSizeNeededOut: nil,
            bufferListOut: audioBufferList,
            bufferListSize: ablSize,
            blockBufferAllocator: kCFAllocatorDefault,
            blockBufferMemoryAllocator: kCFAllocatorDefault,
            flags: kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment,
            blockBufferOut: &blockBuffer
        )
        guard status == noErr else { return false }
        _ = blockBuffer

        let dest = UnsafeMutableAudioBufferListPointer(buffer.mutableAudioBufferList)
        let src = UnsafeMutableAudioBufferListPointer(audioBufferList)
        guard !src.isEmpty, !dest.isEmpty else { return false }
        for (index, source) in src.enumerated() where index < dest.count {
            guard let srcBytes = source.mData, let dstBytes = dest[index].mData else { continue }
            let count = min(Int(source.mDataByteSize), Int(dest[index].mDataByteSize))
            memcpy(dstBytes, srcBytes, count)
        }
        return true
    }
}
