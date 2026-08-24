import SwiftUI

enum IGITheme {
    static let cornerRadius: CGFloat = 16
    static let cardRadius: CGFloat = 20
    static let spacingXS: CGFloat = 4
    static let spacingS: CGFloat = 8
    static let spacingM: CGFloat = 16
    static let spacingL: CGFloat = 24
    static let spacingXL: CGFloat = 32

    static let accent = Color.accentColor
    static let cardBackground = Color(.secondarySystemGroupedBackground)
    static let screenBackground = Color(.systemGroupedBackground)
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
                Capsule().fill(Color.secondary.opacity(0.2))
                Capsule()
                    .fill(Color.accentColor.gradient)
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
                .foregroundStyle(.secondary)
            Text(title).font(.title3.weight(.semibold))
            Text(message)
                .multilineTextAlignment(.center)
                .foregroundStyle(.secondary)
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
                .foregroundStyle(.secondary)
        }
    }
}

struct IGIScanningIndicator: View {
    @State private var rotation: Double = 0
    @Environment(\.accessibilityReduceMotion) private var reduceMotion

    var body: some View {
        ZStack {
            Circle()
                .stroke(Color.accentColor.opacity(0.2), lineWidth: 4)
            Circle()
                .trim(from: 0, to: 0.28)
                .stroke(Color.accentColor, style: StrokeStyle(lineWidth: 4, lineCap: .round))
                .rotationEffect(.degrees(rotation))
        }
        .frame(width: 56, height: 56)
        .onAppear {
            guard !reduceMotion else { return }
            withAnimation(.linear(duration: 1).repeatForever(autoreverses: false)) {
                rotation = 360
            }
        }
        .accessibilityLabel("Ricerca in corso")
    }
}
