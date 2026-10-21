import Foundation
import Observation

@Observable
final class AudioProfilesViewModel {
    private let service: any DigiRadioService

    var builtIn: [AudioProfileTemplate] = AudioProfileLibrary.builtIn
    var userProfiles: [AudioProfileTemplate] = []
    var activeProfileID: String?
    var isLoading = false
    var isApplying = false
    var errorMessage: String?

    var isConnected: Bool { service.state.connection.isConnected }

    init(service: any DigiRadioService) {
        self.service = service
        reloadUserProfiles()
    }

    func reloadUserProfiles() {
        userProfiles = AudioProfileStore.loadUserProfiles()
    }

    func load() async {
        isLoading = true
        errorMessage = nil
        defer { isLoading = false }
        do {
            try await service.refreshAudioProfile()
            detectActiveProfile()
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }

    func detectActiveProfile() {
        let audio = service.state.audio
        activeProfileID = AudioProfileStore.allProfiles().first {
            AudioProfileLibrary.matchActive($0, eq: audio.eq, enhancements: audio.enhancements)
        }?.id
    }

    func apply(_ profile: AudioProfileTemplate) async {
        guard service.state.connection.isConnected else {
            errorMessage = String(localized: "Connetti igiRadio a DigiRadio prima di applicare un profilo.")
            return
        }
        guard !isApplying else { return }

        isApplying = true
        errorMessage = nil
        defer { isApplying = false }
        do {
            try await service.applyAudioPreset(profile)
            try await service.refreshAudioProfile()
            activeProfileID = profile.id
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }

    func restoreFactoryAudio() async {
        guard !isApplying else { return }
        isApplying = true
        errorMessage = nil
        defer { isApplying = false }
        do {
            try await service.resetAudio()
            try await service.refreshAudioProfile()
            activeProfileID = AudioProfileLibrary.builtIn.first?.id
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }

    func deleteUserProfile(_ profile: AudioProfileTemplate) {
        guard !profile.isBuiltIn else { return }
        userProfiles.removeAll { $0.id == profile.id }
        AudioProfileStore.saveUserProfiles(userProfiles)
        if activeProfileID == profile.id { activeProfileID = nil }
    }

    func saveUserProfile(_ profile: AudioProfileTemplate) {
        var copy = profile
        copy.isBuiltIn = false
        if let index = userProfiles.firstIndex(where: { $0.id == copy.id }) {
            userProfiles[index] = copy
        } else {
            userProfiles.append(copy)
        }
        AudioProfileStore.saveUserProfiles(userProfiles)
    }

    func profileFromDevice(name: String) -> AudioProfileTemplate {
        let audio = service.state.audio
        return AudioProfileTemplate(
            id: UUID().uuidString,
            name: name,
            subtitle: String(localized: "Creato dal dispositivo"),
            systemImage: "waveform.path.ecg",
            isBuiltIn: false,
            eq: audio.eq.isEmpty ? AudioProfileLibrary.builtIn[0].eq : audio.eq,
            enhancements: audio.enhancements
        )
    }
}
