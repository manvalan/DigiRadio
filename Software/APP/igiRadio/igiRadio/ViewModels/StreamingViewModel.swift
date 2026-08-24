import Foundation
import Observation
import UniformTypeIdentifiers

enum StreamSource: String, CaseIterable, Identifiable {
    case webRadio = "Radio web"
    case urlContent = "URL contenuto"
    case localFile = "File iPhone"

    var id: String { rawValue }

    var subtitle: String {
        switch self {
        case .webRadio: "MP3 HTTP sul DigiRadio (decoder ESP32)"
        case .urlContent: "Podcast, playlist o stream personalizzato"
        case .localFile: "File audio decodificato e inviato dal telefono"
        }
    }

    var icon: String {
        switch self {
        case .webRadio: "antenna.radiowaves.left.and.right"
        case .urlContent: "link"
        case .localFile: "music.note.list"
        }
    }
}

struct StreamPreset: Identifiable {
    var id: String { name }
    var name: String
    var url: String
}

enum StreamPresets {
    /// Default from firmware WebRadioConfig.hpp (documented example).
    static let catalog: [StreamPreset] = [
        StreamPreset(name: "Radio Monte Carlo", url: "http://edge.radiomontecarlo.net/RMC.mp3"),
        StreamPreset(name: "Esempio MP3", url: "http://stream.example.com/radio.mp3")
    ]
}

@Observable
final class StreamingViewModel {
    private let service: any DigiRadioService

    var source: StreamSource = .webRadio
    var enabled = false
    var url = StreamPresets.catalog[0].url
    var isLoading = false
    var isSaving = false
    var errorMessage: String?

    var selectedFileName = ""
    var selectedFileURL: URL?
    var phoneStreamActive = false
    var phoneStreamStatus = ""

    private var statusPollTask: Task<Void, Never>?

    init(service: any DigiRadioService) {
        self.service = service
    }

    deinit {
        statusPollTask?.cancel()
    }

    var connectionHost: String {
        service.state.connection.host
    }

    var isConnected: Bool {
        service.state.connection.isConnected
    }

    func load() async {
        isLoading = true
        errorMessage = nil
        defer { isLoading = false }
        do {
            try await service.refreshStreaming()
            syncFromDevice()
        } catch {
            errorMessage = error.localizedDescription
        }
        startStatusPolling()
    }

    func selectPreset(_ preset: StreamPreset) {
        url = preset.url
        source = .webRadio
    }

    func normalizedHTTPURL() -> String? {
        var trimmed = url.trimmingCharacters(in: .whitespacesAndNewlines)
        if trimmed.isEmpty { return nil }
        if !trimmed.hasPrefix("http://") {
            if trimmed.hasPrefix("https://") {
                errorMessage = "Il firmware accetta solo URL http:// (non https)."
                return nil
            }
            trimmed = "http://\(trimmed)"
        }
        return trimmed
    }

    func playWebStream() async {
        guard let normalized = normalizedHTTPURL() else { return }
        url = normalized
        await apply(enabled: true)
    }

    func stopWebStream() async {
        await apply(enabled: false)
    }

    func apply(enabled: Bool? = nil) async {
        guard let normalized = normalizedHTTPURL() else { return }
        isSaving = true
        errorMessage = nil
        defer { isSaving = false }
        let targetEnabled = enabled ?? self.enabled
        do {
            try await service.setStreaming(enabled: targetEnabled, url: normalized)
            syncFromDevice()
        } catch {
            errorMessage = error.localizedDescription
        }
    }

    func pickFile(_ url: URL) {
        selectedFileURL = url
        selectedFileName = url.lastPathComponent
        source = .localFile
        errorMessage = nil
    }

    func playLocalFile() async {
        guard isConnected, let fileURL = selectedFileURL else {
            errorMessage = "Seleziona un file e connetti DigiRadio."
            return
        }
        errorMessage = nil
        await stopWebStreamSilently()
        await PhonePCMStreamService.shared.start(fileURL: fileURL, connectionHost: connectionHost)
    }

    func stopLocalFile() async {
        await PhonePCMStreamService.shared.stop()
        phoneStreamActive = false
        phoneStreamStatus = "Fermo"
    }

    func stopPolling() {
        statusPollTask?.cancel()
        statusPollTask = nil
    }

    private func stopWebStreamSilently() async {
        try? await service.setStreaming(enabled: false, url: url)
        syncFromDevice()
    }

    private func syncFromDevice() {
        enabled = service.state.streaming.enabled
        if !service.state.streaming.url.isEmpty {
            url = service.state.streaming.url
        }
    }

    private func startStatusPolling() {
        statusPollTask?.cancel()
        statusPollTask = Task { @MainActor in
            while !Task.isCancelled {
                let streamer = PhonePCMStreamService.shared
                phoneStreamActive = await streamer.isStreaming
                let status = await streamer.statusMessage
                if !status.isEmpty { phoneStreamStatus = status }
                try? await Task.sleep(for: .milliseconds(500))
            }
        }
    }
}
