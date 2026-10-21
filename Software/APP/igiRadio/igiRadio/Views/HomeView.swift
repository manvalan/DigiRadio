import SwiftUI

struct HomeView: View {
    @Environment(AppEnvironment.self) private var environment
    @Environment(\.horizontalSizeClass) private var sizeClass
    @State private var viewModel: HomeViewModel?
    @State private var showRadioParams = false
    @State private var showDABParams = false
    @State private var showStreamParams = false
    @State private var showSpeaker = false
    @State private var lastRDSName: String?

    private var audio: AdauStudioViewModel { environment.adauStudio }
    private var state: DigiRadioState { environment.state }
    private var scan: StationScanViewModel { environment.stationScan }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                header

                if let shareBanner = environment.shareBanner {
                    Text(shareBanner)
                        .font(.subheadline.weight(.semibold))
                        .foregroundStyle(.white)
                        .multilineTextAlignment(.center)
                        .padding(.horizontal, 16)
                        .padding(.vertical, 10)
                        .frame(maxWidth: .infinity)
                        .background(IGITheme.accent, in: RoundedRectangle(cornerRadius: 14, style: .continuous))
                        .onTapGesture { environment.shareBanner = nil }
                }

                IGINowPlayingCard(
                    kicker: nowPlayingKicker,
                    title: nowPlayingTitle,
                    frequency: nowPlayingFrequency,
                    messageKind: digitalMessageKind,
                    message: nowPlayingMessage
                )
                .animation(.snappy, value: nowPlayingMessage)
                .opacity(state.power == .standby ? 0.45 : 1)

                HStack(spacing: 8) {
                    IGIAtmospherePill(title: "FM", isOn: fmIsActive) {
                        Task { await selectFM() }
                    }
                    IGIAtmospherePill(title: "DAB", isOn: dabIsActive) {
                        Task { await selectDAB() }
                    }
                    IGIAtmospherePill(title: "Stream", isOn: streamIsActive) {
                        Task { await selectStream() }
                    }
                }

                IGIAtmosphereVolume(
                    level: Binding(
                        get: { audio.masterLevel },
                        set: { audio.masterLevel = $0 }
                    ),
                    dbLabel: audio.masterDbLabel,
                    onEditingEnded: {
                        audio.pushMasterLevelToChannels()
                        Task { await audio.setMasterVolumeFromHome(audio.masterLevel) }
                    }
                )
                .onChange(of: audio.masterLevel) { _, _ in
                    audio.pushMasterLevelToChannels()
                }

                IGIOutputDestinationCard(
                    title: speakerOutTitle,
                    subtitle: speakerOutSubtitle,
                    isLinked: state.bluetooth.a2dpState.isLinked,
                    levels: audio.outputLevels,
                    levelsAvailable: audio.outputLevelsAvailable
                ) {
                    showSpeaker = true
                }

                if !quickFavorites.isEmpty {
                    VStack(alignment: .leading, spacing: 10) {
                        Text("Preferiti")
                            .font(.subheadline.weight(.semibold))
                            .foregroundStyle(IGITheme.inkMuted)
                        IGIChipWrap(spacing: 8) {
                            ForEach(quickFavorites) { station in
                                IGIFavoriteChip(
                                    title: station.displayTitle,
                                    isCurrent: scan.isTuned(to: station)
                                ) {
                                    Task {
                                        await scan.listenFavorite(station)
                                        environment.adauStudio.syncFromDevice()
                                    }
                                }
                            }
                        }
                    }
                    .accessibilityLabel("Preferiti")
                }
            }
            .padding(.horizontal, sizeClass == .regular ? 32 : 20)
            .padding(.vertical, 20)
            .frame(maxWidth: 720)
            .frame(maxWidth: .infinity)
        }
        .background(IGITheme.canvas)
        .toolbarBackground(IGITheme.canvas, for: .navigationBar)
        .navigationBarTitleDisplayMode(.inline)
        .toolbar {
            ToolbarItem(placement: .principal) {
                Text("igiRadio")
                    .font(.headline)
                    .foregroundStyle(IGITheme.ink)
            }
        }
        .sheet(isPresented: $showRadioParams) { radioParamsSheet }
        .sheet(isPresented: $showDABParams) { dabParamsSheet }
        .sheet(isPresented: $showStreamParams) { streamParamsSheet }
        .sheet(isPresented: $showSpeaker) { speakerSheet }
        .onAppear(perform: appear)
        .onDisappear {
            viewModel?.onDisappear()
            environment.adauStudio.stopOutputLevelsPolling()
        }
        .onChange(of: environment.state.audio.master) { _, _ in
            environment.adauStudio.syncFromDevice()
        }
        .onChange(of: environment.state.tuner.fm) { _, fm in
            lastRDSName = nil
            if let fm {
                applyLiveStationName(fm.stationName, khz: fm.frequencyKhz, afKhz: fm.afKhz ?? [])
            }
        }
        .onChange(of: environment.state.tuner.band) { _, band in
            if band != .fm {
                environment.stationScan.stopAlternateFrequencyMonitor()
            } else if let khz = environment.state.tuner.fm?.frequencyKhz {
                environment.stationScan.syncAlternateFrequencyMonitor(forFMKhz: khz)
            }
        }
        .contextMenu {
            Button("Sintonizza FM") { showRadioParams = true }
            Button("Sintonizza DAB") { showDABParams = true }
            Button("Stream") { showStreamParams = true }
            Button("Altoparlante") { showSpeaker = true }
        }
    }

    private var header: some View {
        HStack(alignment: .lastTextBaseline) {
            VStack(alignment: .leading, spacing: 4) {
                Text("Ascolto")
                    .font(.largeTitle.weight(.semibold))
                    .foregroundStyle(IGITheme.ink)
            }
            Spacer()
            powerButton
            Text(audio.masterDbLabel)
                .font(.title2.monospacedDigit().weight(.medium))
                .foregroundStyle(IGITheme.ink)
        }
    }

    private var powerButton: some View {
        let isOn = state.power == .on
        return Button {
            Task { await viewModel?.togglePower() }
        } label: {
            Image(systemName: "power")
                .font(.title3.weight(.bold))
                .foregroundStyle(isOn ? IGITheme.accent : .secondary)
                .frame(width: 40, height: 40)
                .background(
                    Circle().fill(isOn ? IGITheme.accent.opacity(0.2) : Color.secondary.opacity(0.12))
                )
        }
        .buttonStyle(.plain)
        .disabled(!state.connection.isConnected)
        .accessibilityLabel(isOn ? "Spegni DigiRadio" : "Accendi DigiRadio")
        .animation(.snappy, value: state.power)
    }

    private var fmIsActive: Bool {
        audio.activeSource == .radio && state.tuner.band == .fm
    }

    private var dabIsActive: Bool {
        audio.activeSource == .radio && state.tuner.band == .dab
    }

    private var streamIsActive: Bool {
        audio.activeSource == .bluetooth
    }

    private var nowPlayingKicker: String {
        if streamIsActive { return "Stream" }
        return state.tuner.band == .dab ? "DAB" : "FM"
    }

    private var nowPlayingTitle: String {
        if streamIsActive {
            if state.phoneAudioActive { return "Altre app" }
            if state.streaming.enabled {
                if let named = WebRadioStore.load().first(where: { $0.url == state.streaming.url }) {
                    return named.name
                }
                return URL(string: state.streaming.url)?.host ?? "Webradio"
            }
            return state.nowPlayingTitle
        }
        switch state.tuner.band {
        case .fm:
            if let khz = state.tuner.fm?.frequencyKhz,
               let name = scan.displayName(forFMFrequencyKhz: khz, liveName: state.tuner.fm?.stationName) {
                return name
            }
            return lastRDSName ?? state.nowPlayingTitle
        case .dab:
            if let dab = state.tuner.dab,
               let name = scan.displayName(
                forDAB: dab.freqIndex,
                serviceId: dab.playingServiceId,
                componentId: dab.playingComponentId,
                liveLabel: nil
               ) {
                return name
            }
            return "DAB"
        }
    }

    private var nowPlayingFrequency: String {
        if streamIsActive {
            if state.phoneAudioActive { return "Dal telefono" }
            if state.streaming.enabled { return "Webradio" }
            return state.nowPlayingSubtitle
        }
        switch state.tuner.band {
        case .fm:
            if let khz = state.tuner.fm?.frequencyKhz {
                if let station = scan.stationCarrying(fmKhz: khz),
                   station.allFMFrequenciesKhz.count > 1 {
                    return station.subtitle
                }
                return String(format: "%.2f MHz", Double(khz) / 1000)
            }
            return "FM"
        case .dab:
            if let idx = state.tuner.dab?.freqIndex {
                return "DAB · ensemble \(idx)"
            }
            return "DAB"
        }
    }

    private var digitalMessageKind: String {
        state.tuner.band == .dab ? "DAB" : "RDS"
    }

    private var nowPlayingMessage: String? {
        if streamIsActive { return nil }
        switch state.tuner.band {
        case .fm:
            guard fmIsActive else { return nil }
            return RDSText.sanitized(state.tuner.fm?.radiotext)
        case .dab:
            guard dabIsActive else { return nil }
            let label = RDSText.sanitized(state.tuner.dab?.dynamicLabel)
            if label == nowPlayingTitle { return nil }
            return label
        }
    }

    private var quickFavorites: [ScannedStation] {
        scan.favoriteStations
    }

    private var speakerOutTitle: String {
        let live = state.bluetooth.deviceName.trimmingCharacters(in: .whitespacesAndNewlines)
        if !live.isEmpty { return live }
        if let saved = state.bluetooth.savedSpeaker {
            let name = saved.name.trimmingCharacters(in: .whitespacesAndNewlines)
            return name.isEmpty ? "Altoparlante" : name
        }
        return "Altoparlante"
    }

    private var speakerOutSubtitle: String {
        if !state.bluetooth.booted { return "Uscita BT spenta" }
        return state.bluetooth.a2dpState.title
    }

    private func selectFM() async {
        if fmIsActive {
            showRadioParams = true
            return
        }
        await environment.stopPhoneCapture()
        await viewModel?.selectFM()
        environment.adauStudio.syncFromDevice()
    }

    private func selectDAB() async {
        if dabIsActive {
            showDABParams = true
            return
        }
        await environment.stopPhoneCapture()
        await viewModel?.selectDAB()
        environment.adauStudio.syncFromDevice()
    }

    private func selectStream() async {
        if streamIsActive {
            showStreamParams = true
            return
        }
        await viewModel?.selectStream()
        environment.adauStudio.syncFromDevice()
    }

    private var radioParamsSheet: some View {
        NavigationStack {
            RadioPathParamsView(band: .fm, viewModel: viewModel)
                .navigationTitle("FM")
                .navigationBarTitleDisplayMode(.inline)
                .toolbar {
                    ToolbarItem(placement: .confirmationAction) {
                        Button("Fine") { showRadioParams = false }
                    }
                }
        }
        .presentationDetents([.large])
        .presentationDragIndicator(.visible)
    }

    private var dabParamsSheet: some View {
        NavigationStack {
            RadioPathParamsView(band: .dab, viewModel: viewModel)
                .navigationTitle("DAB")
                .navigationBarTitleDisplayMode(.inline)
                .toolbar {
                    ToolbarItem(placement: .confirmationAction) {
                        Button("Fine") { showDABParams = false }
                    }
                }
        }
        .presentationDetents([.large])
        .presentationDragIndicator(.visible)
    }

    private var streamParamsSheet: some View {
        NavigationStack {
            StreamingView()
                .toolbar {
                    ToolbarItem(placement: .confirmationAction) {
                        Button("Fine") { showStreamParams = false }
                    }
                }
        }
        .presentationDetents([.large])
        .presentationDragIndicator(.visible)
    }

    private var speakerSheet: some View {
        NavigationStack {
            PrepareRadioView(entry: .speaker)
                .toolbar {
                    ToolbarItem(placement: .confirmationAction) {
                        Button("Fine") { showSpeaker = false }
                    }
                }
        }
        .presentationDetents([.large])
        .presentationDragIndicator(.visible)
    }

    private func applyLiveStationName(_ raw: String?, khz: Int?, afKhz: [Int] = []) {
        guard let khz else { return }
        environment.stationScan.ingestLiveFM(khz: khz, stationName: raw, afKhz: afKhz)
        environment.stationScan.syncAlternateFrequencyMonitor(forFMKhz: khz)
        lastRDSName = StationNameDisplay.meaningful(raw, khz: khz)
    }

    private func appear() {
        if viewModel == nil {
            viewModel = HomeViewModel(service: environment.digiRadio)
        }
        viewModel?.onAppear()
        environment.adauStudio.syncFromDevice()
        environment.adauStudio.startOutputLevelsPolling()
        lastRDSName = StationNameDisplay.meaningful(
            environment.state.tuner.fm?.stationName,
            khz: environment.state.tuner.fm?.frequencyKhz
        )
        Task {
            try? await environment.digiRadio.refreshStreaming()
            await viewModel?.refreshAll()
            await environment.stationScan.refreshLibrary()
            if let fm = environment.state.tuner.fm {
                applyLiveStationName(fm.stationName, khz: fm.frequencyKhz, afKhz: fm.afKhz ?? [])
            }
            environment.adauStudio.syncFromDevice()
        }
    }
}
