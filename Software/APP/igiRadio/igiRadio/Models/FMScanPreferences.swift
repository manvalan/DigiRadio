import Foundation

/// Soglie scansione FM persistite sull'iPhone (non sul DigiRadio).
enum FMScanPreferences {
    static let rssiKey = "igiRadio.fmScan.minRssiDbuv"
    static let snrKey = "igiRadio.fmScan.minSnrDb"

    static let defaultRssiDbuv = 5
    /// 0 = non filtrare per SNR (come `scanFullFmBand` nel firmware).
    static let defaultSnrDb = 0
    static let rssiRange = 0 ... 30
    static let snrRange = 0 ... 20

    static var minRssiDbuv: Int {
        clamped(UserDefaults.standard.object(forKey: rssiKey) as? Int ?? defaultRssiDbuv, rssiRange)
    }

    static var minSnrDb: Int {
        clamped(UserDefaults.standard.object(forKey: snrKey) as? Int ?? defaultSnrDb, snrRange)
    }

    static func save(minRssiDbuv: Int, minSnrDb: Int) {
        UserDefaults.standard.set(clamped(minRssiDbuv, rssiRange), forKey: rssiKey)
        UserDefaults.standard.set(clamped(minSnrDb, snrRange), forKey: snrKey)
    }

    static func resetToDefault() {
        save(minRssiDbuv: defaultRssiDbuv, minSnrDb: defaultSnrDb)
    }

    private static func clamped(_ value: Int, _ range: ClosedRange<Int>) -> Int {
        min(range.upperBound, max(range.lowerBound, value))
    }
}
