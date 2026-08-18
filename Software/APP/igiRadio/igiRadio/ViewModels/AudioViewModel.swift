import Foundation
import Observation

@Observable
final class AudioViewModel {
    private let service: any DigiRadioService
    private var applyTask: Task<Void, Never>?

    var panel: AudioPanel = .mixer
    var mixer = MixerState()
    var master = MasterVolumeState()
    var eqBands: [EQBandState] = []
    var stereoLevel: Double = 0
    var bassLevel: Double = 0
    var isLoading = false
    var isSaving = false

    init(service: any DigiRadioService) {
        self.service = service
    }

    func load() async {
        isLoading = true
        defer { isLoading = false }
        try? await service.refreshAudioProfile()
        syncFromDevice()
    }

    func syncFromDevice() {
        let audio = service.state.audio
        mixer = audio.mixer
        master = audio.master
        eqBands = audio.eq
        stereoLevel = Double(audio.enhancements.stereoLevel)
        bassLevel = Double(audio.enhancements.bassLevel)
        if master.leftDb == 0 && master.rightDb == 0 {
            let vol = Double(service.state.tuner.volume)
            master = MasterVolumeState(leftDb: vol, rightDb: vol)
        }
    }

    func scheduleApply() {
        applyTask?.cancel()
        applyTask = Task {
            try? await Task.sleep(for: .milliseconds(350))
            guard !Task.isCancelled else { return }
            await applyNow()
        }
    }

    func applyNow() async {
        isSaving = true
        defer { isSaving = false }
        let profile = AudioProfileDTO(
            mixer: mixer,
            master: master,
            eq: eqBands,
            enhancements: EnhancementsState(stereoLevel: Int(stereoLevel), bassLevel: Int(bassLevel))
        )
        try? await service.applyAudioProfile(profile)
        try? await service.setVolume(Int((master.leftDb + master.rightDb) / 2))
        syncFromDevice()
    }

    func reset() async {
        try? await service.resetAudio()
        syncFromDevice()
    }

    func applyEnhancements() async {
        try? await service.setStereoEnhance(level: Int(stereoLevel))
        try? await service.setBassEnhance(level: Int(bassLevel))
        syncFromDevice()
    }
}
