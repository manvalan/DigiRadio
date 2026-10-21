import SwiftUI

struct AudioProfileEditorView: View {
    @Environment(AppEnvironment.self) private var environment
    @Environment(\.dismiss) private var dismiss

    @State private var viewModel: AudioProfileEditorViewModel?
    let profile: AudioProfileTemplate
    let isNewProfile: Bool
    let onSave: (AudioProfileTemplate) -> Void

    private var errorBinding: Binding<Bool> {
        Binding(
            get: { viewModel?.errorMessage != nil },
            set: { if !$0 { viewModel?.errorMessage = nil } }
        )
    }

    var body: some View {
        let vm = viewModel ?? AudioProfileEditorViewModel(
            profile: profile,
            service: environment.digiRadio,
            isNewProfile: isNewProfile,
            onSave: onSave
        )

        Form {
            if !vm.profile.isBuiltIn {
                Section("Nome") {
                    TextField("Nome profilo", text: Binding(
                        get: { vm.profile.name },
                        set: { vm.profile.name = $0 }
                    ))
                }
            }

            Section("Equalizzatore") {
                ForEach(Array(vm.profile.eq.enumerated()), id: \.offset) { index, band in
                    VStack(alignment: .leading, spacing: 6) {
                        HStack {
                            Text(freqLabel(band.centerHz))
                            Spacer()
                            Text(gainLabel(band.gainDb))
                                .foregroundStyle(.secondary)
                                .monospacedDigit()
                        }
                        Slider(
                            value: Binding(
                                get: { vm.profile.eq[index].gainDb },
                                set: { vm.profile.eq[index].gainDb = $0 }
                            ),
                            in: -12 ... 12,
                            step: 0.5
                        ) { editing in
                            if !editing { vm.schedulePreviewApply() }
                        }
                    }
                    .padding(.vertical, 2)
                }
            }

            Section("Miglioramento suono") {
                VStack(alignment: .leading, spacing: 8) {
                    HStack {
                        Text("Bass Boost")
                        Spacer()
                        Text("\(vm.profile.enhancements.bassLevel)")
                            .foregroundStyle(.secondary)
                            .monospacedDigit()
                    }
                    Slider(
                        value: Binding(
                            get: { Double(vm.profile.enhancements.bassLevel) },
                            set: { vm.profile.enhancements.bassLevel = Int($0) }
                        ),
                        in: 0 ... 100,
                        step: 1
                    ) { editing in
                        if !editing { vm.schedulePreviewApply() }
                    }
                }
                VStack(alignment: .leading, spacing: 8) {
                    HStack {
                        Text("SuperPhat")
                        Spacer()
                        Text("\(vm.profile.enhancements.stereoLevel)")
                            .foregroundStyle(.secondary)
                            .monospacedDigit()
                    }
                    Slider(
                        value: Binding(
                            get: { Double(vm.profile.enhancements.stereoLevel) },
                            set: { vm.profile.enhancements.stereoLevel = Int($0) }
                        ),
                        in: 0 ... 100,
                        step: 1
                    ) { editing in
                        if !editing { vm.schedulePreviewApply() }
                    }
                }
                Toggle("Voice Clarifier", isOn: Binding(
                    get: { vm.profile.enhancements.voiceClean },
                    set: { vm.profile.enhancements.voiceClean = $0 }
                ))
            }

            Section {
                if !vm.profile.isBuiltIn {
                    Button {
                        vm.saveUserProfile()
                        Task { await vm.applyToDevice(persistUserCopy: true) }
                        dismiss()
                    } label: {
                        Text("Salva profilo")
                    }
                }
                Button {
                    Task { await vm.applyToDevice(persistUserCopy: false) }
                } label: {
                    Text("Applica a DigiRadio")
                }
                if vm.profile.isBuiltIn {
                    Button {
                        let duplicate = vm.profile.duplicatedAsUser(named: "\(vm.profile.name) custom")
                        onSave(duplicate)
                        dismiss()
                    } label: {
                        Text("Salva come profilo personale")
                    }
                }
            }
        }
        .navigationTitle(vm.profile.isBuiltIn ? vm.profile.name : "Modifica profilo")
        .navigationBarTitleDisplayMode(.inline)
        .alert("Profilo audio", isPresented: errorBinding) {
            Button("OK", role: .cancel) { viewModel?.errorMessage = nil }
        } message: {
            Text(viewModel?.errorMessage ?? "")
        }
        .onAppear {
            if viewModel == nil { viewModel = vm }
        }
    }

    private func freqLabel(_ hz: Double) -> String {
        hz >= 1000 ? String(format: "%.0f kHz", hz / 1000) : String(format: "%.0f Hz", hz)
    }

    private func gainLabel(_ db: Double) -> String {
        db >= 0 ? String(format: "+%.1f dB", db) : String(format: "%.1f dB", db)
    }
}
