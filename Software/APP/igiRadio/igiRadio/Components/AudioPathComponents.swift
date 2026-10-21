import SwiftUI

extension VerticalAlignment {
    private enum PathIconCenter: AlignmentID {
        static func defaultValue(in context: ViewDimensions) -> CGFloat {
            context[VerticalAlignment.center]
        }
    }

    /// Centro del riquadro icona (72 pt), così la catena resta in linea.
    static let pathIconCenter = VerticalAlignment(PathIconCenter.self)
}

struct IGIPathNode: View {
    var title: String
    var subtitle: String?
    var systemImage: String
    var isActive: Bool
    var onTap: () -> Void
    var onDoubleTap: (() -> Void)?
    var onLongPress: (() -> Void)?

    var body: some View {
        VStack(spacing: 8) {
            Image(systemName: systemImage)
                .font(.title2)
                .symbolVariant(isActive ? .fill : .none)
                .foregroundStyle(isActive ? Color.white : IGITheme.ink)
                .frame(width: 72, height: 72)
                .background(nodeBackground, in: RoundedRectangle(cornerRadius: 18, style: .continuous))
                .overlay {
                    RoundedRectangle(cornerRadius: 18, style: .continuous)
                        .strokeBorder(isActive ? IGITheme.accent : IGITheme.ink.opacity(0.10), lineWidth: 1.5)
                }
                .shadow(color: isActive ? IGITheme.accent.opacity(0.28) : .clear, radius: 10, y: 4)

            VStack(spacing: 2) {
                Text(title)
                    .font(.caption.weight(.semibold))
                    .foregroundStyle(IGITheme.ink)
                    .lineLimit(2)
                    .minimumScaleFactor(0.85)
                if let subtitle, !subtitle.isEmpty {
                    Text(subtitle)
                        .font(.caption2)
                        .foregroundStyle(IGITheme.inkMuted)
                        .lineLimit(1)
                }
            }
            .frame(width: 96)
            .multilineTextAlignment(.center)
        }
        .contentShape(Rectangle())
        .modifier(PathNodeGestures(
            onTap: onTap,
            onDoubleTap: onDoubleTap,
            onLongPress: onLongPress
        ))
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(title)
        .accessibilityValue(subtitle ?? (isActive ? "attivo" : "inattivo"))
        .accessibilityHint(accessibilityHintText)
        .accessibilityAddTraits(.isButton)
        .accessibilityAction(named: "Regola livello") {
            onLongPress?()
        }
        .alignmentGuide(.pathIconCenter) { _ in 36 }
    }

    private var accessibilityHintText: String {
        if onLongPress != nil {
            return "Tocco per attivare, pressione prolungata per il livello"
        }
        if onDoubleTap != nil {
            return "Tocco per selezionare, doppio tocco per i parametri"
        }
        return "Tocco per selezionare"
    }

    private var nodeBackground: Color {
        isActive ? IGITheme.accent : IGITheme.card
    }
}

private struct PathNodeGestures: ViewModifier {
    var onTap: () -> Void
    var onDoubleTap: (() -> Void)?
    var onLongPress: (() -> Void)?

    func body(content: Content) -> some View {
        if let onDoubleTap, let onLongPress {
            content
                .onTapGesture(count: 2, perform: onDoubleTap)
                .onTapGesture(count: 1, perform: onTap)
                .onLongPressGesture(minimumDuration: 0.45, perform: onLongPress)
        } else if let onDoubleTap {
            content
                .onTapGesture(count: 2, perform: onDoubleTap)
                .onTapGesture(count: 1, perform: onTap)
        } else if let onLongPress {
            content
                .onTapGesture(perform: onTap)
                .onLongPressGesture(minimumDuration: 0.45, perform: onLongPress)
        } else {
            content.onTapGesture(perform: onTap)
        }
    }
}

/// Settore sinistro ~120°, centrato sull’orizzontale (DAB a sinistra).
struct IGISourceArc<Top: View, Mid: View, Bottom: View>: View {
    var radius: CGFloat = 128
    var top: Top
    var mid: Mid
    var bottom: Bottom

    init(
        radius: CGFloat = 128,
        @ViewBuilder top: () -> Top,
        @ViewBuilder mid: () -> Mid,
        @ViewBuilder bottom: () -> Bottom
    ) {
        self.radius = radius
        self.top = top()
        self.mid = mid()
        self.bottom = bottom()
    }

    private let nodeWidth: CGFloat = 96
    private let verticalPad: CGFloat = 58

    var body: some View {
        let angle = Double.pi / 3
        let cosA = CGFloat(cos(angle))
        let sinA = CGFloat(sin(angle))
        let width = nodeWidth + radius * (1 - cosA)
        let height = 2 * radius * sinA + verticalPad * 2
        let cx = nodeWidth / 2 + radius
        let cy = height / 2

        ZStack {
            top.position(x: cx - radius * cosA, y: cy - radius * sinA)
            mid.position(x: cx - radius, y: cy)
            bottom.position(x: cx - radius * cosA, y: cy + radius * sinA)
        }
        .frame(width: width, height: height)
        .alignmentGuide(.pathIconCenter) { dimensions in
            dimensions[VerticalAlignment.center]
        }
        .accessibilityElement(children: .contain)
    }
}

/// Fader verticale 0–100, stesso linguaggio visivo dei VU di uscita.
struct IGIPathFader: View {
    @Binding var value: Double
    var label: String
    var height: CGFloat = 120
    var trackWidth: CGFloat = 18
    var fill: LinearGradient = LinearGradient(
        colors: [Color.accentColor.opacity(0.55), Color.accentColor],
        startPoint: .bottom,
        endPoint: .top
    )
    var onEditingEnded: (() -> Void)? = nil

    var body: some View {
        VStack(spacing: 6) {
            Text("\(Int(value.rounded()))")
                .font(.caption.monospacedDigit().weight(.semibold))
                .foregroundStyle(.secondary)
                .frame(minWidth: 28)

            GeometryReader { geo in
                let h = geo.size.height
                let clamped = max(0, min(100, value))
                let fillHeight = h * clamped / 100

                ZStack(alignment: .bottom) {
                    Capsule()
                        .fill(Color.primary.opacity(0.08))
                    Capsule()
                        .fill(fill)
                        .frame(height: max(trackWidth * 0.6, fillHeight))

                    Circle()
                        .fill(Color(.systemBackground))
                        .shadow(color: .black.opacity(0.18), radius: 2, y: 1)
                        .overlay {
                            Circle().strokeBorder(Color.accentColor.opacity(0.55), lineWidth: 1.5)
                        }
                        .frame(width: trackWidth + 10, height: trackWidth + 10)
                        .offset(y: -(fillHeight - (trackWidth + 10) / 2))
                        .accessibilityHidden(true)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                .contentShape(Rectangle())
                .gesture(
                    DragGesture(minimumDistance: 0)
                        .onChanged { drag in
                            let ratio = 1 - (drag.location.y / h)
                            value = max(0, min(100, (ratio * 100).rounded()))
                        }
                        .onEnded { _ in
                            onEditingEnded?()
                        }
                )
            }
            .frame(width: trackWidth + 18, height: height)

            Text(label)
                .font(.caption.weight(.semibold))
        }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(label)
        .accessibilityValue("\(Int(value.rounded()))")
        .accessibilityAdjustableAction { direction in
            switch direction {
            case .increment: value = min(100, value + 5)
            case .decrement: value = max(0, value - 5)
            default: break
            }
            onEditingEnded?()
        }
    }
}

/// Barre L/R da GET /api/audio/levels (dBFS). Se la lettura fallisce, meter non disponibili.
struct IGIPathVUMeter: View {
    var leftDb: Double?
    var rightDb: Double?
    var isAvailable: Bool
    var height: CGFloat = 120

    private let segmentCount = 12

    var body: some View {
        HStack(alignment: .bottom, spacing: 8) {
            meter(label: "L", dbfs: leftDb)
            meter(label: "R", dbfs: rightDb)
        }
        .accessibilityLabel("Livello uscita stereo")
        .accessibilityValue(isAvailable ? accessibilityValueText : "non disponibile")
        .alignmentGuide(.pathIconCenter) { _ in height / 2 }
    }

    private var accessibilityValueText: String {
        let left = leftDb.map { String(format: "%.1f", $0) } ?? "—"
        let right = rightDb.map { String(format: "%.1f", $0) } ?? "—"
        return "sinistra \(left) dB, destra \(right) dB"
    }

    private func meter(label: String, dbfs: Double?) -> some View {
        let fill = (isAvailable && dbfs != nil)
            ? AudioLevelsDTO.meterFill(dbfs: dbfs!)
            : 0
        let lit = isAvailable ? min(segmentCount, max(0, Int((fill * Double(segmentCount)).rounded()))) : 0

        return VStack(spacing: 4) {
            VStack(spacing: 2) {
                ForEach(Array((0..<segmentCount).reversed()), id: \.self) { index in
                    RoundedRectangle(cornerRadius: 1.5, style: .continuous)
                        .fill(segmentColor(index: index, lit: lit))
                        .frame(width: 12, height: segmentHeight)
                }
            }
            .frame(height: height)
            .animation(.easeOut(duration: 0.12), value: lit)

            Text(label)
                .font(.caption2.weight(.semibold))
                .foregroundStyle(IGITheme.inkMuted)
        }
    }

    private var segmentHeight: CGFloat {
        let gaps = CGFloat(segmentCount - 1) * 2
        return max(3, (height - gaps) / CGFloat(segmentCount))
    }

    private func segmentColor(index: Int, lit: Int) -> Color {
        guard index < lit else {
            return IGITheme.ink.opacity(0.10)
        }
        let t = Double(index) / Double(max(1, segmentCount - 1))
        if t > 0.84 {
            return Color(red: 0.72, green: 0.28, blue: 0.28)
        }
        if t > 0.62 {
            return Color(red: 0.78, green: 0.55, blue: 0.18)
        }
        return IGITheme.accent
    }
}
