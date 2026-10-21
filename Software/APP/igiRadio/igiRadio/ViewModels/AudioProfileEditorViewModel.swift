import Foundation
import Observation

@Observable
final class AudioProfileEditorViewModel {
    var profile: AudioProfileTemplate
    private let service: any DigiRadioService
    private let onSave: (AudioProfileTemplate) -> Void
    private var applyTask: Task<Void, Never>?

    var isSaving = false
    var errorMessage: String?
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
        applyTask?.cancel()
        applyTask = Task {
            try? await Task.sleep(for: .milliseconds(600))
            guard !Task.isCancelled else { return }
            await applyToDevice(persistUserCopy: false)
        }
    }

    func applyToDevice(persistUserCopy: Bool) async {
        guard service.state.connection.isConnected else {
            errorMessage = "Non connesso a DigiRadio."
            return
        }
        guard !isSaving else { return }

        isSaving = true
        errorMessage = nil
        defer { isSaving = false }
        do {
            try await service.applyAudioPreset(profile)
            if persistUserCopy, !profile.isBuiltIn {
                onSave(profile)
            }
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }

    func saveUserProfile() {
        guard !profile.isBuiltIn else { return }
        onSave(profile)
    }
}
