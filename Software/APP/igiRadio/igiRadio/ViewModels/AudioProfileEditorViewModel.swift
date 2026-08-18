import Foundation
import Observation

@Observable
final class AudioProfileEditorViewModel {
    var profile: AudioProfileTemplate
    private let service: any DigiRadioService
    private let onSave: (AudioProfileTemplate) -> Void
    private var applyTask: Task<Void, Never>?

    var isSaving = false
    var isNewProfile: Bool

    init(
        profile: AudioProfileTemplate,
        service: any DigiRadioService,
        isNewProfile: Bool,
        onSave: @escaping (AudioProfileTemplate) -> Void
    ) {
        self.profile = profile
        self.service = service
        self.isNewProfile = isNewProfile
        self.onSave = onSave
    }

    func schedulePreviewApply() {
        guard !profile.isBuiltIn else { return }
        applyTask?.cancel()
        applyTask = Task {
            try? await Task.sleep(for: .milliseconds(400))
            guard !Task.isCancelled else { return }
            await applyToDevice(persistUserCopy: false)
        }
    }

    func applyToDevice(persistUserCopy: Bool) async {
        isSaving = true
        defer { isSaving = false }
        let audio = service.state.audio
        let dto = profile.profileDTO(mixer: audio.mixer, master: audio.master)
        try? await service.applyAudioProfile(dto)
        if persistUserCopy, !profile.isBuiltIn {
            onSave(profile)
        }
    }

    func saveUserProfile() {
        guard !profile.isBuiltIn else { return }
        onSave(profile)
    }
}
