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

    init(service: any DigiRadioService) {
        self.service = service
        reloadUserProfiles()
    }

    func reloadUserProfiles() {
        userProfiles = AudioProfileStore.loadUserProfiles()
    }

    func load() async {
        isLoading = true
        defer { isLoading = false }
        try? await service.refreshAudioProfile()
        detectActiveProfile()
    }

    func detectActiveProfile() {
        let audio = service.state.audio
        activeProfileID = AudioProfileStore.allProfiles().first {
            AudioProfileLibrary.matchActive($0, eq: audio.eq, enhancements: audio.enhancements)
        }?.id
    }

    func apply(_ profile: AudioProfileTemplate) async {
        isApplying = true
        errorMessage = nil
        defer { isApplying = false }
        do {
            let audio = service.state.audio
            let dto = profile.profileDTO(mixer: audio.mixer, master: audio.master)
            try await service.applyAudioProfile(dto)
            activeProfileID = profile.id
        } catch {
            errorMessage = error.localizedDescription
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

    func duplicateAsUser(from profile: AudioProfileTemplate, name: String) -> AudioProfileTemplate {
        AudioProfileTemplate(
            id: UUID().uuidString,
            name: name,
            subtitle: "Profilo personalizzato",
            systemImage: "slider.horizontal.3",
            isBuiltIn: false,
            eq: profile.eq,
            enhancements: profile.enhancements
        )
    }

    func profileFromDevice(name: String) -> AudioProfileTemplate {
        let audio = service.state.audio
        return AudioProfileTemplate(
            id: UUID().uuidString,
            name: name,
            subtitle: "Creato dal dispositivo",
            systemImage: "waveform.path.ecg",
            isBuiltIn: false,
            eq: audio.eq.isEmpty ? AudioProfileLibrary.builtIn[0].eq : audio.eq,
            enhancements: audio.enhancements
        )
    }
}
