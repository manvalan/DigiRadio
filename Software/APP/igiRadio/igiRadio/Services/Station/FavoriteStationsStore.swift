import Foundation

/// Preferiti — in app e, se connessi, anche come preset hardware sul DigiRadio (max 20).
enum FavoriteStationsStore {
    private static let key = "igiRadio.favoriteStations"

    static func load() -> [ScannedStation] {
        guard let data = UserDefaults.standard.data(forKey: key),
              let stations = try? JSONDecoder().decode([ScannedStation].self, from: data) else {
            return []
        }
        return Array(stations.prefix(DigiRadioPresetLimits.maxPresets))
    }

    static func save(_ stations: [ScannedStation]) {
        let capped = Array(stations.prefix(DigiRadioPresetLimits.maxPresets))
        guard let data = try? JSONEncoder().encode(capped) else { return }
        UserDefaults.standard.set(data, forKey: key)
    }
}
