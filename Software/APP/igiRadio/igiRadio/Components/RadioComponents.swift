import SwiftUI

// MARK: - Shared premium surface

extension View {
    func igiPremiumCard() -> some View {
        self
            .padding(IGITheme.spacingM)
            .background {
                RoundedRectangle(cornerRadius: IGITheme.cardRadius, style: .continuous)
                    .fill(.ultraThinMaterial)
                    .shadow(color: .black.opacity(0.06), radius: 12, y: 4)
            }
    }
}

struct IGIHeroBackground: View {
    var body: some View {
        LinearGradient(
            colors: [
                IGITheme.screenBackground,
                IGITheme.accent.opacity(0.08),
                IGITheme.screenBackground
            ],
            startPoint: .topLeading,
            endPoint: .bottomTrailing
        )
        .ignoresSafeArea()
    }
}

// MARK: - Transport

struct IGITransportCluster: View {
    var onPrevious: () -> Void
    var onPlay: () -> Void
    var onNext: () -> Void

    var body: some View {
        HStack(spacing: IGITheme.spacingL) {
            transportButton(icon: "backward.fill", size: 56, prominent: false, action: onPrevious)
            transportButton(icon: "play.fill", size: 76, prominent: true, action: onPlay)
            transportButton(icon: "forward.fill", size: 56, prominent: false, action: onNext)
        }
        .frame(maxWidth: .infinity)
    }

    @ViewBuilder
    private func transportButton(icon: String, size: CGFloat, prominent: Bool, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Image(systemName: icon)
                .font(prominent ? .largeTitle : .title2)
                .foregroundStyle(prominent ? .white : .primary)
                .frame(width: size, height: size)
                .background {
                    if prominent {
                        Circle().fill(IGITheme.accent.gradient)
                            .shadow(color: IGITheme.accent.opacity(0.35), radius: 12, y: 6)
                    } else {
                        Circle().fill(.ultraThinMaterial)
                    }
                }
        }
        .buttonStyle(.plain)
    }
}

// MARK: - Band badge

struct IGIBandBadge: View {
    var band: TunerBand
    var locked: Bool

    var body: some View {
        HStack(spacing: 6) {
            Image(systemName: band == .fm ? "dot.radiowaves.left.and.right" : "antenna.radiowaves.left.and.right")
            Text(band.rawValue.uppercased())
                .fontWeight(.bold)
            if locked {
                Image(systemName: "lock.fill")
                    .font(.caption2)
            }
        }
        .font(.caption)
        .padding(.horizontal, 12)
        .padding(.vertical, 6)
        .background(IGITheme.accent.opacity(0.15), in: Capsule())
        .foregroundStyle(IGITheme.accent)
    }
}

// MARK: - Preset chip

struct IGIPresetChip: View {
    var name: String
    var subtitle: String
    var action: () -> Void

    var body: some View {
        Button(action: action) {
            VStack(alignment: .leading, spacing: 4) {
                Text(name)
                    .font(.subheadline.weight(.semibold))
                    .lineLimit(1)
                Text(subtitle)
                    .font(.caption2)
                    .foregroundStyle(.secondary)
            }
            .padding(.horizontal, 14)
            .padding(.vertical, 12)
            .frame(minWidth: 110, alignment: .leading)
            .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 14, style: .continuous))
            .overlay(
                RoundedRectangle(cornerRadius: 14, style: .continuous)
                    .strokeBorder(Color.primary.opacity(0.06))
            )
        }
        .buttonStyle(.plain)
    }
}

// MARK: - Metric bar

struct IGIMetricBar: View {
    var label: String
    var value: String
    var level: Double

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Text(label).font(.caption).foregroundStyle(.secondary)
                Spacer()
                Text(value).font(.caption.monospacedDigit().weight(.medium))
            }
            GeometryReader { geo in
                ZStack(alignment: .leading) {
                    Capsule().fill(Color.primary.opacity(0.08))
                    Capsule()
                        .fill(IGITheme.accent.gradient)
                        .frame(width: geo.size.width * max(0, min(1, level)))
                }
            }
            .frame(height: 6)
        }
    }
}

// MARK: - FM frequency hero

struct IGIFrequencyHero: View {
    @Binding var frequencyMHz: Double
    var stationName: String?
    var isTuning: Bool
    var onTune: () -> Void

    var body: some View {
        VStack(spacing: IGITheme.spacingL) {
            if let stationName, !stationName.isEmpty {
                Text(stationName)
                    .font(.title3.weight(.semibold))
                    .foregroundStyle(IGITheme.accent)
                    .lineLimit(1)
            }

            ZStack {
                Circle()
                    .stroke(Color.primary.opacity(0.06), lineWidth: 20)
                Circle()
                    .trim(from: 0, to: normalizedFrequency)
                    .stroke(
                        AngularGradient(colors: [IGITheme.accent.opacity(0.3), IGITheme.accent], center: .center),
                        style: StrokeStyle(lineWidth: 20, lineCap: .round)
                    )
                    .rotationEffect(.degrees(-90))
                    .animation(.snappy, value: frequencyMHz)

                VStack(spacing: 4) {
                    Text(String(format: "%.2f", frequencyMHz))
                        .font(.system(size: 44, weight: .bold, design: .rounded))
                        .monospacedDigit()
                        .contentTransition(.numericText())
                    Text("MHz")
                        .font(.caption.weight(.semibold))
                        .foregroundStyle(.secondary)
                }
            }
            .frame(width: 220, height: 220)

            Slider(value: $frequencyMHz, in: 64 ... 108, step: 0.05)
                .tint(IGITheme.accent)

            HStack(spacing: IGITheme.spacingM) {
                stepButton("−0.1") { frequencyMHz = max(64, frequencyMHz - 0.1) }
                Button(action: onTune) {
                    Group {
                        if isTuning {
                            ProgressView().tint(.white)
                        } else {
                            Text("Sintonizza")
                                .fontWeight(.semibold)
                        }
                    }
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 14)
                }
                .buttonStyle(.borderedProminent)
                .disabled(isTuning)
                stepButton("+0.1") { frequencyMHz = min(108, frequencyMHz + 0.1) }
            }
        }
        .igiPremiumCard()
    }

    private var normalizedFrequency: Double {
        (frequencyMHz - 64) / (108 - 64)
    }

    @ViewBuilder
    private func stepButton(_ title: String, action: @escaping () -> Void) -> some View {
        Button(title, action: action)
            .font(.subheadline.weight(.semibold).monospacedDigit())
            .frame(width: 56, height: 48)
            .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 12))
            .buttonStyle(.plain)
    }
}

// MARK: - Scan result row

struct IGIScanResultRow: View {
    var title: String
    var frequencyMHz: Double
    var rssi: Int?
    var action: () -> Void

    var body: some View {
        Button(action: action) {
            HStack(spacing: IGITheme.spacingM) {
                ZStack {
                    RoundedRectangle(cornerRadius: 10, style: .continuous)
                        .fill(IGITheme.accent.opacity(0.12))
                        .frame(width: 44, height: 44)
                    Image(systemName: "radio.fill")
                        .foregroundStyle(IGITheme.accent)
                }
                VStack(alignment: .leading, spacing: 2) {
                    Text(title).font(.headline).lineLimit(1)
                    Text(String(format: "%.2f MHz", frequencyMHz))
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
                Spacer()
                if let rssi {
                    Text("\(rssi)")
                        .font(.caption.monospacedDigit().weight(.semibold))
                        .padding(.horizontal, 8)
                        .padding(.vertical, 4)
                        .background(Color.primary.opacity(0.06), in: Capsule())
                }
                Image(systemName: "play.circle.fill")
                    .font(.title2)
                    .foregroundStyle(IGITheme.accent)
            }
            .padding(IGITheme.spacingS)
            .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 14, style: .continuous))
        }
        .buttonStyle(.plain)
    }
}
