import Foundation

enum BassBoostPreferences {
    private static let enabledKey = "igiRadio.bassBoost.enabled"
    private static let levelKey = "igiRadio.bassBoost.level"

    static let defaultLevel = 45

    static var isEnabled: Bool {
        UserDefaults.standard.bool(forKey: enabledKey)
    }

    static var level: Int {
        let stored = UserDefaults.standard.integer(forKey: levelKey)
        return stored > 0 ? stored : defaultLevel
    }

    static func save(enabled: Bool, level: Int) {
        UserDefaults.standard.set(enabled, forKey: enabledKey)
        UserDefaults.standard.set(max(1, min(100, level)), forKey: levelKey)
    }
}
