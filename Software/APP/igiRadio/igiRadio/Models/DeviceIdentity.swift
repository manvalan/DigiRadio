import Foundation

/// Identità on-air del dispositivo (firmware rinominato DigiRadio → igiRadio).
enum DeviceIdentity {
    static let productName = "igiRadio"
    static let blePrefix = "igiRadio"
    static let ssidPrefix = "igiRadio-"
    static let mdnsPrefix = "igiradio-"

    static let legacyBlePrefix = "DigiRadio"
    static let legacySsidPrefix = "DigiRadio-"
    static let legacyMdnsPrefix = "digiradio-"

    /// BLE advertising / SoftAP SSID (`igiRadio-CC4DB4` o legacy `DigiRadio-CC4DB4`).
    static func matchesAdvertisedName(_ name: String) -> Bool {
        let trimmed = name.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return false }
        return trimmed.localizedCaseInsensitiveHasPrefix(blePrefix)
            || trimmed.localizedCaseInsensitiveHasPrefix(legacyBlePrefix)
    }

    /// Servizio Bonjour `_http._tcp`.
    static func matchesBonjourServiceName(_ name: String) -> Bool {
        let lowered = name.lowercased()
        return lowered.contains("igiradio") || lowered.contains("digiradio")
    }

    /// Hostname mDNS HTTP da nome BLE/SSID.
    static func httpHost(fromAdvertisedName name: String) -> String? {
        if let suffix = serialSuffix(in: name, prefix: ssidPrefix) {
            return "http://\(mdnsPrefix)\(suffix).local"
        }
        if let suffix = serialSuffix(in: name, prefix: legacySsidPrefix) {
            return "http://\(legacyMdnsPrefix)\(suffix).local"
        }
        return nil
    }

    /// Se l'utente ha salvato il vecchio hostname, prova anche quello nuovo (e viceversa).
    static func connectionCandidates(from url: String) -> [String] {
        var hosts = [url]
        let lowered = url.lowercased()
        if lowered.contains(legacyMdnsPrefix) {
            hosts.append(
                url.replacingOccurrences(
                    of: legacyMdnsPrefix,
                    with: mdnsPrefix,
                    options: .caseInsensitive
                )
            )
        }
        if lowered.contains(mdnsPrefix) {
            hosts.append(
                url.replacingOccurrences(
                    of: mdnsPrefix,
                    with: legacyMdnsPrefix,
                    options: .caseInsensitive
                )
            )
        }
        var seen = Set<String>()
        return hosts.filter { seen.insert($0).inserted }
    }

    private static func serialSuffix(in name: String, prefix: String) -> String? {
        guard name.count > prefix.count,
              name.lowercased().hasPrefix(prefix.lowercased()) else { return nil }
        let suffix = String(name.dropFirst(prefix.count)).lowercased()
        return suffix.isEmpty ? nil : suffix
    }
}

private extension String {
    func localizedCaseInsensitiveHasPrefix(_ prefix: String) -> Bool {
        lowercased().hasPrefix(prefix.lowercased())
    }
}
