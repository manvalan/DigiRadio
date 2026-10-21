import SwiftUI

/// Schermata audio — stile Impostazioni iOS (inset grouped).
struct AudioSettingsView: View {
    @Bindable var vm: AdauStudioViewModel
    var focus: AudioSettingsFocus = .all

    var body: some View {
        Form {
            if let error = vm.errorMessage {
                Section {
                    Label(error, systemImage: "exclamationmark.triangle.fill")
                        .foregroundStyle(.red)
                }
            }

            if vm.isLoading {
                Section {
                    HStack {
                        Spacer()
                        ProgressView()
                        Spacer()
                    }
                }
            } else {
                if showsListening { listeningSections }
                if showsSound { soundSections }
                if showsDiagnostics { diagnosticsSections }
            }
        }
        .navigationTitle(navigationTitle)
        .navigationBarTitleDisplayMode(focus == .all ? .large : .inline)
        .toolbar {
            if vm.isSaving {
                ToolbarItem(placement: .topBarTrailing) {
                    ProgressView()
                }
            }
        }
        .animation(.snappy, value: vm.sourceChangeToken)
        .onAppear {
            Task { await vm.load() }
        }
    }

    private var showsListening: Bool { focus == .all || focus == .listening }
    private var showsSound: Bool { focus == .all || focus == .sound }
    private var showsDiagnostics: Bool { focus == .all || focus == .diagnostics }

    private var navigationTitle: String {
        switch focus {
        case .all: "Audio"
        case .listening: "Ascolto"
        case .sound: "Suono"
        case .diagnostics: "Diagnostica"
        }
    }

    @ViewBuilder
    private var listeningSections: some View {
        Section {
            IGIActiveSourceSegmentedPicker(selection: vm.activeSource) { source in
                Task { await vm.applyActiveSource(source) }
            }
            .listRowInsets(EdgeInsets(top: 10, leading: 16, bottom: 10, trailing: 16))
        } header: {
            Text("Sorgente audio")
        } footer: {
            Text("Radio o stream telefono — una sorgente alla volta. L’altoparlante è in La mia radio → Cambia altoparlante.")
        }

        Section {
            IGIAppleVolumeSlider(
                level: $vm.masterLevel,
                onEditingEnded: { vm.commitMasterVolume() }
            )
            .onChange(of: vm.masterLevel) { _, _ in
                vm.pushMasterLevelToChannels()
            }
        } header: {
            Text("Volume")
        } footer: {
            Text("\(vm.masterDbLabel)  (−96 … +12 dB).")
        }
    }

    @ViewBuilder
    private var soundSections: some View {
        Section {
            IGIActiveSourceSegmentedPicker(selection: vm.activeSource) { source in
                Task { await vm.applyActiveSource(source) }
            }
            .listRowInsets(EdgeInsets(top: 10, leading: 16, bottom: 10, trailing: 16))
        } header: {
            Text("Sorgente")
        } footer: {
            Text("Radio (FM/DAB) o stream — una alla volta. Il Bluetooth della scheda è solo l’uscita verso l’altoparlante. Il tono di test è in Diagnostica.")
        }

        Section {
            IGIEQCurvePreview(bands: vm.eqBands)
                .listRowInsets(EdgeInsets(top: 8, leading: 16, bottom: 8, trailing: 16))
        } header: {
            Text("Equalizzatore")
        }

        Section {
            ForEach($vm.eqBands) { $band in
                eqBandRow($band)
            }
        } footer: {
            Text("La banda a 20 Hz è un filtro passa-alto fisso del DSP.")
        }

        Section {
            VStack(alignment: .leading, spacing: 8) {
                HStack {
                    Text("Bass Boost")
                    Spacer()
                    Text("\(Int(vm.bassLevel))")
                        .foregroundStyle(.secondary)
                        .monospacedDigit()
                }
                Slider(value: $vm.bassLevel, in: 0 ... 100, step: 1) { editing in
                    if editing { vm.notePresetEdited() }
                    if !editing { Task { await vm.applyEnhancements() } }
                }
            }
            .padding(.vertical, 4)

            VStack(alignment: .leading, spacing: 8) {
                HStack {
                    Text("Stereo Spread")
                    Spacer()
                    Text("\(Int(vm.stereoLevel))")
                        .foregroundStyle(.secondary)
                        .monospacedDigit()
                }
                Slider(value: $vm.stereoLevel, in: 0 ... 100, step: 1) { editing in
                    if editing { vm.notePresetEdited() }
                    if !editing { Task { await vm.applyEnhancements() } }
                }
            }
            .padding(.vertical, 4)

            Toggle(isOn: Binding(
                get: { vm.voiceClean },
                set: { enabled in
                    guard vm.voiceClean != enabled else { return }
                    Task { await vm.toggleVoiceClean() }
                }
            )) {
                Text("Voice Clarifier")
            }
        } header: {
            Text("Miglioramento suono")
        } footer: {
            Text("Bass Boost e Stereo Spread: POST dedicati 0–100. L'EQ resta indipendente.")
        }
    }

    @ViewBuilder
    private func eqBandRow(_ band: Binding<EQBandState>) -> some View {
        if band.wrappedValue.isLocked {
            LabeledContent(freqLabel(band.wrappedValue.centerHz), value: "Fisso")
                .foregroundStyle(.secondary)
        } else {
            VStack(alignment: .leading, spacing: 6) {
                HStack {
                    Text(freqLabel(band.wrappedValue.centerHz))
                    Spacer()
                    Text(gainLabel(band.wrappedValue.gainDb))
                        .font(.subheadline.monospacedDigit())
                        .foregroundStyle(.secondary)
                }
                Slider(value: band.gainDb, in: -12 ... 12, step: 0.5) { editing in
                    if editing { vm.notePresetEdited() }
                    if !editing { vm.scheduleApply() }
                }
            }
            .padding(.vertical, 2)
        }
    }

    @ViewBuilder
    private var diagnosticsSections: some View {
        Section {
            Button("Attiva tono di test") {
                Task { await vm.testBeep(enabled: true) }
            }
            .disabled(!vm.isConnected || vm.isSaving)
            Button("Disattiva tono") {
                Task { await vm.testBeep(enabled: false) }
            }
            .disabled(!vm.isConnected || vm.isSaving)
        } header: {
            Text("Tono di test")
        } footer: {
            Text("Imposta la sorgente su beep. Solo per collaudo.")
        }

        Section {
            HStack {
                TextField("Cerca parametro", text: $vm.dspSearchText)
                Button("Carica") { Task { await vm.loadDspParams() } }
                    .disabled(!vm.isConnected)
            }
            if !vm.dspParams.isEmpty {
                Text("\(vm.dspParams.count) parametri")
                    .foregroundStyle(.secondary)
            }
            ForEach(Array(vm.filteredDspParams.prefix(20))) { param in
                Button {
                    vm.selectedDspParam = param
                    vm.dspParamValue = ""
                } label: {
                    LabeledContent(param.name, value: "#\(param.address)")
                }
                .foregroundStyle(.primary)
            }
            if let param = vm.selectedDspParam {
                TextField("Valore per \(param.name)", text: $vm.dspParamValue)
                    .keyboardType(.decimalPad)
                Button("Scrivi (non persistente)") {
                    Task { await vm.writeDspParam() }
                }
                .disabled(vm.dspParamValue.isEmpty)
            }
        } header: {
            Text("Parametri SigmaStudio")
        }

        Section {
            Button("Ripristina impostazioni audio", role: .destructive) {
                Task { await vm.reset() }
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

enum AudioSettingsFocus {
    case all, listening, sound, diagnostics
}
