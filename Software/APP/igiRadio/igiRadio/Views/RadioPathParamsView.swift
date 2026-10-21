import SwiftUI

struct StationEditDraft: Equatable {
    var name: String
    var band: TunerBand
    var frequencyMHz: Double
    var dabFreqIndex: Int
    var dabServiceId: Int
    var dabComponentId: Int
    var isFavorite: Bool
    var originalID: String
    var fmFrequenciesKhz: [Int]

    init(station: Station, isFavorite: Bool) {
        name = station.displayLabel
        band = station.band
        frequencyMHz = Double(station.fmFrequencyKhz ?? 100_000) / 1000
        dabFreqIndex = station.dabFreqIndex ?? 0
        dabServiceId = station.dabServiceId ?? 0
        dabComponentId = station.dabComponentId ?? 0
        self.isFavorite = isFavorite
        originalID = station.id
        fmFrequenciesKhz = station.fmFrequencyKhz.map { [$0] } ?? []
    }

    init(station: ScannedStation, isFavorite: Bool) {
        name = station.name
        band = station.band
        frequencyMHz = Double(station.fmFrequencyKhz ?? 100_000) / 1000
        dabFreqIndex = station.dabFreqIndex ?? 0
        dabServiceId = station.dabServiceId ?? 0
        dabComponentId = station.dabComponentId ?? 0
        self.isFavorite = isFavorite
        originalID = station.id
        fmFrequenciesKhz = station.allFMFrequenciesKhz
    }

    func asScannedStation() -> ScannedStation {
        switch band {
        case .fm:
            let khz = Int((min(108, max(87.5, frequencyMHz)) * 1000).rounded())
            var freqs = Set(fmFrequenciesKhz)
            freqs.insert(khz)
            return ScannedStation(
                id: originalID.hasPrefix("fm-") ? originalID : "fm-\(khz)",
                name: name.trimmingCharacters(in: .whitespacesAndNewlines),
                band: .fm,
                fmFrequencyKhz: khz,
                dabFreqIndex: nil,
                dabServiceId: nil,
                dabComponentId: nil,
                rssiDbuv: nil,
                fmFrequenciesKhz: freqs.sorted(),
                hasBroadcastName: true
            )
        case .dab:
            return ScannedStation(
                id: originalID,
                name: name.trimmingCharacters(in: .whitespacesAndNewlines),
                band: .dab,
                fmFrequencyKhz: nil,
                dabFreqIndex: dabFreqIndex,
                dabServiceId: dabServiceId,
                dabComponentId: dabComponentId,
                rssiDbuv: nil,
                hasBroadcastName: true
            )
        }
    }
}

struct RadioPathParamsView: View {
    @Environment(AppEnvironment.self) private var environment
    var band: TunerBand = .fm
    var viewModel: HomeViewModel?

    @State private var frequencyMHz: Double = 87.5
    @State private var frequencyText = "87.50"
    @State private var showStationList = false
    @State private var editingDraft: StationEditDraft?

    private var scan: StationScanViewModel { environment.stationScan }

    var body: some View {
        ScrollView {
            VStack(spacing: 22) {
                if band == .fm {
                    fmTuner
                } else {
                    dabNowPlaying
                }

                Button {
                    showStationList = true
                } label: {
                    Image(systemName: "play.circle")
                        .font(.system(size: 42, weight: .regular))
                        .symbolRenderingMode(.hierarchical)
                        .foregroundStyle(IGITheme.ink)
                }
                .buttonStyle(.plain)
                .accessibilityLabel("Tutte le stazioni")
                .padding(.top, 4)

                favoritesBlock
            }
            .padding(.horizontal, 22)
            .padding(.top, 12)
            .padding(.bottom, 28)
            .frame(maxWidth: 560)
            .frame(maxWidth: .infinity)
        }
        .igiThemedScreen()
        .onAppear {
            scan.selectedBand = band == .dab ? .dab : .fm
            syncFrequencyFromDevice()
            Task {
                await environment.digiRadio.refreshFirmwareAF()
                await scan.refreshLibrary()
            }
        }
        .onChange(of: environment.state.tuner.fm?.frequencyKhz) { _, _ in
            syncFrequencyFromDevice()
        }
        .onChange(of: frequencyMHz) { _, mhz in
            frequencyText = String(format: "%.2f", mhz)
        }
        .sheet(isPresented: $showStationList) {
            stationListSheet
                .sheet(item: $editingDraft, content: editSheet)
        }
        .sheet(item: Binding(
            get: { showStationList ? nil : editingDraft },
            set: { editingDraft = $0 }
        ), content: editSheet)
    }

    private var fmTuner: some View {
        VStack(spacing: 10) {
            if let name = fmStationName {
                Text(name)
                    .font(.title2.weight(.semibold))
                    .foregroundStyle(IGITheme.ink)
                    .multilineTextAlignment(.center)
                    .lineLimit(2)
                    .minimumScaleFactor(0.8)
                    .padding(.horizontal, 8)
            }
            if let radiotext = RDSText.sanitized(environment.state.tuner.fm?.radiotext) {
                Text(radiotext)
                    .font(.subheadline.weight(.medium))
                    .foregroundStyle(IGITheme.ink)
                    .multilineTextAlignment(.center)
                    .lineLimit(3)
                    .padding(.horizontal, 12)
            }
            Text(String(format: "%.2f", frequencyMHz))
                .font(.system(size: 52, weight: .light, design: .rounded))
                .monospacedDigit()
                .foregroundStyle(IGITheme.ink)
            Text("MHz")
                .font(.subheadline.weight(.medium))
                .foregroundStyle(IGITheme.inkMuted)

            if let af = environment.state.firmwareAF {
                firmwareAFRow(af)
            }

            TextField("Frequenza", text: $frequencyText)
                .keyboardType(.decimalPad)
                .multilineTextAlignment(.center)
                .font(.body.monospacedDigit())
                .foregroundStyle(IGITheme.ink)
                .padding(.vertical, 10)
                .padding(.horizontal, 16)
                .background(IGITheme.card, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                .frame(maxWidth: 180)
                .onSubmit { applyTypedFrequency() }

            IGIRotaryEncoder(value: $frequencyMHz, onCommit: commitFrequency)
                .frame(width: 300, height: 300)
                .padding(.top, 8)
        }
        .padding(.top, 8)
    }

    /// RDS AF follow runs on the radio (firmware 0.9.2+): show state and switch.
    private func firmwareAFRow(_ af: FirmwareAFState) -> some View {
        VStack(spacing: 4) {
            Toggle("Segui frequenze alternative", isOn: Binding(
                get: { af.enabled },
                set: { enabled in
                    Task { try? await environment.digiRadio.setFirmwareAF(enabled: enabled) }
                }
            ))
            .font(.subheadline)
            .tint(IGITheme.accent)
            if let from = af.lastFromKhz, let to = af.lastToKhz {
                Text(String(
                    format: NSLocalizedString("Ultimo cambio della radio: %.2f → %.2f MHz", comment: ""),
                    Double(from) / 1000,
                    Double(to) / 1000
                ))
                .font(.caption)
                .foregroundStyle(IGITheme.inkMuted)
            }
        }
        .frame(maxWidth: 320)
    }

    private var dabNowPlaying: some View {
        VStack(spacing: 8) {
            Text(dabStationName)
                .font(.system(.title, design: .rounded).weight(.semibold))
                .foregroundStyle(IGITheme.ink)
                .multilineTextAlignment(.center)
            Text(dabSubtitle)
                .font(.subheadline)
                .foregroundStyle(IGITheme.inkMuted)
                .multilineTextAlignment(.center)
            if let label = RDSText.sanitized(environment.state.tuner.dab?.dynamicLabel),
               label != dabStationName {
                Text(label)
                    .font(.subheadline.weight(.medium))
                    .foregroundStyle(IGITheme.ink)
                    .multilineTextAlignment(.center)
                    .lineLimit(3)
                    .padding(.horizontal, 12)
            }
        }
        .padding(.top, 20)
        .padding(.bottom, 8)
    }

    private var fmStationName: String? {
        let khz = Int((frequencyMHz * 1000).rounded())
        return scan.displayName(
            forFMFrequencyKhz: khz,
            liveName: environment.state.tuner.fm?.stationName
        )
    }

    private var dabStationName: String {
        if let dab = environment.state.tuner.dab,
           let name = scan.displayName(
            forDAB: dab.freqIndex,
            serviceId: dab.playingServiceId,
            componentId: dab.playingComponentId,
            liveLabel: dab.dynamicLabel
           ) {
            return name
        }
        return environment.state.nowPlayingTitle
    }

    private var dabSubtitle: String {
        if let dab = environment.state.tuner.dab {
            var parts = ["Ensemble \(dab.freqIndex)"]
            if environment.state.tuner.locked { parts.append("sintonizzato") }
            return parts.joined(separator: " · ")
        }
        return "DAB"
    }

    private var favoritesBlock: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Preferiti")
                .font(.headline)
                .foregroundStyle(IGITheme.ink)

            if scan.filteredFavorites.isEmpty {
                Text("Nessun preferito. Tocca il tasto play per aprire tutte le stazioni.")
                    .font(.subheadline)
                    .foregroundStyle(IGITheme.inkMuted)
            } else {
                VStack(spacing: 0) {
                    ForEach(scan.filteredFavorites) { station in
                        RadioStationRow(
                            title: station.displayTitle,
                            subtitle: station.subtitle,
                            isFavorite: true,
                            onPlay: { Task { await scan.listenFavorite(station) } },
                            onEdit: { editingDraft = StationEditDraft(station: station, isFavorite: true) }
                        )
                        if station.id != scan.filteredFavorites.last?.id {
                            Divider().background(IGITheme.ink.opacity(0.08))
                        }
                    }
                }
                .padding(.horizontal, 12)
                .background(IGITheme.card, in: RoundedRectangle(cornerRadius: 16, style: .continuous))
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }

    private var stationListSheet: some View {
        NavigationStack {
            List {
                Section {
                    if scan.isScanning {
                        HStack {
                            ProgressView()
                            Text(scan.progressMessage ?? "Scansione in corso…")
                                .font(.footnote)
                                .foregroundStyle(IGITheme.inkMuted)
                            Spacer()
                            Button("Annulla") { scan.cancelScan() }
                        }
                    } else {
                        Button {
                            Task { await scan.startScan() }
                        } label: {
                            Label("Avvia scansione \(scan.selectedBand.rawValue)", systemImage: "magnifyingglass")
                        }
                        .disabled(!scan.isConnected)
                    }
                }

                if scan.listedStations.isEmpty {
                    ContentUnavailableView(
                        "Nessuna stazione \(scan.selectedBand.rawValue)",
                        systemImage: "radio",
                        description: Text("Stesso elenco del tab Radio. Avvia una ricerca o passa all'altra banda.")
                    )
                } else {
                    ForEach(scan.listedStations) { station in
                        IGIStationRow(
                            station: station,
                            isSaved: scan.isSaved(station),
                            isListening: scan.listeningStationID == station.id,
                            onListen: {
                                Task { await scan.listen(station) }
                                showStationList = false
                            },
                            onSave: { Task { await scan.save(station) } },
                            onEdit: {
                                editingDraft = StationEditDraft(station: station, isFavorite: scan.isSaved(station))
                            }
                        )
                        .listRowBackground(IGITheme.card)
                    }
                }
            }
            .scrollContentBackground(.hidden)
            .background(IGITheme.canvas)
            .navigationTitle("Tutte le stazioni")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("Chiudi") { showStationList = false }
                        .foregroundStyle(IGITheme.ink)
                }
            }
        }
        .presentationDetents([.large])
        .presentationDragIndicator(.visible)
    }

    private func editSheet(_ draft: StationEditDraft) -> some View {
        StationEditSheet(draft: draft) { saved in
            Task { await scan.applyStationEdit(saved) }
            editingDraft = nil
        } onDelete: { deleted in
            scan.deleteStation(deleted)
            editingDraft = nil
        } onCancel: {
            editingDraft = nil
        }
    }

    private func syncFrequencyFromDevice() {
        if let khz = environment.state.tuner.fm?.frequencyKhz {
            let mhz = Double(khz) / 1000
            frequencyMHz = mhz
            frequencyText = String(format: "%.2f", mhz)
        }
        scan.selectedBand = band == .dab ? .dab : .fm
    }

    private func commitFrequency() {
        frequencyText = String(format: "%.2f", frequencyMHz)
        Task { await viewModel?.tuneFM(frequencyMHz: frequencyMHz) }
    }

    private func applyTypedFrequency() {
        let normalized = frequencyText.replacingOccurrences(of: ",", with: ".")
        guard let mhz = Double(normalized) else { return }
        frequencyMHz = min(108, max(87.5, mhz))
        frequencyText = String(format: "%.2f", frequencyMHz)
        Task { await viewModel?.tuneFM(frequencyMHz: frequencyMHz) }
    }
}

struct RadioStationRow: View {
    var title: String
    var subtitle: String
    var isFavorite: Bool
    var onPlay: () -> Void
    var onEdit: () -> Void

    var body: some View {
        HStack {
            VStack(alignment: .leading, spacing: 2) {
                Text(title)
                    .foregroundStyle(IGITheme.ink)
                Text(subtitle)
                    .font(.footnote)
                    .foregroundStyle(IGITheme.inkMuted)
            }
            Spacer()
            if isFavorite {
                Image(systemName: "star.fill")
                    .font(.footnote)
                    .foregroundStyle(Color(red: 0.78, green: 0.62, blue: 0.18))
            }
        }
        .padding(.vertical, 10)
        .contentShape(Rectangle())
        .onTapGesture(perform: onPlay)
        .onLongPressGesture(minimumDuration: 0.45, perform: onEdit)
        .accessibilityHint("Tocco: ascolta. Pressione prolungata: modifica o elimina.")
    }
}

extension StationEditDraft: Identifiable {
    var id: String { originalID }
}

struct StationEditSheet: View {
    @State var draft: StationEditDraft
    var onSave: (StationEditDraft) -> Void
    var onDelete: (StationEditDraft) -> Void
    var onCancel: () -> Void
    @State private var confirmDelete = false

    var body: some View {
        NavigationStack {
            Form {
                Section("Identità") {
                    TextField("Nome", text: $draft.name)
                    Picker("Banda", selection: $draft.band) {
                        Text("FM").tag(TunerBand.fm)
                        Text("DAB").tag(TunerBand.dab)
                    }
                    .pickerStyle(.segmented)
                }

                if draft.band == .fm {
                    Section {
                        TextField("Principale MHz", value: $draft.frequencyMHz, format: .number.precision(.fractionLength(2)))
                            .keyboardType(.decimalPad)
                        if draft.fmFrequenciesKhz.count > 1 {
                            ForEach(draft.fmFrequenciesKhz, id: \.self) { khz in
                                Text(String(format: "%.2f MHz", Double(khz) / 1000))
                                    .foregroundStyle(IGITheme.inkMuted)
                            }
                        }
                    } header: {
                        Text("Frequenze")
                    } footer: {
                        Text("Le frequenze alternative RDS restano in questa scheda. Se il segnale cala, si prova quella più forte.")
                    }
                } else {
                    Section("DAB") {
                        Stepper("Ensemble \(draft.dabFreqIndex)", value: $draft.dabFreqIndex, in: 0...37)
                        TextField("Service ID", value: $draft.dabServiceId, format: .number)
                            .keyboardType(.numberPad)
                        TextField("Component ID", value: $draft.dabComponentId, format: .number)
                            .keyboardType(.numberPad)
                    }
                }

                Section {
                    Toggle(isOn: $draft.isFavorite) {
                        Label("Preferiti", systemImage: draft.isFavorite ? "star.fill" : "star")
                    }
                    .tint(Color(red: 0.78, green: 0.62, blue: 0.18))
                }

                Section {
                    Button("Elimina stazione", role: .destructive) {
                        confirmDelete = true
                    }
                }
            }
            .scrollContentBackground(.hidden)
            .background(IGITheme.canvas)
            .navigationTitle("Modifica stazione")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Annulla", action: onCancel)
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Salva") { onSave(draft) }
                        .disabled(draft.name.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
                }
            }
            .confirmationDialog(
                "Eliminare “\(draft.name)”?",
                isPresented: $confirmDelete,
                titleVisibility: .visible
            ) {
                Button("Elimina", role: .destructive) { onDelete(draft) }
                Button("Annulla", role: .cancel) {}
            } message: {
                Text("La toglie dall'elenco sull'iPhone e, se era un preferito, anche dai preset del DigiRadio.")
            }
        }
        .presentationDetents([.large])
        .tint(IGITheme.accent)
    }
}
