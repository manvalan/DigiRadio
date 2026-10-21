import Foundation

/// Normalizza MAC per API BT1035 (12 cifre esadecimali, senza separatori).
enum BtMacNormalizer {
    static func forApi(_ raw: String) -> String? {
        let hex = raw.uppercased().filter(\.isHexDigit)
        guard hex.count == 12 else { return nil }
        return hex
    }
}
