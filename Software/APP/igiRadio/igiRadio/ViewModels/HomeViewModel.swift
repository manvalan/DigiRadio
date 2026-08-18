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
                try? await Task.sleep(for: .seconds(3))
                guard state.connection.isConnected else { continue }
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
        try? await service.refreshTunerStatus()
        try? await service.refreshAudioProfile()
        try? await service.refreshBluetoothStatus()
        try? await service.refreshStations()
    }

    func setVolume(_ value: Double) async {
        try? await service.setVolume(Int(value))
    }

    func seekFM(_ direction: String) async {
        try? await service.seekFM(direction: direction)
    }

    func tunePreset(at index: Int) async {
        try? await service.tuneStation(at: index)
    }
}
