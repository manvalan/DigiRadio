import SwiftUI

// MARK: - Vertical fader (mixer channels)

struct IGIVerticalFader: View {
    var label: String
    var icon: String
    @Binding var valueDb: Double
    var range: ClosedRange<Double> = -40 ... 12
    var onCommit: () -> Void

    private let trackHeight: CGFloat = 160

    var body: some View {
        VStack(spacing: IGITheme.spacingS) {
            Text(formattedDb)
                .font(.caption.monospacedDigit().weight(.semibold))
                .foregroundStyle(valueDb > 0 ? IGITheme.accent : .secondary)
                .contentTransition(.numericText())
                .animation(.snappy, value: valueDb)

            GeometryReader { geo in
                let h = geo.size.height
                ZStack(alignment: .bottom) {
                    Capsule()
                        .fill(Color.primary.opacity(0.08))
                    Capsule()
                        .fill(
                            LinearGradient(
                                colors: [IGITheme.accent.opacity(0.35), IGITheme.accent],
                                startPoint: .bottom,
                                endPoint: .top
                            )
                        )
                        .frame(height: fillHeight(total: h))
                }
                .overlay(alignment: .top) {
                    Circle()
                        .fill(.background)
                        .shadow(color: .black.opacity(0.18), radius: 4, y: 2)
                        .overlay(Circle().stroke(IGITheme.accent.opacity(0.5), lineWidth: 2))
                        .frame(width: 28, height: 28)
                        .offset(y: thumbOffset(total: h))
                }
                .gesture(
                    DragGesture(minimumDistance: 0)
                        .onChanged { drag in
                            valueDb = db(from: drag.location.y, height: h)
                        }
                        .onEnded { _ in onCommit() }
                )
                .accessibilityElement(children: .ignore)
                .accessibilityLabel("\(label), \(formattedDb)")
                .accessibilityAdjustableAction { direction in
                    let step = 0.5
                    switch direction {
                    case .increment: valueDb = min(range.upperBound, valueDb + step)
                    case .decrement: valueDb = max(range.lowerBound, valueDb - step)
                    @unknown default: break
                    }
                    onCommit()
                }
            }
            .frame(height: trackHeight)

            Image(systemName: icon)
                .font(.caption)
                .foregroundStyle(.secondary)
            Text(label)
                .font(.caption2.weight(.medium))
                .multilineTextAlignment(.center)
                .lineLimit(2)
                .frame(width: 52)
        }
        .frame(width: 64)
    }

    private var formattedDb: String {
        valueDb >= 0 ? String(format: "+%.1f", valueDb) : String(format: "%.1f", valueDb)
    }

    private func normalized(_ db: Double) -> Double {
        (db - range.lowerBound) / (range.upperBound - range.lowerBound)
    }

    private func fillHeight(total: CGFloat) -> CGFloat {
        max(4, total * normalized(valueDb))
    }

    private func thumbOffset(total: CGFloat) -> CGFloat {
        let travel = total - 28
        return travel * (1 - normalized(valueDb))
    }

    private func db(from y: CGFloat, height: CGFloat) -> Double {
        let clamped = max(0, min(height, y))
        let norm = 1 - (clamped / height)
        let raw = range.lowerBound + norm * (range.upperBound - range.lowerBound)
        return (raw * 2).rounded() / 2
    }
}

// MARK: - Mixer channel group

struct IGIMixerChannelGroup: View {
    var title: String
    var subtitle: String
    var systemImage: String
    @Binding var leftDb: Double
    @Binding var rightDb: Double
    var onCommit: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: IGITheme.spacingM) {
            HStack(spacing: IGITheme.spacingS) {
                Image(systemName: systemImage)
                    .font(.title3)
                    .foregroundStyle(IGITheme.accent)
                    .frame(width: 36, height: 36)
                    .background(IGITheme.accent.opacity(0.12), in: RoundedRectangle(cornerRadius: 10))
                VStack(alignment: .leading, spacing: 2) {
                    Text(title).font(.headline)
                    Text(subtitle).font(.caption).foregroundStyle(.secondary)
                }
            }

            HStack(spacing: IGITheme.spacingL) {
                Spacer()
                IGIVerticalFader(label: "Sinistra", icon: "l.circle", valueDb: $leftDb, onCommit: onCommit)
                IGIVerticalFader(label: "Destra", icon: "r.circle", valueDb: $rightDb, onCommit: onCommit)
                Spacer()
            }
        }
        .igiAudioCard()
    }
}

// MARK: - Graphic equalizer

struct IGIGraphicEqualizer: View {
    @Binding var bands: [EQBandState]
    var onCommit: () -> Void

    private let gainRange: ClosedRange<Double> = -12 ... 12
    private let barMaxHeight: CGFloat = 120

    var body: some View {
        VStack(spacing: IGITheme.spacingM) {
            // Curve preview
            GeometryReader { geo in
                let w = geo.size.width
                let h = geo.size.height
                let sorted = bands.sorted { $0.centerHz < $1.centerHz }
                if sorted.count >= 2 {
                    Path { path in
                        for (i, band) in sorted.enumerated() {
                            let x = w * CGFloat(i) / CGFloat(max(1, sorted.count - 1))
                            let y = h - normalizedGain(band.gainDb) * h
                            if i == 0 { path.move(to: CGPoint(x: x, y: y)) }
                            else { path.addLine(to: CGPoint(x: x, y: y)) }
                        }
                    }
                    .stroke(
                        LinearGradient(colors: [IGITheme.accent, .purple], startPoint: .leading, endPoint: .trailing),
                        style: StrokeStyle(lineWidth: 2.5, lineCap: .round, lineJoin: .round)
                    )
                }
            }
            .frame(height: 48)
            .padding(.horizontal, IGITheme.spacingS)

            HStack(alignment: .bottom, spacing: IGITheme.spacingS) {
                ForEach($bands) { $band in
                    EQBar(
                        centerHz: band.centerHz,
                        gainDb: $band.gainDb,
                        maxHeight: barMaxHeight,
                        range: gainRange,
                        onCommit: onCommit
                    )
                }
            }
            .frame(maxWidth: .infinity)

            HStack {
                Text("-12 dB").font(.caption2).foregroundStyle(.secondary)
                Spacer()
                Text("0").font(.caption2.weight(.medium))
                Spacer()
                Text("+12 dB").font(.caption2).foregroundStyle(.secondary)
            }
        }
        .igiAudioCard()
    }

    private func normalizedGain(_ db: Double) -> CGFloat {
        CGFloat((db - gainRange.lowerBound) / (gainRange.upperBound - gainRange.lowerBound))
    }
}

private struct EQBar: View {
    var centerHz: Double
    @Binding var gainDb: Double
    var maxHeight: CGFloat
    var range: ClosedRange<Double>
    var onCommit: () -> Void

    var body: some View {
        VStack(spacing: 6) {
            Text(gainLabel)
                .font(.system(size: 10, weight: .semibold, design: .rounded))
                .monospacedDigit()
                .foregroundStyle(gainDb == 0 ? .secondary : IGITheme.accent)
                .frame(height: 14)

            GeometryReader { geo in
                let h = geo.size.height
                let mid = h * 0.5
                ZStack(alignment: .bottom) {
                    RoundedRectangle(cornerRadius: 8, style: .continuous)
                        .fill(Color.primary.opacity(0.06))

                    if gainDb >= 0 {
                        RoundedRectangle(cornerRadius: 8, style: .continuous)
                            .fill(barGradient)
                            .frame(height: max(4, normalized(gainDb) * mid))
                            .offset(y: -mid)
                    } else {
                        RoundedRectangle(cornerRadius: 8, style: .continuous)
                            .fill(barGradient)
                            .frame(height: max(4, normalized(abs(gainDb)) * mid))
                            .offset(y: -(mid - max(4, normalized(abs(gainDb)) * mid)))
                    }
                }
                .overlay {
                    Rectangle()
                        .fill(Color.primary.opacity(0.2))
                        .frame(height: 1)
                        .position(x: geo.size.width / 2, y: mid)
                }
                .gesture(
                    DragGesture(minimumDistance: 0)
                        .onChanged { drag in
                            gainDb = db(from: drag.location.y, height: h)
                        }
                        .onEnded { _ in onCommit() }
                )
            }
            .frame(height: maxHeight)

            Text(freqLabel)
                .font(.system(size: 9, weight: .medium, design: .rounded))
                .foregroundStyle(.secondary)
                .lineLimit(1)
                .minimumScaleFactor(0.7)
        }
        .frame(maxWidth: .infinity)
        .accessibilityElement(children: .ignore)
        .accessibilityLabel("Banda \(freqLabel)")
        .accessibilityValue(gainLabel)
    }

    private var gainLabel: String {
        gainDb >= 0 ? String(format: "+%.1f", gainDb) : String(format: "%.1f", gainDb)
    }

    private var freqLabel: String {
        if centerHz >= 1000 {
            return String(format: "%.0fk", centerHz / 1000)
        }
        return String(format: "%.0f", centerHz)
    }

    private var barGradient: LinearGradient {
        if gainDb >= 0 {
            return LinearGradient(colors: [IGITheme.accent.opacity(0.5), IGITheme.accent], startPoint: .bottom, endPoint: .top)
        }
        return LinearGradient(colors: [.orange.opacity(0.4), .orange], startPoint: .top, endPoint: .bottom)
    }

    private func normalized(_ db: Double) -> CGFloat {
        CGFloat(db / range.upperBound)
    }

    private func db(from y: CGFloat, height: CGFloat) -> Double {
        let clamped = max(0, min(height, y))
        let norm = 1 - (clamped / height)
        let raw = range.lowerBound + norm * (range.upperBound - range.lowerBound)
        return (raw * 2).rounded() / 2
    }
}

// MARK: - Enhancement arc dial

struct IGIEnhancementDial: View {
    var title: String
    var systemImage: String
    @Binding var level: Double
    var onCommit: () -> Void

    @State private var dragOrigin: Double?

    var body: some View {
        VStack(spacing: IGITheme.spacingS) {
            ZStack {
                Circle()
                    .stroke(Color.primary.opacity(0.08), lineWidth: 10)
                Circle()
                    .trim(from: 0, to: level / 100)
                    .stroke(
                        AngularGradient(colors: [IGITheme.accent.opacity(0.4), IGITheme.accent], center: .center),
                        style: StrokeStyle(lineWidth: 10, lineCap: .round)
                    )
                    .rotationEffect(.degrees(-90))
                VStack(spacing: 2) {
                    Image(systemName: systemImage)
                        .font(.title3)
                        .foregroundStyle(IGITheme.accent)
                    Text("\(Int(level))")
                        .font(.title2.weight(.bold).monospacedDigit())
                }
            }
            .frame(width: 88, height: 88)
            .gesture(
                DragGesture(minimumDistance: 0)
                    .onChanged { drag in
                        if dragOrigin == nil { dragOrigin = level }
                        let base = dragOrigin ?? level
                        level = max(0, min(100, base - drag.translation.height / 2))
                    }
                    .onEnded { _ in
                        dragOrigin = nil
                        onCommit()
                    }
            )
            .accessibilityLabel(title)
            .accessibilityValue("\(Int(level)) su cento")

            Text(title)
                .font(.caption.weight(.medium))
                .foregroundStyle(.secondary)
        }
        .frame(maxWidth: .infinity)
    }
}

// MARK: - Master volume hero

struct IGIMasterVolumeHero: View {
    @Binding var leftDb: Double
    @Binding var rightDb: Double
    var onCommit: () -> Void

    private var linkedDb: Binding<Double> {
        Binding(
            get: { (leftDb + rightDb) / 2 },
            set: { newValue in
                leftDb = newValue
                rightDb = newValue
            }
        )
    }

    var body: some View {
        VStack(spacing: IGITheme.spacingM) {
            HStack {
                VStack(alignment: .leading, spacing: 4) {
                    Text("Volume master")
                        .font(.subheadline.weight(.medium))
                        .foregroundStyle(.secondary)
                    Text("\(Int(linkedDb.wrappedValue))")
                        .font(.system(size: 48, weight: .bold, design: .rounded))
                        .monospacedDigit()
                        .contentTransition(.numericText())
                }
                Spacer()
                Image(systemName: "speaker.wave.3.fill")
                    .font(.largeTitle)
                    .foregroundStyle(IGITheme.accent.gradient)
                    .symbolEffect(.variableColor.iterative, options: .repeating, value: linkedDb.wrappedValue)
            }

            Slider(value: linkedDb, in: 0 ... 100, step: 1) { editing in
                if !editing { onCommit() }
            }
            .tint(IGITheme.accent)

            HStack(spacing: IGITheme.spacingL) {
                stereoBalance(label: "L", value: $leftDb)
                stereoBalance(label: "R", value: $rightDb)
            }
        }
        .igiAudioCard()
    }

    @ViewBuilder
    private func stereoBalance(label: String, value: Binding<Double>) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Text("Canale \(label)")
                .font(.caption2)
                .foregroundStyle(.secondary)
            HStack {
                Text(label).font(.caption.weight(.bold))
                Slider(value: value, in: 0 ... 100, step: 1) { editing in
                    if !editing { onCommit() }
                }
                .tint(.secondary)
            }
        }
    }
}

// MARK: - Card style

extension View {
    func igiAudioCard() -> some View {
        igiPremiumCard()
    }
}

enum AudioPanel: String, CaseIterable, Identifiable {
    case mixer = "Mixer"
    case enhance = "FX"

    var id: String { rawValue }

    var icon: String {
        switch self {
        case .mixer: "slider.vertical.3"
        case .enhance: "sparkles"
        }
    }
}
