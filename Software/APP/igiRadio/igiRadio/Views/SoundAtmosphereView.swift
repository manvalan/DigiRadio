import SwiftUI

struct SoundAtmosphereView: View {
    @Bindable var vm: AdauStudioViewModel
    @State private var showInputs = false
    @State private var showBands = false

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                if let error = vm.errorMessage {
                    Text(error)
                        .font(.subheadline)
                        .foregroundStyle(.red)
                }

                HStack(alignment: .lastTextBaseline) {
                    VStack(alignment: .leading, spacing: 4) {
                        Text("Suono")
                            .font(.largeTitle.weight(.semibold))
                            .foregroundStyle(IGITheme.ink)
                        Text(vm.activeSource == .bluetooth ? "Stream" : "Radio")
                            .font(.subheadline)
                            .foregroundStyle(IGITheme.inkMuted)
                    }
                    Spacer()
                    Text(vm.masterDbLabel)
                        .font(.title2.monospacedDigit().weight(.medium))
                        .foregroundStyle(IGITheme.ink)
                }

                IGIAtmosphereEQCurve(bands: vm.eqBands)

                HStack(spacing: 8) {
                    IGIAtmospherePill(title: "SuperPhat", isOn: vm.stereoLevel > 0) {
                        Task { await toggleSuperPhat() }
                    }
                    IGIAtmospherePill(title: "Voice", isOn: vm.voiceClean) {
                        Task { await vm.toggleVoiceClean() }
                    }
                    IGIAtmospherePill(title: "Bass Boost", isOn: vm.bassLevel > 0) {
                        Task { await toggleBass() }
                    }
                }

                if vm.stereoLevel > 0 {
                    characterSlider(title: "SuperPhat", value: $vm.stereoLevel) {
                        Task { await vm.applyEnhancements() }
                    }
                }
                if vm.bassLevel > 0 {
                    characterSlider(title: "Bass Boost", value: $vm.bassLevel) {
                        Task { await vm.applyEnhancements() }
                    }
                }

                IGIAtmosphereVolume(
                    level: $vm.masterLevel,
                    dbLabel: vm.masterDbLabel,
                    onEditingEnded: {
                        vm.pushMasterLevelToChannels()
                        vm.commitMasterVolume()
                    }
                )
                .onChange(of: vm.masterLevel) { _, _ in
                    vm.pushMasterLevelToChannels()
                }

                DisclosureGroup("Bande equalizzatore", isExpanded: $showBands) {
                    VStack(spacing: 14) {
                        ForEach($vm.eqBands) { $band in
                            eqBandRow($band)
                        }
                    }
                    .padding(.top, 8)
                }
                .font(.subheadline.weight(.semibold))
                .foregroundStyle(IGITheme.ink)
                .tint(IGITheme.accent)

                Toggle("Mostra ingressi ADAU", isOn: $showInputs)
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(IGITheme.ink)
                    .tint(IGITheme.accent)

                if showInputs {
                    IGIAtmosphereInputMeters(
                        levels: vm.outputLevels,
                        isAvailable: vm.outputLevelsAvailable
                    )
                }
            }
            .padding(.horizontal, 20)
            .padding(.vertical, 16)
            .frame(maxWidth: 720)
            .frame(maxWidth: .infinity)
        }
        .igiThemedScreen()
        .navigationTitle("Suono")
        .navigationBarTitleDisplayMode(.inline)
        .toolbar {
            if vm.isSaving {
                ToolbarItem(placement: .topBarTrailing) { ProgressView() }
            }
        }
        .onAppear {
            Task { await vm.load() }
        }
        .onChange(of: showInputs) { _, on in
            if on {
                vm.startOutputLevelsPolling()
            } else {
                vm.stopOutputLevelsPolling()
            }
        }
        .onDisappear {
            vm.stopOutputLevelsPolling()
        }
    }

    private func toggleSuperPhat() async {
        vm.notePresetEdited()
        vm.stereoLevel = vm.stereoLevel > 0 ? 0 : 42
        await vm.applyEnhancements()
    }

    private func toggleBass() async {
        vm.notePresetEdited()
        vm.bassLevel = vm.bassLevel > 0 ? 0 : 40
        await vm.applyEnhancements()
    }

    private func characterSlider(title: String, value: Binding<Double>, onEnd: @escaping () -> Void) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Text(title)
                    .font(.subheadline)
                    .foregroundStyle(IGITheme.ink)
                Spacer()
                Text("\(Int(value.wrappedValue))")
                    .font(.subheadline.monospacedDigit())
                    .foregroundStyle(IGITheme.inkMuted)
            }
            Slider(value: value, in: 0 ... 100, step: 1) { editing in
                if editing { vm.notePresetEdited() }
                if !editing { onEnd() }
            }
        }
    }

    @ViewBuilder
    private func eqBandRow(_ band: Binding<EQBandState>) -> some View {
        if band.wrappedValue.isLocked {
            HStack {
                Text(freqLabel(band.wrappedValue.centerHz))
                Spacer()
                Text("Fisso")
                    .foregroundStyle(IGITheme.inkMuted)
            }
            .font(.subheadline)
        } else {
            VStack(alignment: .leading, spacing: 6) {
                HStack {
                    Text(freqLabel(band.wrappedValue.centerHz))
                    Spacer()
                    Text(gainLabel(band.wrappedValue.gainDb))
                        .font(.subheadline.monospacedDigit())
                        .foregroundStyle(IGITheme.inkMuted)
                }
                Slider(value: band.gainDb, in: -12 ... 12, step: 0.5) { editing in
                    if editing { vm.notePresetEdited() }
                    if !editing { vm.scheduleApply() }
                }
            }
        }
    }

    private func freqLabel(_ hz: Double) -> String {
        hz >= 1000 ? String(format: "%.0f kHz", hz / 1000) : String(format: "%.0f Hz", hz)
    }

    private func gainLabel(_ db: Double) -> String {
        db >= 0 ? String(format: "+%.1f dB", db) : String(format: "%.1f dB", db)
    }
}
