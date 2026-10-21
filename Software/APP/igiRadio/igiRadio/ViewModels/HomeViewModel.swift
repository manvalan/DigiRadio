import Foundation
import Observation

@Observable
final class HomeViewModel {
    private let service: any DigiRadioService
    private var refreshTask: Task<Void, Never>?

    init(service: any DigiRadioService) {
        self.service = service
    }

    var state: DigiRadioState { service.state }

    func onAppear() {
        refreshTask?.cancel()
        refreshTask = Task {
            await refreshAll()
            while !Task.isCancelled {
                try? await Task.sleep(for: .seconds(2))
                guard state.connection.isConnected else { continue }
                guard !service.isBackgroundRefreshPaused else { continue }
                try? await service.refreshTunerStatus()
            }
        }
    }

    func onDisappear() {
        refreshTask?.cancel()
        refreshTask = nil
    }

    func refreshAll() async {
        guard state.connection.isConnected else { return }
        try? await service.refreshHealth()
        try? await service.refreshPower()
        try? await service.refreshTunerStatus()
        try? await service.refreshAudioProfile()
        try? await service.refreshStations()
    }

    func togglePower() async {
        let next: DevicePowerState = state.power == .on ? .standby : .on
        try? await service.setPower(next)
    }

    func setVolume(_ value: Double) async {
        try? await service.setVolume(Int(value))
    }

    func tuneFM(frequencyMHz: Double) async {
        let clamped = min(108.0, max(87.5, frequencyMHz))
        let khz = Int((clamped * 1000).rounded())
        try? await service.tuneFM(frequencyKhz: khz)
        try? await service.refreshTunerStatus()
    }

    func seekFM(_ direction: String) async {
        try? await service.seekFM(direction: direction)
    }

    func selectFM() async {
        try? await service.endPhoneAudio()
        if state.tuner.band == .fm { return }
        let mhz: Double
        if let khz = state.tuner.fm?.frequencyKhz {
            mhz = Double(khz) / 1000
        } else {
            mhz = 100
        }
        await tuneFM(frequencyMHz: mhz)
    }

    func selectDAB() async {
        try? await service.applyAudioSourceRouting(forStreaming: false)
        await service.prepareForDABOperations()
        if isDABServicePlaying() { return }

        for station in dabPlaybackCandidates() {
            await playDAB(station)
            if isDABServicePlaying() { return }
        }

        await playFirstService(onEnsemble: state.tuner.dab?.freqIndex ?? 12)
    }

    func selectStream() async {
        try? await service.applyAudioSourceRouting(forStreaming: true)
    }

    private func isDABServicePlaying() -> Bool {
        state.tuner.band == .dab && state.tuner.dab?.playingServiceId != nil
    }

    private func dabPlaybackCandidates() -> [ScannedStation] {
        var seen = Set<String>()
        var list: [ScannedStation] = []
        let sources: [[ScannedStation]] = [
            FavoriteStationsStore.load().filter { $0.band == .dab },
            ScannedStationStore.load().dab,
            state.stations.filter { $0.band == .dab }.map(ScannedStation.from),
        ]
        for station in sources.flatMap({ $0 }) {
            guard seen.insert(station.id).inserted else { continue }
            list.append(station)
        }
        return list
    }

    private func playDAB(_ station: ScannedStation) async {
        guard let ensemble = station.dabFreqIndex,
              let sid = station.dabServiceId,
              let cid = station.dabComponentId else { return }
        try? await service.playScannedDAB(ensemble: ensemble, serviceId: sid, componentId: cid)
        try? await service.refreshTunerStatus()
    }

    private func playFirstService(onEnsemble ensemble: Int) async {
        do {
            try await service.tuneDAB(freqIndex: ensemble)
            let services = try await service.listDABServices()
            guard let first = services.first else { return }
            try await service.playDAB(
                serviceId: first.serviceId,
                componentId: first.componentId
            )
        } catch {
            return
        }
    }
}
