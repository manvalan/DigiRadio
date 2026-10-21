import UIKit
import UniformTypeIdentifiers

@objc(ShareViewController)
final class ShareViewController: UIViewController {
    private let status = UILabel()

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .systemBackground
        preferredContentSize = CGSize(width: 320, height: 140)
        status.text = "Invio a igiRadio…"
        status.font = .preferredFont(forTextStyle: .body)
        status.textAlignment = .center
        status.numberOfLines = 0
        status.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(status)
        NSLayoutConstraint.activate([
            status.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            status.centerYAnchor.constraint(equalTo: view.centerYAnchor),
            status.leadingAnchor.constraint(equalTo: view.layoutMarginsGuide.leadingAnchor),
            status.trailingAnchor.constraint(equalTo: view.layoutMarginsGuide.trailingAnchor),
        ])
        Task { await run() }
    }

    private func run() async {
        let shared = await firstSharedURL() ?? "https://music.youtube.com"
        UIPasteboard.general.addItems([["com.digiradio.igiRadio.share": shared]])
        let open = Self.appURL(for: shared) ?? URL(string: "igiradio://share")!
        openApp(open)
        await configureRadio()
        status.text = "Fatto. igiRadio è su Stream."
        try? await Task.sleep(for: .milliseconds(700))
        extensionContext?.completeRequest(returningItems: nil, completionHandler: nil)
    }

    private func firstSharedURL() async -> String? {
        let items = extensionContext?.inputItems.compactMap { $0 as? NSExtensionItem } ?? []
        for item in items {
            if let text = item.attributedContentText?.string, let url = Self.url(in: text) {
                return url
            }
            for provider in item.attachments ?? [] {
                for type in [UTType.url.identifier, "public.url", UTType.plainText.identifier, "public.text"] {
                    if provider.hasItemConformingToTypeIdentifier(type),
                       let value = try? await loadItem(provider, type: type) {
                        return value
                    }
                }
            }
        }
        return nil
    }

    private func loadItem(_ provider: NSItemProvider, type: String) async throws -> String? {
        try await withCheckedThrowingContinuation { continuation in
            provider.loadItem(forTypeIdentifier: type, options: nil) { item, error in
                if let error {
                    continuation.resume(throwing: error)
                    return
                }
                if let url = item as? URL {
                    continuation.resume(returning: url.absoluteString)
                } else if let text = item as? String {
                    continuation.resume(returning: Self.url(in: text) ?? text)
                } else {
                    continuation.resume(returning: nil)
                }
            }
        }
    }

    private func configureRadio() async {
        let host = (UIPasteboard.general.value(forPasteboardType: "com.digiradio.igiRadio.host") as? String)
            ?? UIPasteboard.general.string
        guard let host, !host.isEmpty else { return }
        let base = host.hasPrefix("http") ? host : "http://\(host)"
        guard let streaming = URL(string: base + "/api/streaming"),
              let profileURL = URL(string: base + "/api/audio/profile") else { return }

        _ = try? await postJSON(streaming, [
            "enabled": false,
            "url": "http://edge.radiomontecarlo.net/RMC.mp3",
        ])
        guard var profile = try? await getJSON(profileURL) else { return }
        profile["active_source"] = "bluetooth"
        if var enhancements = profile["enhancements"] as? [String: Any] {
            enhancements["stereo_level"] = 0
            enhancements["bass_level"] = 0
            profile["enhancements"] = enhancements
        }
        _ = try? await putJSON(profileURL, profile)
    }

    private func getJSON(_ url: URL) async throws -> [String: Any] {
        let (data, _) = try await URLSession.shared.data(from: url)
        guard let json = try JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            throw URLError(.cannotParseResponse)
        }
        return json
    }

    private func postJSON(_ url: URL, _ body: [String: Any]) async throws -> Data {
        try await sendJSON(url, method: "POST", body: body)
    }

    private func putJSON(_ url: URL, _ body: [String: Any]) async throws -> Data {
        try await sendJSON(url, method: "PUT", body: body)
    }

    private func sendJSON(_ url: URL, method: String, body: [String: Any]) async throws -> Data {
        var request = URLRequest(url: url)
        request.httpMethod = method
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = try JSONSerialization.data(withJSONObject: body)
        request.timeoutInterval = 4
        let (data, _) = try await URLSession.shared.data(for: request)
        return data
    }

    private static func url(in text: String) -> String? {
        let detector = try? NSDataDetector(types: NSTextCheckingResult.CheckingType.link.rawValue)
        let range = NSRange(text.startIndex..<text.endIndex, in: text)
        return detector?.firstMatch(in: text, options: [], range: range)?.url?.absoluteString
    }

    private static func appURL(for shared: String) -> URL? {
        var parts = URLComponents()
        parts.scheme = "igiradio"
        parts.host = "share"
        parts.queryItems = [URLQueryItem(name: "u", value: shared)]
        return parts.url
    }

    private func openApp(_ url: URL) {
        if let appClass = NSClassFromString("UIApplication") as? NSObject.Type {
            let sharedSel = NSSelectorFromString("sharedApplication")
            if let app = appClass.perform(sharedSel)?.takeUnretainedValue() as? NSObject {
                callOpen(app, url: url)
            }
        }
        var responder: UIResponder? = self
        let legacy = NSSelectorFromString("openURL:")
        while let current = responder {
            if current.responds(to: legacy) {
                current.perform(legacy, with: url)
                break
            }
            responder = current.next
        }
    }

    private func callOpen(_ target: NSObject, url: URL) {
        let modern = NSSelectorFromString("openURL:options:completionHandler:")
        if target.responds(to: modern), let imp = target.method(for: modern) {
            typealias Fn = @convention(c) (Any, Selector, URL, NSDictionary, (@convention(block) (Bool) -> Void)?) -> Void
            let done: @convention(block) (Bool) -> Void = { _ in }
            unsafeBitCast(imp, to: Fn.self)(target, modern, url, [:], done)
            return
        }
        let legacy = NSSelectorFromString("openURL:")
        if target.responds(to: legacy) {
            target.perform(legacy, with: url)
        }
    }
}
