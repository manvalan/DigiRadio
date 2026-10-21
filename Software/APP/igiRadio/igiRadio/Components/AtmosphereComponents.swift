import SwiftUI

struct IGIAtmospherePill: View {
    var title: String
    var isOn: Bool
    var action: () -> Void

    var body: some View {
        Button(action: action) {
            Text(title)
                .font(.subheadline.weight(.semibold))
                .padding(.horizontal, 14)
                .frame(minHeight: 40)
                .foregroundStyle(isOn ? Color.white : IGITheme.ink)
                .background(isOn ? IGITheme.accent : IGITheme.card, in: Capsule())
                .overlay {
                    Capsule().strokeBorder(isOn ? IGITheme.accent : IGITheme.ink.opacity(0.10), lineWidth: 1)
                }
        }
        .buttonStyle(.plain)
        .accessibilityAddTraits(isOn ? .isSelected : [])
    }
}

struct IGIAtmosphereVolume: View {
    @Binding var level: Double
    var dbLabel: String
    var onEditingEnded: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text("Volume")
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(IGITheme.ink)
                Spacer()
                Text(dbLabel)
                    .font(.subheadline.monospacedDigit())
                    .foregroundStyle(IGITheme.inkMuted)
            }
            IGIAppleVolumeSlider(level: $level, onEditingEnded: onEditingEnded)
        }
    }
}

struct IGIAtmosphereEQCurve: View {
    var bands: [EQBandState]
    var height: CGFloat = 128

    var body: some View {
        let sorted = bands.sorted { $0.centerHz < $1.centerHz }
        VStack(spacing: 8) {
            GeometryReader { geo in
                ZStack {
                    Path { path in
                        appendCurve(to: &path, in: geo.size, bands: sorted, close: true)
                    }
                    .fill(IGITheme.accent.opacity(0.16))
                    Path { path in
                        appendCurve(to: &path, in: geo.size, bands: sorted, close: false)
                    }
                    .stroke(IGITheme.accent, style: StrokeStyle(lineWidth: 2.4, lineCap: .round, lineJoin: .round))
                    Path { path in
                        path.move(to: CGPoint(x: 0, y: geo.size.height / 2))
                        path.addLine(to: CGPoint(x: geo.size.width, y: geo.size.height / 2))
                    }
                    .stroke(IGITheme.ink.opacity(0.10), lineWidth: 1)
                }
            }
            .frame(height: height)
            .accessibilityHidden(true)

            HStack {
                ForEach(sorted) { band in
                    Text(freqLabel(band.centerHz))
                        .font(.caption2)
                        .foregroundStyle(IGITheme.inkMuted)
                        .frame(maxWidth: .infinity)
                }
            }
        }
        .padding(16)
        .background(IGITheme.card, in: RoundedRectangle(cornerRadius: IGITheme.cardRadius, style: .continuous))
        .overlay {
            RoundedRectangle(cornerRadius: IGITheme.cardRadius, style: .continuous)
                .strokeBorder(IGITheme.ink.opacity(0.08), lineWidth: 1)
        }
        .accessibilityLabel("Equalizzatore")
    }

    private func appendCurve(to path: inout Path, in size: CGSize, bands: [EQBandState], close: Bool) {
        guard bands.count >= 2 else { return }
        func point(_ i: Int) -> CGPoint {
            let x = size.width * CGFloat(i) / CGFloat(bands.count - 1)
            let y = size.height * (1 - CGFloat((bands[i].gainDb + 12) / 24))
            return CGPoint(x: x, y: y)
        }
        path.move(to: point(0))
        for i in 1..<bands.count {
            path.addLine(to: point(i))
        }
        if close {
            path.addLine(to: CGPoint(x: size.width, y: size.height))
            path.addLine(to: CGPoint(x: 0, y: size.height))
            path.closeSubpath()
        }
    }

    private func freqLabel(_ hz: Double) -> String {
        hz >= 1000 ? String(format: "%.0fk", hz / 1000) : String(format: "%.0f", hz)
    }
}

struct IGINowPlayingCard: View {
    var kicker: String
    var title: String
    var frequency: String
    var messageKind: String = "Messaggio"
    var message: String?

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text(kicker)
                .font(.caption.weight(.semibold))
                .foregroundStyle(IGITheme.inkMuted)
            Text(title)
                .font(.largeTitle.weight(.semibold))
                .foregroundStyle(IGITheme.ink)
                .minimumScaleFactor(0.7)
                .lineLimit(2)
            Text(frequency)
                .font(.title3)
                .foregroundStyle(IGITheme.inkMuted)
            if let message, !message.isEmpty {
                VStack(alignment: .leading, spacing: 4) {
                    Text(messageKind)
                        .font(.caption.weight(.semibold))
                        .foregroundStyle(IGITheme.inkMuted)
                    Text(message)
                        .font(.subheadline.weight(.medium))
                        .foregroundStyle(IGITheme.ink)
                        .fixedSize(horizontal: false, vertical: true)
                }
                .padding(12)
                .frame(maxWidth: .infinity, alignment: .leading)
                .background(IGITheme.canvas, in: RoundedRectangle(cornerRadius: 14, style: .continuous))
                .padding(.top, 8)
                .accessibilityLabel("\(messageKind): \(message)")
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(22)
        .background(IGITheme.card, in: RoundedRectangle(cornerRadius: IGITheme.cardRadius, style: .continuous))
        .overlay {
            RoundedRectangle(cornerRadius: IGITheme.cardRadius, style: .continuous)
                .strokeBorder(IGITheme.ink.opacity(0.08), lineWidth: 1)
        }
        .accessibilityElement(children: .combine)
        .accessibilityLabel("\(kicker), \(title), \(frequency)\(message.map { ", \($0)" } ?? "")")
    }
}

struct IGIChipWrap: Layout {
    var spacing: CGFloat = 8

    func sizeThatFits(proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) -> CGSize {
        arrange(width: proposal.width, subviews: subviews).size
    }

    func placeSubviews(in bounds: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) {
        let placed = arrange(width: bounds.width, subviews: subviews)
        for (subview, origin) in zip(subviews, placed.origins) {
            subview.place(
                at: CGPoint(x: bounds.minX + origin.x, y: bounds.minY + origin.y),
                proposal: .unspecified
            )
        }
    }

    private struct Arrangement {
        var origins: [CGPoint]
        var size: CGSize
    }

    private func arrange(width: CGFloat?, subviews: Subviews) -> Arrangement {
        let limit = width ?? .infinity
        var origins: [CGPoint] = []
        var x: CGFloat = 0
        var y: CGFloat = 0
        var rowHeight: CGFloat = 0
        var usedWidth: CGFloat = 0
        for subview in subviews {
            let size = subview.sizeThatFits(.unspecified)
            if x > 0, x + size.width > limit {
                x = 0
                y += rowHeight + spacing
                rowHeight = 0
            }
            origins.append(CGPoint(x: x, y: y))
            rowHeight = max(rowHeight, size.height)
            x += size.width + spacing
            usedWidth = max(usedWidth, x - spacing)
        }
        return Arrangement(
            origins: origins,
            size: CGSize(width: usedWidth, height: y + rowHeight)
        )
    }
}

struct IGIFavoriteChip: View {
    var title: String
    var isCurrent: Bool
    var action: () -> Void

    var body: some View {
        Button(action: action) {
            Text(title)
                .font(.subheadline.weight(.semibold))
                .lineLimit(1)
                .padding(.horizontal, 14)
                .frame(minHeight: 40)
                .foregroundStyle(isCurrent ? Color.white : IGITheme.ink)
                .background(isCurrent ? IGITheme.accent : IGITheme.card, in: Capsule())
                .overlay {
                    Capsule().strokeBorder(isCurrent ? IGITheme.accent : IGITheme.ink.opacity(0.10), lineWidth: 1)
                }
        }
        .buttonStyle(.plain)
        .fixedSize(horizontal: true, vertical: true)
    }
}

struct IGIStationCard: View {
    var kicker: String
    var title: String
    var footnote: String
    var isCurrent: Bool
    var action: () -> Void

    var body: some View {
        Button(action: action) {
            VStack(alignment: .leading, spacing: 6) {
                Text(kicker)
                    .font(.caption)
                    .foregroundStyle(isCurrent ? Color.white.opacity(0.8) : IGITheme.inkMuted)
                Text(title)
                    .font(.headline)
                    .foregroundStyle(isCurrent ? Color.white : IGITheme.ink)
                    .lineLimit(2)
                    .multilineTextAlignment(.leading)
                Text(footnote)
                    .font(.caption)
                    .foregroundStyle(isCurrent ? Color.white.opacity(0.8) : IGITheme.inkMuted)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(16)
            .background(isCurrent ? IGITheme.accent : IGITheme.card, in: RoundedRectangle(cornerRadius: IGITheme.cardRadius, style: .continuous))
            .overlay {
                RoundedRectangle(cornerRadius: IGITheme.cardRadius, style: .continuous)
                    .strokeBorder(isCurrent ? IGITheme.accent : IGITheme.ink.opacity(0.08), lineWidth: 1)
            }
        }
        .buttonStyle(.plain)
        .accessibilityAddTraits(isCurrent ? .isSelected : [])
    }
}

struct IGIAtmosphereInputMeters: View {
    var levels: AudioLevelsDTO?
    var isAvailable: Bool

    var body: some View {
        HStack(spacing: 12) {
            meter(title: "Radio", left: levels?.radioInLeftDb, right: levels?.radioInRightDb)
            meter(title: "Stream", left: levels?.bluetoothInLeftDb, right: levels?.bluetoothInRightDb)
        }
    }

    private func meter(title: String, left: Double?, right: Double?) -> some View {
        let db: Double? = {
            guard isAvailable, let left, let right else { return nil }
            return (left + right) / 2
        }()
        let quiet = (db ?? -96) < -40
        return VStack(alignment: .leading, spacing: 8) {
            Text(title)
                .font(.caption.weight(.semibold))
                .foregroundStyle(IGITheme.ink)
            IGIPathVUMeter(leftDb: left, rightDb: right, isAvailable: isAvailable, height: 56)
            Text(quiet ? "silenzio" : (db.map { VolumeMapping.formattedDb($0) } ?? "—"))
                .font(.caption2.monospacedDigit())
                .foregroundStyle(IGITheme.inkMuted)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(14)
        .background(IGITheme.card, in: RoundedRectangle(cornerRadius: IGITheme.cardRadius, style: .continuous))
        .overlay {
            RoundedRectangle(cornerRadius: IGITheme.cardRadius, style: .continuous)
                .strokeBorder(IGITheme.ink.opacity(0.08), lineWidth: 1)
        }
    }
}

struct IGIOutputDestinationCard: View {
    var title: String
    var subtitle: String
    var isLinked: Bool
    var levels: AudioLevelsDTO?
    var levelsAvailable: Bool
    var action: () -> Void

    var body: some View {
        Button(action: action) {
            HStack(alignment: .center, spacing: 16) {
                Image(systemName: "hifispeaker.fill")
                    .font(.title2)
                    .foregroundStyle(isLinked ? Color.white : IGITheme.ink)
                    .frame(width: 56, height: 56)
                    .background(isLinked ? IGITheme.accent : IGITheme.canvas, in: RoundedRectangle(cornerRadius: 16, style: .continuous))

                VStack(alignment: .leading, spacing: 4) {
                    Text("Uscita")
                        .font(.caption.weight(.semibold))
                        .foregroundStyle(IGITheme.inkMuted)
                    Text(title)
                        .font(.headline)
                        .foregroundStyle(IGITheme.ink)
                        .lineLimit(2)
                        .multilineTextAlignment(.leading)
                    Text(subtitle)
                        .font(.caption)
                        .foregroundStyle(IGITheme.inkMuted)
                    Text(outputCaption)
                        .font(.caption2.monospacedDigit())
                        .foregroundStyle(IGITheme.inkMuted)
                }
                .frame(maxWidth: .infinity, alignment: .leading)

                IGIPathVUMeter(
                    leftDb: levels?.outputLeftDb,
                    rightDb: levels?.outputRightDb,
                    isAvailable: levelsAvailable,
                    height: 56
                )
            }
            .padding(16)
            .background(IGITheme.card, in: RoundedRectangle(cornerRadius: IGITheme.cardRadius, style: .continuous))
            .overlay {
                RoundedRectangle(cornerRadius: IGITheme.cardRadius, style: .continuous)
                    .strokeBorder(isLinked ? IGITheme.accent.opacity(0.35) : IGITheme.ink.opacity(0.08), lineWidth: 1)
            }
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Uscita \(title), \(subtitle)")
        .accessibilityHint("Tocco per cambiare altoparlante")
    }

    private var outputCaption: String {
        guard levelsAvailable, let levels else { return "—" }
        return VolumeMapping.formattedDb((levels.outputLeftDb + levels.outputRightDb) / 2)
    }
}
