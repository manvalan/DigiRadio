import SwiftUI
import UIKit

enum IGITheme {
    static let cornerRadius: CGFloat = 16
    static let cardRadius: CGFloat = 20
    static let spacingXS: CGFloat = 4
    static let spacingS: CGFloat = 8
    static let spacingM: CGFloat = 16
    static let spacingL: CGFloat = 24
    static let spacingXL: CGFloat = 32

    /// Bianco sporco, leggermente smorzato.
    static let canvas = Color(red: 0.957, green: 0.945, blue: 0.922)
    /// Carta un filo più chiara del fondo.
    static let card = Color(red: 0.988, green: 0.980, blue: 0.965)
    /// Blu scuro spento per testi e icone.
    static let ink = Color(red: 0.165, green: 0.227, blue: 0.365)
    static let inkMuted = Color(red: 0.32, green: 0.38, blue: 0.50)
    static let accent = Color(red: 0.20, green: 0.29, blue: 0.47)

    static let cardBackground = card
    static let screenBackground = canvas

    static var canvasUIColor: UIColor {
        UIColor(red: 0.957, green: 0.945, blue: 0.922, alpha: 1)
    }

    static var inkUIColor: UIColor {
        UIColor(red: 0.165, green: 0.227, blue: 0.365, alpha: 1)
    }

    static func applyGlobalAppearance() {
        let canvas = canvasUIColor
        let ink = inkUIColor
        let muted = UIColor(red: 0.32, green: 0.38, blue: 0.50, alpha: 1)

        let nav = UINavigationBarAppearance()
        nav.configureWithOpaqueBackground()
        nav.backgroundColor = canvas
        nav.shadowColor = .clear
        nav.titleTextAttributes = [.foregroundColor: ink]
        nav.largeTitleTextAttributes = [.foregroundColor: ink]
        UINavigationBar.appearance().standardAppearance = nav
        UINavigationBar.appearance().scrollEdgeAppearance = nav
        UINavigationBar.appearance().compactAppearance = nav
        UINavigationBar.appearance().tintColor = ink

        let tab = UITabBarAppearance()
        tab.configureWithOpaqueBackground()
        tab.backgroundColor = canvas
        tab.shadowColor = UIColor.black.withAlphaComponent(0.06)
        UITabBar.appearance().standardAppearance = tab
        UITabBar.appearance().scrollEdgeAppearance = tab
        UITabBar.appearance().tintColor = ink
        UITabBar.appearance().unselectedItemTintColor = muted

        UITableView.appearance().backgroundColor = canvas
        UICollectionView.appearance().backgroundColor = canvas
        UITextField.appearance().tintColor = ink
    }
}

struct IGICard<Content: View>: View {
    @ViewBuilder var content: () -> Content

    var body: some View {
        content()
            .padding(IGITheme.spacingM)
            .background(IGITheme.cardBackground, in: RoundedRectangle(cornerRadius: IGITheme.cardRadius, style: .continuous))
    }
}

struct IGIStatusDot: View {
    var isConnected: Bool

    var body: some View {
        Circle()
            .fill(isConnected ? Color.green : Color.orange)
            .frame(width: 10, height: 10)
            .accessibilityLabel(isConnected ? "Connesso" : "Non connesso")
    }
}

struct IGIPrimaryButton: View {
    var title: String
    var systemImage: String?
    var action: () -> Void

    var body: some View {
        Button(action: action) {
            Label {
                Text(title)
                    .fontWeight(.semibold)
            } icon: {
                if let systemImage {
                    Image(systemName: systemImage)
                }
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, 14)
        }
        .buttonStyle(.borderedProminent)
        .controlSize(.large)
    }
}

struct IGISignalBar: View {
    var level: Double

    var body: some View {
        GeometryReader { proxy in
            ZStack(alignment: .leading) {
                Capsule().fill(IGITheme.ink.opacity(0.12))
                Capsule()
                    .fill(IGITheme.accent.gradient)
                    .frame(width: proxy.size.width * max(0, min(1, level)))
            }
        }
        .frame(height: 8)
        .accessibilityLabel("Segnale")
        .accessibilityValue("\(Int(level * 100)) percento")
    }
}

struct IGISectionHeader: View {
    var title: String

    var body: some View {
        Text(title)
            .font(.title3.weight(.semibold))
            .foregroundStyle(IGITheme.ink)
            .frame(maxWidth: .infinity, alignment: .leading)
    }
}

struct IGIEmptyState: View {
    var title: String
    var message: String
    var systemImage: String

    var body: some View {
        VStack(spacing: IGITheme.spacingM) {
            Image(systemName: systemImage)
                .font(.system(size: 44))
                .foregroundStyle(IGITheme.inkMuted)
            Text(title)
                .font(.title3.weight(.semibold))
                .foregroundStyle(IGITheme.ink)
            Text(message)
                .multilineTextAlignment(.center)
                .foregroundStyle(IGITheme.inkMuted)
        }
        .padding(IGITheme.spacingXL)
    }
}

struct IGIVolumeSlider: View {
    @Binding var value: Double
    var onEditingEnded: ((Double) -> Void)?

    var body: some View {
        VStack(alignment: .leading, spacing: IGITheme.spacingS) {
            HStack {
                Image(systemName: "speaker.fill")
                Slider(value: $value, in: 0 ... 100, step: 1) { editing in
                    if !editing { onEditingEnded?(value) }
                }
                Image(systemName: "speaker.wave.3.fill")
            }
            Text("Volume \(Int(value))")
                .font(.caption)
                .foregroundStyle(IGITheme.inkMuted)
        }
        .foregroundStyle(IGITheme.ink)
    }
}

struct IGIScanningIndicator: View {
    var body: some View {
        ProgressView()
            .accessibilityLabel("Ricerca in corso")
    }
}

struct IGIReadableWidthModifier: ViewModifier {
    @Environment(\.horizontalSizeClass) private var sizeClass
    var width: CGFloat

    func body(content: Content) -> some View {
        if sizeClass == .regular {
            content
                .frame(maxWidth: width)
                .frame(maxWidth: .infinity)
        } else {
            content
        }
    }
}

extension View {
    /// Centra il contenuto su iPad, stile Impostazioni / Mail.
    func igiReadableWidth(_ width: CGFloat = 720) -> some View {
        modifier(IGIReadableWidthModifier(width: width))
    }

    func igiThemedScreen() -> some View {
        self
            .tint(IGITheme.accent)
            .background(IGITheme.canvas.ignoresSafeArea())
            .scrollContentBackground(.hidden)
            .toolbarBackground(IGITheme.canvas, for: .navigationBar)
            .toolbarBackground(.visible, for: .navigationBar)
            .toolbarBackground(IGITheme.canvas, for: .tabBar)
            .toolbarBackground(.visible, for: .tabBar)
            .toolbarColorScheme(.light, for: .navigationBar, .tabBar)
    }
}
