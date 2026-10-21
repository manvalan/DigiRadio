import Foundation

private struct ScannedStationLibrary: Codable {
    var fm: [ScannedStation] = []
    var dab: [ScannedStation] = []
}

/// Elenco stazioni scansionate persistito sull'iPhone. Il DigiRadio ha solo 20 preset hardware.
enum ScannedStationStore {
    private static let key = "igiRadio.stationCatalog.v1"

    static func load() -> (fm: [ScannedStation], dab: [ScannedStation]) {
        guard let data = UserDefaults.standard.data(forKey: key),
              let library = try? JSONDecoder().decode(ScannedStationLibrary.self, from: data) else {
            return ([], [])
        }
        return (library.fm, library.dab)
    }

    static func save(fm: [ScannedStation], dab: [ScannedStation]) {
        let library = ScannedStationLibrary(fm: fm, dab: dab)
        guard let data = try? JSONEncoder().encode(library) else { return }
        UserDefaults.standard.set(data, forKey: key)
    }
}
