import Foundation

struct SavedWebRadio: Identifiable, Equatable, Codable, Hashable {
    var id: String
    var name: String
    var url: String

    var hostLabel: String {
        URL(string: url)?.host ?? "URL"
    }
}

enum WebRadioStore {
    private static let key = "igiRadio.webRadios"

    static func load() -> [SavedWebRadio] {
        if let data = UserDefaults.standard.data(forKey: key),
           let saved = try? JSONDecoder().decode([SavedWebRadio].self, from: data),
           !saved.isEmpty {
            return saved
        }
        return StreamPresets.catalog.map {
            SavedWebRadio(id: $0.url, name: $0.name, url: $0.url)
        }
    }

    static func save(_ radios: [SavedWebRadio]) {
        guard let data = try? JSONEncoder().encode(radios) else { return }
        UserDefaults.standard.set(data, forKey: key)
    }
}
