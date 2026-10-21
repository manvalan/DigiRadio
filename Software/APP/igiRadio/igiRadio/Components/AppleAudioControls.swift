import SwiftUI

// MARK: - Volume (Musica / Control Center)

struct IGIAppleVolumeSlider: View {
    @Binding var level: Double
    var onEditingEnded: () -> Void

    var body: some View {
        HStack(spacing: 12) {
            Image(systemName: "speaker.fill")
                .font(.footnote)
                .foregroundStyle(.secondary)
                .frame(width: 18)
                .accessibilityHidden(true)

            Slider(value: $level, in: 0 ... 100, step: 1) { editing in
                if !editing { onEditingEnded() }
            }

            Image(systemName: "speaker.wave.3.fill")
                .font(.footnote)
                .foregroundStyle(.secondary)
                .frame(width: 22)
                .accessibilityHidden(true)
        }
        .accessibilityElement(children: .combine)
        .accessibilityLabel("Volume")
        .accessibilityValue("\(VolumeMapping.formattedDb(VolumeMapping.masterDb(forVolumeLevel: level)))")
    }
}

// MARK: - Sorgente

struct IGIActiveSourceSegmentedPicker: View {
    var selection: AudioActiveSource
    var onSelect: (AudioActiveSource) -> Void

    var body: some View {
        Picker("Sorgente", selection: Binding(
            get: { selection },
            set: { onSelect($0) }
        )) {
            ForEach(AudioActiveSource.userFacing) { source in
                Text(source.title).tag(source)
            }
        }
        .pickerStyle(.segmented)
        .accessibilityLabel("Sorgente audio")
    }
}

// MARK: - Now Playing (unica sezione grafica, stile Musica)

struct IGINowPlayingHero: View {
    var symbol: String
    var title: String
    var subtitle: String
    var isLive: Bool
    var sourceToken: UUID
    var artworkMaxWidth: CGFloat = 220
    var centered: Bool = false

    var body: some View {
        VStack(alignment: centered ? .center : .leading, spacing: 16) {
            artwork
            VStack(alignment: centered ? .center : .leading, spacing: 4) {
                Text(title)
                    .font(.title2.weight(.semibold))
                    .multilineTextAlignment(centered ? .center : .leading)
                    .lineLimit(2)
                    .contentTransition(.interpolate)
                Text(subtitle)
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
                    .multilineTextAlignment(centered ? .center : .leading)
                    .lineLimit(2)
            }
            .frame(maxWidth: .infinity, alignment: centered ? .center : .leading)
        }
        .animation(.snappy(duration: 0.35), value: sourceToken)
        .animation(.snappy(duration: 0.35), value: title)
    }

    private var artwork: some View {
        RoundedRectangle(cornerRadius: 12, style: .continuous)
            .fill(
                LinearGradient(
                    colors: [
                        Color.accentColor.opacity(0.92),
                        Color.accentColor.opacity(0.55),
                        Color.indigo.opacity(0.45)
                    ],
                    startPoint: .topLeading,
                    endPoint: .bottomTrailing
                )
            )
            .aspectRatio(1, contentMode: .fit)
            .frame(maxWidth: artworkMaxWidth)
            .overlay {
                Image(systemName: symbol)
                    .font(.system(size: 56, weight: .regular))
                    .foregroundStyle(.white)
                    .symbolEffect(.pulse, options: .repeating, value: isLive)
                    .contentTransition(.symbolEffect(.replace))
            }
            .shadow(color: Color.accentColor.opacity(0.28), radius: 18, y: 10)
            .frame(maxWidth: .infinity)
            .accessibilityHidden(true)
    }
}

// MARK: - EQ preview

struct IGIEQCurvePreview: View {
    var bands: [EQBandState]

    var body: some View {
        GeometryReader { geo in
            let sorted = bands.sorted { $0.centerHz < $1.centerHz }
            ZStack {
                RoundedRectangle(cornerRadius: 8, style: .continuous)
                    .fill(Color(.tertiarySystemFill))
                if sorted.count >= 2 {
                    Path { path in
                        for (i, band) in sorted.enumerated() {
                            let x = geo.size.width * CGFloat(i) / CGFloat(max(1, sorted.count - 1))
                            let y = geo.size.height * (1 - normalizedGain(band.gainDb))
                            if i == 0 { path.move(to: CGPoint(x: x, y: y)) }
                            else { path.addLine(to: CGPoint(x: x, y: y)) }
                        }
                    }
                    .stroke(Color.accentColor, style: StrokeStyle(lineWidth: 2.5, lineCap: .round, lineJoin: .round))
                }
            }
        }
        .frame(height: 56)
        .accessibilityHidden(true)
    }

    private func normalizedGain(_ db: Double) -> CGFloat {
        CGFloat((db + 12) / 24)
    }
}

// MARK: - Impostazioni: glifo colorato

struct IGISettingsIcon: View {
    var systemName: String
    var color: Color

    var body: some View {
        Image(systemName: systemName)
            .font(.footnote.weight(.semibold))
            .foregroundStyle(.white)
            .frame(width: 29, height: 29)
            .background(color, in: RoundedRectangle(cornerRadius: 7, style: .continuous))
            .accessibilityHidden(true)
    }
}
