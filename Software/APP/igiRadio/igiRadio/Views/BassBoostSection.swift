import SwiftUI

struct BassBoostSection: View {
    @Bindable var viewModel: BassBoostViewModel

    var body: some View {
        VStack(alignment: .leading, spacing: IGITheme.spacingS) {
            IGIBassBoostCard(
                isEnabled: $viewModel.isEnabled,
                level: $viewModel.level,
                isDisabled: viewModel.isBusy,
                onToggle: { enabled in
                    Task { await viewModel.setEnabled(enabled) }
                },
                onLevelCommit: {
                    Task { await viewModel.commitLevel() }
                }
            )
            if viewModel.isBusy {
                HStack(spacing: 8) {
                    ProgressView().controlSize(.small)
                    Text("Invio al dispositivo…")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
            if let error = viewModel.errorMessage {
                Text(error)
                    .font(.caption)
                    .foregroundStyle(.red)
            }
        }
    }
}
