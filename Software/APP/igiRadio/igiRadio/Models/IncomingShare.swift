import Foundation

enum IncomingShare: Equatable, Sendable {
    case webStream(URL)
    case otherApp(URL)

    static func parse(openURL url: URL) -> IncomingShare? {
        if url.scheme?.lowercased() == "http" || url.scheme?.lowercased() == "https" {
            return classify(url)
        }
        guard url.scheme?.lowercased() == "igiradio" else { return nil }
        let components = URLComponents(url: url, resolvingAgainstBaseURL: false)
        if let raw = components?.queryItems?.first(where: { $0.name == "u" })?.value,
           let shared = urlFromSharedText(raw) {
            return classify(shared)
        }
        if (components?.host ?? "").lowercased() == "share" {
            return .otherApp(URL(string: "https://music.youtube.com")!)
        }
        return nil
    }

    /// Link left by the share extension in the shared App Group.
    static func consumePendingShare() -> IncomingShare? {
        let defaults = SharedAppGroup.defaults
        let raw = defaults?.string(forKey: SharedAppGroup.pendingShareKey)
        defaults?.removeObject(forKey: SharedAppGroup.pendingShareKey)
        guard let raw, !raw.isEmpty else { return nil }
        if let shared = urlFromSharedText(raw) {
            return classify(shared)
        }
        return .otherApp(URL(string: "https://music.youtube.com")!)
    }

    static func classify(_ url: URL) -> IncomingShare? {
        if isDocumentOrJunk(url) { return nil }
        if isDirectAudioStream(url) { return .webStream(url) }
        if isAppMediaLink(url) { return .otherApp(url) }
        return nil
    }

    static func urlFromSharedText(_ raw: String) -> URL? {
        let trimmed = raw.trimmingCharacters(in: .whitespacesAndNewlines)
        if let url = URL(string: trimmed), let scheme = url.scheme?.lowercased(),
           scheme == "http" || scheme == "https" {
            return url
        }
        let detector = try? NSDataDetector(types: NSTextCheckingResult.CheckingType.link.rawValue)
        let range = NSRange(trimmed.startIndex..<trimmed.endIndex, in: trimmed)
        return detector?.firstMatch(in: trimmed, options: [], range: range)?.url
    }

    static func isDocumentOrJunk(_ url: URL) -> Bool {
        let path = url.path.lowercased()
        let junk = [
            ".pdf", ".doc", ".docx", ".xls", ".xlsx", ".ppt", ".pptx",
            ".zip", ".rar", ".7z", ".pages", ".numbers", ".key", ".rtf", ".epub",
        ]
        return junk.contains { path.hasSuffix($0) }
    }

    static func isDirectAudioStream(_ url: URL) -> Bool {
        let path = url.path.lowercased()
        let audioSuffixes = [".mp3", ".aac", ".m4a", ".ogg", ".opus", ".wav", ".m3u", ".m3u8", ".pls"]
        if audioSuffixes.contains(where: { path.hasSuffix($0) }) { return true }
        let host = url.host?.lowercased() ?? ""
        return host.contains("icecast")
            || host.contains("shoutcast")
            || path.contains("/stream")
            || path.contains("icecast")
    }

    static func isAppMediaLink(_ url: URL) -> Bool {
        let host = url.host?.lowercased() ?? ""
        let markers = [
            "youtube.com", "youtu.be", "spotify.com", "music.apple.com",
            "deezer.com", "soundcloud.com", "tidal.com", "music.amazon.com",
            "bandcamp.com", "mixcloud.com", "pandora.com", "audiomack.com",
        ]
        return markers.contains { host.contains($0) }
    }
}
