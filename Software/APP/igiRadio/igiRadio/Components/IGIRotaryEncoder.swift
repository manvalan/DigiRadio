import SwiftUI

/// Encoder rotativo da pannello: manopola zigrinata vista dall’alto.
/// Una rotazione completa ≈ 8 MHz, scatti da 0.05 MHz.
struct IGIRotaryEncoder: View {
    @Binding var value: Double
    var range: ClosedRange<Double> = 87.5 ... 108.0
    var step: Double = 0.05
    /// Scatti per giro completo (160 × 0.05 MHz = 8 MHz/giro).
    var detentsPerTurn: Double = 160
    var onCommit: () -> Void

    @State private var knobAngle: Double = 0
    @State private var lastTouchAngle: Double?
    @State private var detentToken = 0

    var body: some View {
        GeometryReader { geo in
            let size = min(geo.size.width, geo.size.height)

            ZStack {
                Circle()
                    .fill(
                        LinearGradient(
                            colors: [
                                Color(red: 0.82, green: 0.81, blue: 0.78),
                                Color(red: 0.62, green: 0.61, blue: 0.58)
                            ],
                            startPoint: .top,
                            endPoint: .bottom
                        )
                    )
                    .shadow(color: IGITheme.ink.opacity(0.22), radius: 10, y: 5)

                Circle()
                    .strokeBorder(Color.white.opacity(0.35), lineWidth: 1)
                    .padding(1)

                knurlRing(size: size)

                Circle()
                    .fill(
                        RadialGradient(
                            colors: [
                                Color(red: 0.97, green: 0.96, blue: 0.94),
                                Color(red: 0.86, green: 0.85, blue: 0.82),
                                Color(red: 0.70, green: 0.69, blue: 0.66)
                            ],
                            center: UnitPoint(x: 0.36, y: 0.30),
                            startRadius: 2,
                            endRadius: size * 0.42
                        )
                    )
                    .padding(size * 0.13)
                    .rotationEffect(.degrees(knobAngle))

                Circle()
                    .strokeBorder(IGITheme.ink.opacity(0.12), lineWidth: 1)
                    .padding(size * 0.13)

                Circle()
                    .fill(IGITheme.ink.opacity(0.85))
                    .frame(width: size * 0.048, height: size * 0.048)
                    .offset(y: -size * 0.27)
                    .rotationEffect(.degrees(knobAngle))

                Capsule()
                    .fill(IGITheme.ink.opacity(0.45))
                    .frame(width: 3, height: size * 0.045)
                    .offset(y: -(size / 2 - 7))
            }
            .frame(width: size, height: size)
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .contentShape(Circle())
            .gesture(
                DragGesture(minimumDistance: 0)
                    .onChanged { drag in
                        let center = CGPoint(x: size / 2, y: size / 2)
                        let angle = atan2(drag.location.y - center.y, drag.location.x - center.x)
                        if let lastTouchAngle {
                            var delta = angle - lastTouchAngle
                            while delta > .pi { delta -= 2 * .pi }
                            while delta < -.pi { delta += 2 * .pi }
                            knobAngle += delta * 180 / .pi
                            let steps = delta / (2 * .pi) * detentsPerTurn
                            let next = snap(value + steps * step)
                            if next != value {
                                value = next
                                detentToken += 1
                            }
                        }
                        lastTouchAngle = angle
                    }
                    .onEnded { _ in
                        lastTouchAngle = nil
                        value = snap(value)
                        onCommit()
                    }
            )
        }
        .aspectRatio(1, contentMode: .fit)
        .sensoryFeedback(.selection, trigger: detentToken)
        .accessibilityLabel("Encoder frequenza FM")
        .accessibilityValue(String(format: NSLocalizedString("%.2f megahertz", comment: ""), value))
        .accessibilityAdjustableAction { direction in
            switch direction {
            case .increment: value = snap(min(range.upperBound, value + step))
            case .decrement: value = snap(max(range.lowerBound, value - step))
            default: break
            }
            onCommit()
        }
    }

    private func knurlRing(size: CGFloat) -> some View {
        ForEach(0..<80, id: \.self) { i in
            RoundedRectangle(cornerRadius: 0.6)
                .fill(knurlColor(i))
                .frame(width: i.isMultiple(of: 2) ? 2.4 : 1.6, height: size * 0.078)
                .offset(y: -(size / 2 - size * 0.055))
                .rotationEffect(.degrees(Double(i) * 4.5 + knobAngle))
        }
    }

    private func knurlColor(_ index: Int) -> Color {
        index.isMultiple(of: 2)
            ? IGITheme.ink.opacity(0.38)
            : Color.white.opacity(0.55)
    }

    private func snap(_ raw: Double) -> Double {
        let stepped = (raw / step).rounded() * step
        return min(range.upperBound, max(range.lowerBound, (stepped * 100).rounded() / 100))
    }
}
