import SwiftUI

/// Marchio igiRadio: stessa radio stilizzata dell'icona app.
struct IGIRadioLogo: View {
    var showWordmark: Bool = true
    var markSize: CGFloat = 72

    var body: some View {
        VStack(spacing: 10) {
            IGIRadioMark()
                .frame(width: markSize, height: markSize)
            if showWordmark {
                Text("igiRadio")
                    .font(.system(.title2, design: .rounded).weight(.semibold))
                    .foregroundStyle(IGITheme.ink)
                    .tracking(0.4)
            }
        }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel("igiRadio")
    }
}

struct IGIRadioMark: View {
    var body: some View {
        GeometryReader { geo in
            let side = min(geo.size.width, geo.size.height)
            Image("RadioMark")
                .resizable()
                .interpolation(.high)
                .scaledToFit()
                .frame(width: side, height: side)
                .clipShape(RoundedRectangle(cornerRadius: side * 0.223, style: .continuous))
                .shadow(color: Color.accentColor.opacity(0.28), radius: 10, y: 4)
        }
        .aspectRatio(1, contentMode: .fit)
    }
}
