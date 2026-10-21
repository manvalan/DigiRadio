import SwiftUI

private enum RadioCollection: String, CaseIterable, Identifiable {
    case fm = "FM"
    case dab = "DAB"
    case web = "Web"

    var id: String { rawValue }
}

struct PresetsView: View {
    @Environment(AppEnvironment.self) private var environment
    var lockedBand: StationScanBand?
    @State private var editingDraft: StationEditDraft?
    @State private var collection: RadioCollection = .fm
    @State private var webRadios: [SavedWebRadio] = WebRadioStore.load()
    @State private var streamingVM: StreamingViewModel?
    @State private var addingWeb = false
    @State private var newWebName = ""
    @State private var newWebURL = ""

    private var vm: StationScanViewModel { environment.stationScan }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 18) {
                header

                if lockedBand == nil {
                    HStack(spacing: 8) {
                        ForEach(RadioCollection.allCases) { item in
                            IGIAtmospherePill(title: item.rawValue, isOn: collection == item) {
                                collection = item
                                if item == .fm { vm.selectedBand = .fm }
                                if item == .dab { vm.selectedBand = .dab }
                            }
                        }
                    }
                }

                if let error = vm.errorMessage {
                    Text(error).font(.subheadline).foregroundStyle(.red)
                }
                if let status = vm.statusMessage {
                    Text(status).font(.footnote).foregroundStyle(IGITheme.inkMuted)
                }

                if collection == .web && lockedBand == nil {
                    webContent
                } else {
                    broadcastContent
                }
            }
            .padding(.horizontal, 20)
            .padding(.vertical, 16)
            .frame(maxWidth: 720)
            .frame(maxWidth: .infinity)
        }
        .igiThemedScreen()
        .navigationTitle(lockedBand?.rawValue ?? "Radio")
        .navigationBarTitleDisplayMode(.inline)
        .onAppear {
            if let lockedBand {
                vm.selectedBand = lockedBand
                collection = lockedBand == .dab ? .dab : .fm
            }
            if streamingVM == nil {
                streamingVM = StreamingViewModel(service: environment.digiRadio)
            }
        }
        .task { await vm.refreshLibrary() }
        .refreshable { await vm.refreshLibrary() }
        .sheet(item: $editingDraft) { draft in
            StationEditSheet(draft: draft) { saved in
                Task { await vm.applyStationEdit(saved) }
                editingDraft = nil
            } onDelete: { deleted in
                vm.deleteStation(deleted)
                editingDraft = nil
            } onCancel: {
                editingDraft = nil
            }
        }
        .sheet(isPresented: Binding(
            get: { vm.namingStation != nil },
            set: { if !$0 { vm.cancelNaming() } }
        )) {
            namingSheet
        }
        .sheet(isPresented: $addingWeb) {
            addWebSheet
        }
    }

    private var header: some View {
        HStack(alignment: .lastTextBaseline) {
            VStack(alignment: .leading, spacing: 4) {
                Text(lockedBand?.rawValue ?? "Radio")
                    .font(.largeTitle.weight(.semibold))
                    .foregroundStyle(IGITheme.ink)
                Text(headerSubtitle)
                    .font(.subheadline)
                    .foregroundStyle(IGITheme.inkMuted)
            }
            Spacer()
            if collection != .web {
                Button {
                    if vm.isScanning {
                        vm.cancelScan()
                    } else {
                        Task { await vm.startScan() }
                    }
                } label: {
                    Text(vm.isScanning ? "Annulla" : "Trova stazioni")
                        .font(.subheadline.weight(.semibold))
                        .padding(.horizontal, 14)
                        .frame(minHeight: 40)
                        .foregroundStyle(IGITheme.ink)
                        .background(IGITheme.card, in: Capsule())
                        .overlay { Capsule().strokeBorder(IGITheme.ink.opacity(0.10), lineWidth: 1) }
                }
                .buttonStyle(.plain)
                .disabled(!vm.isScanning && !vm.isConnected)
            }
        }
    }

    private var headerSubtitle: String {
        if vm.isScanning {
            return vm.progressMessage ?? "Scansione in corso…"
        }
        if collection == .web {
            return String(localized: "Webradio salvate")
        }
        if let listening = vm.filteredFavorites.first(where: { vm.isTuned(to: $0) }) {
            return String(localized: "In ascolto · \(listening.displayTitle)")
        }
        return String(localized: "Preferiti ed elenco")
    }

    @ViewBuilder
    private var broadcastContent: some View {
        if vm.isScanning {
            HStack(spacing: 10) {
                ProgressView()
                Text(vm.progressMessage ?? "Scansione…")
                    .font(.subheadline)
                    .foregroundStyle(IGITheme.inkMuted)
            }
        }

        Text("Preferiti")
            .font(.subheadline.weight(.semibold))
            .foregroundStyle(IGITheme.inkMuted)

        if vm.filteredFavorites.isEmpty {
            Text("Nessun preferito. Tieni premuto una stazione nell’elenco per salvarla.")
                .font(.subheadline)
                .foregroundStyle(IGITheme.inkMuted)
        } else {
            LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 10) {
                ForEach(vm.filteredFavorites) { station in
                    IGIStationCard(
                        kicker: station.band == .fm ? "FM" : "DAB",
                        title: station.displayTitle,
                        footnote: vm.isTuned(to: station) ? "In ascolto" : station.subtitle,
                        isCurrent: vm.isTuned(to: station)
                    ) {
                        Task {
                            await vm.listenFavorite(station)
                            environment.adauStudio.syncFromDevice()
                        }
                    }
                    .contextMenu {
                        Button("Modifica") {
                            editingDraft = StationEditDraft(station: station, isFavorite: true)
                        }
                        Button("Rimuovi", role: .destructive) {
                            vm.removeFavorites(ids: [station.id])
                        }
                    }
                }
            }
        }

        HStack {
            Text("Elenco")
                .font(.subheadline.weight(.semibold))
                .foregroundStyle(IGITheme.inkMuted)
            Spacer()
            if !vm.listedStations.isEmpty {
                Button("Svuota", role: .destructive) {
                    Task { await vm.clearResults() }
                }
                .font(.footnote)
            }
        }
        .padding(.top, 8)

        if vm.listedStations.isEmpty {
            Text(vm.isConnected
                 ? "Avvia una ricerca per riempire l’elenco."
                 : "Connetti la radio per cercare le stazioni.")
                .font(.subheadline)
                .foregroundStyle(IGITheme.inkMuted)
        } else {
            VStack(spacing: 0) {
                ForEach(Array(vm.listedStations.enumerated()), id: \.element.id) { index, station in
                    if index > 0 {
                        Divider().background(IGITheme.ink.opacity(0.08))
                    }
                    Button {
                        Task { await vm.listen(station) }
                    } label: {
                        HStack {
                            VStack(alignment: .leading, spacing: 2) {
                                Text(station.displayTitle)
                                    .foregroundStyle(IGITheme.ink)
                                Text(station.subtitle)
                                    .font(.footnote)
                                    .foregroundStyle(IGITheme.inkMuted)
                            }
                            Spacer()
                            if vm.isSaved(station) {
                                Image(systemName: "star.fill")
                                    .font(.footnote)
                                    .foregroundStyle(IGITheme.accent)
                            }
                        }
                        .padding(.horizontal, 16)
                        .padding(.vertical, 12)
                    }
                    .buttonStyle(.plain)
                    .contextMenu {
                        if !vm.isSaved(station) {
                            Button("Salva nei preferiti") {
                                Task { await vm.save(station) }
                            }
                        }
                        Button("Modifica") {
                            editingDraft = StationEditDraft(station: station, isFavorite: vm.isSaved(station))
                        }
                    }
                }
            }
            .background(IGITheme.card, in: RoundedRectangle(cornerRadius: IGITheme.cardRadius, style: .continuous))
            .overlay {
                RoundedRectangle(cornerRadius: IGITheme.cardRadius, style: .continuous)
                    .strokeBorder(IGITheme.ink.opacity(0.08), lineWidth: 1)
            }
        }

        Button {
            Task { await vm.saveCurrentTuner() }
        } label: {
            Text("Salva la sintonia attuale")
                .font(.subheadline.weight(.semibold))
                .frame(maxWidth: .infinity)
                .padding(.vertical, 12)
        }
        .buttonStyle(.plain)
        .foregroundStyle(IGITheme.ink)
        .background(IGITheme.card, in: RoundedRectangle(cornerRadius: 14, style: .continuous))
        .disabled(!vm.isConnected)
        .padding(.top, 8)
    }

    @ViewBuilder
    private var webContent: some View {
        Text("Webradio")
            .font(.subheadline.weight(.semibold))
            .foregroundStyle(IGITheme.inkMuted)

        LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 10) {
            ForEach(webRadios) { radio in
                IGIStationCard(
                    kicker: radio.hostLabel,
                    title: radio.name,
                    footnote: isPlaying(radio) ? "In ascolto" : "URL",
                    isCurrent: isPlaying(radio)
                ) {
                    Task { await play(radio) }
                }
                .contextMenu {
                    Button("Rimuovi", role: .destructive) {
                        webRadios.removeAll { $0.id == radio.id }
                        WebRadioStore.save(webRadios)
                    }
                }
            }
        }

        Button {
            addingWeb = true
        } label: {
            Text("Aggiungi webradio")
                .font(.subheadline.weight(.semibold))
                .frame(maxWidth: .infinity)
                .padding(.vertical, 12)
        }
        .buttonStyle(.plain)
        .foregroundStyle(IGITheme.ink)
        .background(IGITheme.card, in: RoundedRectangle(cornerRadius: 14, style: .continuous))

        Text("Le altre app restano una sorgente in Ascolto, non una stazione in elenco.")
            .font(.footnote)
            .foregroundStyle(IGITheme.inkMuted)
            .padding(.top, 4)
    }

    private func isPlaying(_ radio: SavedWebRadio) -> Bool {
        environment.state.streaming.enabled
            && environment.state.streaming.url == radio.url
            && environment.adauStudio.activeSource == .bluetooth
    }

    private func play(_ radio: SavedWebRadio) async {
        guard let streamingVM else { return }
        await streamingVM.playSaved(radio)
        environment.adauStudio.syncFromDevice()
    }

    @ViewBuilder
    private var namingSheet: some View {
        NavigationStack {
            Form {
                Section("Nome stazione FM") {
                    TextField("Nome", text: Binding(
                        get: { vm.namingText },
                        set: { vm.namingText = $0 }
                    ))
                }
            }
            .navigationTitle("Salva preferito")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Annulla") { vm.cancelNaming() }
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Salva") { Task { await vm.confirmSaveWithCustomName() } }
                }
            }
        }
        .presentationDetents([.medium])
    }

    private var addWebSheet: some View {
        NavigationStack {
            Form {
                Section {
                    TextField("Nome", text: $newWebName)
                    TextField("https://stream.example.com/play.mp3", text: $newWebURL)
                        .textInputAutocapitalization(.never)
                        .keyboardType(.URL)
                }
            }
            .navigationTitle("Webradio")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Annulla") { addingWeb = false }
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Salva") {
                        let name = newWebName.trimmingCharacters(in: .whitespacesAndNewlines)
                        var url = newWebURL.trimmingCharacters(in: .whitespacesAndNewlines)
                        guard !name.isEmpty, !url.isEmpty else { return }
                        if !(url.hasPrefix("http://") || url.hasPrefix("https://")) {
                            url = "http://\(url)"
                        }
                        webRadios.append(SavedWebRadio(id: url, name: name, url: url))
                        WebRadioStore.save(webRadios)
                        newWebName = ""
                        newWebURL = ""
                        addingWeb = false
                    }
                }
            }
        }
        .presentationDetents([.medium])
    }
}
