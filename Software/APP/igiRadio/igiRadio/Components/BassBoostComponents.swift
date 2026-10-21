import SwiftUI

/// Bass boost compatto stile iOS (toggle + slider nativo).
struct IGIBassBoostCard: View {
    @Binding var isEnabled: Bool
    @Binding var level: Double
    var isDisabled = false
    var onToggle: (Bool) -> Void
    var onLevelCommit: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Toggle("Bass Boost", isOn: $isEnabled)
                .disabled(isDisabled)
                .onChange(of: isEnabled) { _, enabled in
                    onToggle(enabled)
                }

            if isEnabled {
                HStack {
                    Image(systemName: "waveform.path")
                        .foregroundStyle(.secondary)
                        .frame(width: 22)
                    Slider(value: $level, in: 0 ... 100, step: 1) { editing in
                        if !editing { onLevelCommit() }
                    }
                    Text("\(Int(level.rounded()))")
                        .font(.body.monospacedDigit())
                        .foregroundStyle(.secondary)
                        .frame(width: 32, alignment: .trailing)
                }
                .transition(.opacity)
            }
        }
        .padding(16)
        .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 16, style: .continuous))
        .animation(.snappy, value: isEnabled)
    }
}
