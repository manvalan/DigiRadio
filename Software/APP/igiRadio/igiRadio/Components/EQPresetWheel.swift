import SwiftUI

/// Preset EQ disposti su una circonferenza; al centro la freccia apre le barre.
struct EQPresetWheel: View {
    var presets: [AudioProfileTemplate]
    var activeID: String?
    var onSelect: (AudioProfileTemplate) -> Void
    @Binding var showBars: Bool
    @Binding var bands: [EQBandState]
    var onBandEditing: (() -> Void)? = nil
    var onBandCommit: () -> Void

    var body: some View {
        GeometryReader { geo in
            let side = min(geo.size.width, geo.size.height)
            let cx = geo.size.width / 2
            let cy = geo.size.height / 2
            let icon: CGFloat = 54
            let labelBand: CGFloat = 26
            let radius = max(88, side / 2 - icon / 2 - labelBand)

            ZStack {
                ForEach(Array(presets.enumerated()), id: \.element.id) { index, preset in
                    let angleDeg = -90.0 + 360.0 * Double(index) / Double(max(1, presets.count))
                    let angleRad = angleDeg * .pi / 180
                    let ix = cx + radius * CGFloat(cos(angleRad))
                    let iy = cy + radius * CGFloat(sin(angleRad))
                    let labelR = radius + icon / 2 + 14
                    let lx = cx + labelR * CGFloat(cos(angleRad))
                    let ly = cy + labelR * CGFloat(sin(angleRad))
                    let isActive = preset.id == activeID

                    Button {
                        onSelect(preset)
                    } label: {
                        Image(systemName: preset.systemImage)
                            .font(.body.weight(.semibold))
                            .symbolVariant(isActive ? .fill : .none)
                            .foregroundStyle(isActive ? Color.white : IGITheme.ink)
                            .frame(width: icon, height: icon)
                            .background(
                                isActive ? IGITheme.accent : IGITheme.card,
                                in: RoundedRectangle(cornerRadius: 16, style: .continuous)
                            )
                            .overlay {
                                RoundedRectangle(cornerRadius: 16, style: .continuous)
                                    .strokeBorder(
                                        isActive ? IGITheme.accent : IGITheme.ink.opacity(0.10),
                                        lineWidth: 1.5
                                    )
                            }
                            .shadow(color: isActive ? IGITheme.accent.opacity(0.28) : .clear, radius: 8, y: 3)
                    }
                    .buttonStyle(.plain)
                    .position(x: ix, y: iy)
                    .accessibilityLabel(preset.name)
                    .accessibilityAddTraits(isActive ? [.isSelected] : [])

                    Text(preset.name)
                        .font(.caption2.weight(.semibold))
                        .foregroundStyle(IGITheme.ink)
                        .lineLimit(1)
                        .fixedSize()
                        .rotationEffect(.degrees(angleDeg + 90))
                        .position(x: lx, y: ly)
                        .allowsHitTesting(false)
                        .accessibilityHidden(true)
                }

                centerControl
                    .position(x: cx, y: cy)
            }
        }
    }

    @ViewBuilder
    private var centerControl: some View {
        VStack(spacing: 10) {
            Button {
                withAnimation(.snappy(duration: 0.28)) {
                    showBars.toggle()
                }
            } label: {
                Image(systemName: "chevron.down")
                    .font(.title2.weight(.semibold))
                    .foregroundStyle(IGITheme.ink)
                    .rotationEffect(.degrees(showBars ? 180 : 0))
                    .frame(width: 44, height: 44)
                    .background(IGITheme.card, in: Circle())
                    .overlay {
                        Circle().strokeBorder(IGITheme.ink.opacity(0.12), lineWidth: 1)
                    }
            }
            .buttonStyle(.plain)
            .accessibilityLabel(showBars ? "Nascondi bande EQ" : "Mostra bande EQ")

            if showBars {
                HStack(alignment: .bottom, spacing: 6) {
                    ForEach($bands) { $band in
                        IGIEQBandBar(
                            centerHz: band.centerHz,
                            gainDb: $band.gainDb,
                            isLocked: band.isLocked,
                            maxHeight: 108,
                            range: -12 ... 12,
                            onEditing: onBandEditing,
                            onCommit: onBandCommit
                        )
                    }
                }
                .frame(width: 210)
                .transition(.opacity.combined(with: .scale(scale: 0.92)))
            }
        }
    }
}
