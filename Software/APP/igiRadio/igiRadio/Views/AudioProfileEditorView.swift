import SwiftUI

struct AudioProfileEditorView: View {
    @Environment(AppEnvironment.self) private var environment
    @Environment(\.dismiss) private var dismiss

    @State private var viewModel: AudioProfileEditorViewModel?
    let profile: AudioProfileTemplate
    let isNewProfile: Bool
    let onSave: (AudioProfileTemplate) -> Void

    var body: some View {
        let vm = viewModel ?? AudioProfileEditorViewModel(
            profile: profile,
            service: environment.digiRadio,
            isNewProfile: isNewProfile,
            onSave: onSave
        )

        ScrollView {
            VStack(alignment: .leading, spacing: IGITheme.spacingL) {
                if !vm.profile.isBuiltIn {
                    VStack(alignment: .leading, spacing: IGITheme.spacingS) {
                        Text("Nome profilo")
                            .font(.headline)
                        TextField("Nome", text: Binding(
                            get: { vm.profile.name },
                            set: { vm.profile.name = $0 }
                        ))
                        .padding()
                        .background(Color.primary.opacity(0.05), in: RoundedRectangle(cornerRadius: 12))
                    }
                    .igiPremiumCard()
                }

                VStack(alignment: .leading, spacing: IGITheme.spacingM) {
                    Text("Equalizzatore")
                        .font(.title3.weight(.bold))
                    IGIGraphicEqualizer(
                        bands: Binding(
                            get: { vm.profile.eq },
                            set: { vm.profile.eq = $0 }
                        ),
                        onCommit: { vm.schedulePreviewApply() }
                    )
                }

                VStack(alignment: .leading, spacing: IGITheme.spacingM) {
                    Text("Enhancements")
                        .font(.headline)
                    HStack(spacing: IGITheme.spacingL) {
                        IGIEnhancementDial(
                            title: "Stereo",
                            systemImage: "circle.lefthalf.filled",
                            level: Binding(
                                get: { Double(vm.profile.enhancements.stereoLevel) },
                                set: { vm.profile.enhancements.stereoLevel = Int($0) }
                            ),
                            onCommit: { vm.schedulePreviewApply() }
                        )
                        IGIEnhancementDial(
                            title: "Bass",
                            systemImage: "waveform.path",
                            level: Binding(
                                get: { Double(vm.profile.enhancements.bassLevel) },
                                set: { vm.profile.enhancements.bassLevel = Int($0) }
                            ),
                            onCommit: { vm.schedulePreviewApply() }
                        )
                    }
                }
                .igiPremiumCard()

                if !vm.profile.isBuiltIn {
                    Button {
                        vm.saveUserProfile()
                        Task { await vm.applyToDevice(persistUserCopy: true) }
                        dismiss()
                    } label: {
                        Label("Salva profilo", systemImage: "square.and.arrow.down")
                            .frame(maxWidth: .infinity)
                            .padding(.vertical, 14)
                    }
                    .buttonStyle(.borderedProminent)
                }

                Button {
                    Task { await vm.applyToDevice(persistUserCopy: false) }
                } label: {
                    Label("Applica a DigiRadio", systemImage: "play.fill")
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 14)
                }
                .buttonStyle(.bordered)

                if vm.profile.isBuiltIn {
                    Button {
                        let duplicate = vm.profile.duplicatedAsUser(named: "\(vm.profile.name) custom")
                        onSave(duplicate)
                        dismiss()
                    } label: {
                        Label("Salva come profilo personale", systemImage: "plus.square.on.square")
                            .frame(maxWidth: .infinity)
                            .padding(.vertical, 14)
                    }
                    .buttonStyle(.bordered)
                }
            }
            .padding(IGITheme.spacingM)
        }
        .background(IGIHeroBackground())
        .navigationTitle(vm.profile.isBuiltIn ? vm.profile.name : "Modifica profilo")
        .navigationBarTitleDisplayMode(.inline)
        .onAppear {
            if viewModel == nil { viewModel = vm }
        }
    }
}
