import Foundation
import UIKit

enum ConnectionPreferences {
    private static let lastHostKey = "igiRadio.lastHost"
    private static let recentHostsKey = "igiRadio.recentHosts"

    static var lastHost: String? {
        UserDefaults.standard.string(forKey: lastHostKey)
    }

    static var recentHosts: [String] {
        UserDefaults.standard.stringArray(forKey: recentHostsKey) ?? []
    }

    static func saveLastHost(_ rawHost: String) {
        let normalized = HostNormalizer.httpURL(from: rawHost)
        guard !normalized.isEmpty else { return }
        UserDefaults.standard.set(normalized, forKey: lastHostKey)
        var recent = recentHosts.filter { $0 != normalized }
        recent.insert(normalized, at: 0)
        UserDefaults.standard.set(Array(recent.prefix(5)), forKey: recentHostsKey)
        if let host = URL(string: normalized)?.host {
            UIPasteboard.general.addItems([["com.digiradio.igiRadio.host": host]])
        }
    }
}

enum HostNormalizer {
    /// Normalizes user input to `http://host` with lowercase hostname.
    static func httpURL(from raw: String) -> String {
        let trimmed = raw.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return "" }

        var candidate = trimmed
        if !candidate.hasPrefix("http://"), !candidate.hasPrefix("https://") {
            candidate = "http://\(candidate)"
        }

        guard var components = URLComponents(string: candidate), let host = components.host else {
            return candidate
        }
        components.host = host.lowercased()
        components.scheme = "http"
        return components.string ?? candidate
    }

    /// Display form without scheme for text fields.
    static func displayHost(from raw: String) -> String {
        let url = httpURL(from: raw)
        guard let host = URL(string: url)?.host else { return raw }
        return host
    }
}
