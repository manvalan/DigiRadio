import Foundation

/// Persists user-created audio profiles on the iPhone (firmware stores one active profile only).
enum AudioProfileStore {
    private static let key = "igiRadio.userAudioProfiles"

    static func loadUserProfiles() -> [AudioProfileTemplate] {
        guard let data = UserDefaults.standard.data(forKey: key) else { return [] }
        return (try? JSONDecoder().decode([AudioProfileTemplate].self, from: data)) ?? []
    }

    static func saveUserProfiles(_ profiles: [AudioProfileTemplate]) {
        guard let data = try? JSONEncoder().encode(profiles) else { return }
        UserDefaults.standard.set(data, forKey: key)
    }

    static func allProfiles() -> [AudioProfileTemplate] {
        AudioProfileLibrary.builtIn + loadUserProfiles()
    }
}
