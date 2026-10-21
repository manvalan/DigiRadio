import Foundation
import Observation

@Observable
final class BassBoostViewModel {
    private var service: any DigiRadioService

    var isEnabled = BassBoostPreferences.isEnabled
    var level = Double(BassBoostPreferences.level)
    var isBusy = false
    var errorMessage: String?

    init(service: any DigiRadioService) {
        self.service = service
        syncFromDevice()
    }

    func replaceService(_ service: any DigiRadioService) {
        self.service = service
        syncFromDevice()
    }

    var isConnected: Bool { service.state.connection.isConnected }

    func syncFromDevice() {
        let deviceLevel = service.state.audio.enhancements.bassLevel
        if deviceLevel > 0 {
            isEnabled = true
            level = Double(deviceLevel)
        } else if !isConnected {
            isEnabled = BassBoostPreferences.isEnabled
            level = Double(BassBoostPreferences.level)
        } else {
            isEnabled = false
        }
    }

    func setEnabled(_ enabled: Bool) async {
        guard !isBusy else { return }
        isBusy = true
        errorMessage = nil
        defer { isBusy = false }

        isEnabled = enabled
        if enabled, level < 1 {
            level = Double(BassBoostPreferences.defaultLevel)
        }
        let sendLevel = enabled ? Int(level) : 0
        BassBoostPreferences.save(
            enabled: enabled,
            level: enabled ? Int(level) : BassBoostPreferences.level
        )
        guard isConnected else { return }
        do {
            try await service.setBassEnhance(level: sendLevel)
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
            syncFromDevice()
        }
    }

    func commitLevel() async {
        guard !isBusy else { return }
        BassBoostPreferences.save(enabled: isEnabled, level: Int(level))
        guard isEnabled, isConnected else { return }

        isBusy = true
        errorMessage = nil
        defer { isBusy = false }
        do {
            try await service.setBassEnhance(level: Int(level))
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
            syncFromDevice()
        }
    }
}
