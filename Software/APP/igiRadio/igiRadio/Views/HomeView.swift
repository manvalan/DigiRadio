import SwiftUI

struct HomeView: View {
    @Environment(AppEnvironment.self) private var environment
    @State private var viewModel: HomeViewModel?
    @State private var volume: Double = 42

    var body: some View {
        let state = environment.state
        ScrollView {
            VStack(alignment: .leading, spacing: IGITheme.spacingL) {
                header(state: state)
                nowPlayingCard(state: state)
                streamCard(state: state)
                quickLinks
                IGITransportCluster(
                    onPrevious: { Task { await viewModel?.seekFM("down") } },
                    onPlay: { Task { await viewModel?.refreshAll() } },
                    onNext: { Task { await viewModel?.seekFM("up") } }
                )
                volumeSection
                presetsSection(state: state)
            }
            .padding(IGITheme.spacingM)
        }
        .background(IGIHeroBackground())
        .navigationTitle("igiRadio")
        .navigationBarTitleDisplayMode(.large)
        .safeAreaInset(edge: .top) {
            if !state.connection.isConnected {
                connectionBanner
            }
        }
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                NavigationLink {
                    ConnectionView()
                } label: {
                    Label(state.connection.isConnected ? "Connesso" : "Connetti", systemImage: "link")
                }
            }
        }
        .onAppear {
            if viewModel == nil {
                viewModel = HomeViewModel(service: environment.digiRadio)
            }
            viewModel?.onAppear()
            volume = Double(state.tuner.volume)
            Task { try? await environment.digiRadio.refreshStreaming() }
        }
        .onDisappear { viewModel?.onDisappear() }
        .onChange(of: environment.state.tuner.volume) { _, newValue in
            volume = Double(newValue)
        }
    }

    private var connectionBanner: some View {
        NavigationLink {
            ConnectionView()
        } label: {
            HStack {
                Image(systemName: "wifi.exclamationmark")
                Text("DigiRadio non connesso — tocca per collegare")
                    .font(.subheadline.weight(.medium))
                Spacer()
                Image(systemName: "chevron.right")
            }
            .padding(.horizontal, IGITheme.spacingM)
            .padding(.vertical, 10)
            .background(.orange.opacity(0.15))
        }
        .buttonStyle(.plain)
    }

    @ViewBuilder
    private func header(state: DigiRadioState) -> some View {
        HStack(alignment: .top) {
            VStack(alignment: .leading, spacing: 8) {
                Text("DigiRadio")
                    .font(.title.weight(.bold))
                HStack(spacing: 8) {
                    IGIStatusDot(isConnected: state.connection.isConnected)
                    Text(state.connection.isConnected ? "Connesso" : "Non connesso")
                        .font(.subheadline)
                        .foregroundStyle(.secondary)
                }
                IGIBandBadge(band: state.tuner.band, locked: state.tuner.locked)
            }
            Spacer()
            if let rssi = state.tuner.fm?.rssiDbuv {
                VStack(alignment: .trailing, spacing: 6) {
                    Text("Segnale")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    IGISignalBar(level: Double(rssi) / 80.0)
                        .frame(width: 100)
                }
            }
        }
    }

    @ViewBuilder
    private func nowPlayingCard(state: DigiRadioState) -> some View {
        VStack(spacing: IGITheme.spacingM) {
            ZStack {
                RoundedRectangle(cornerRadius: 28, style: .continuous)
                    .fill(
                        LinearGradient(
                            colors: [
                                IGITheme.accent.opacity(0.45),
                                .purple.opacity(0.35),
                                IGITheme.accent.opacity(0.25)
                            ],
                            startPoint: .topLeading,
                            endPoint: .bottomTrailing
                        )
                    )
                    .frame(height: 200)
                    .overlay {
                        Circle()
                            .fill(.white.opacity(0.08))
                            .frame(width: 160, height: 160)
                            .blur(radius: 2)
                    }

                Image(systemName: state.tuner.band == .fm ? "radio.fill" : "antenna.radiowaves.left.and.right")
                    .font(.system(size: 56))
                    .foregroundStyle(.white.opacity(0.95))
                    .symbolEffect(.pulse, options: .repeating, value: state.tuner.locked)
            }

            VStack(spacing: 6) {
                Text(primaryTitle(state: state))
                    .font(.title2.weight(.bold))
                    .multilineTextAlignment(.center)
                    .contentTransition(.interpolate)
                Text(secondaryLine(state: state))
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
                    .multilineTextAlignment(.center)
                    .lineLimit(2)
            }
        }
        .igiPremiumCard()
        .animation(.snappy, value: state.tuner.fm?.frequencyKhz)
        .animation(.snappy, value: state.tuner.fm?.stationName)
    }

    @ViewBuilder
    private func streamCard(state: DigiRadioState) -> some View {
        NavigationLink {
            StreamingView()
        } label: {
            HStack(spacing: IGITheme.spacingM) {
                ZStack {
                    RoundedRectangle(cornerRadius: 14, style: .continuous)
                        .fill(state.streaming.enabled ? IGITheme.accent.opacity(0.2) : Color.secondary.opacity(0.12))
                        .frame(width: 52, height: 52)
                    Image(systemName: state.streaming.enabled ? "dot.radiowaves.forward" : "dot.radiowaves.forward.slash")
                        .font(.title2)
                        .foregroundStyle(state.streaming.enabled ? IGITheme.accent : .secondary)
                }
                VStack(alignment: .leading, spacing: 4) {
                    Text("Web radio stream")
                        .font(.headline)
                    if state.streaming.enabled, !state.streaming.url.isEmpty {
                        Text(state.streaming.url)
                            .font(.caption)
                            .foregroundStyle(.secondary)
                            .lineLimit(1)
                    } else {
                        Text("Nessuno stream attivo")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                }
                Spacer()
                Image(systemName: "chevron.right")
                    .foregroundStyle(.tertiary)
            }
            .igiPremiumCard()
        }
        .buttonStyle(.plain)
    }

    private var quickLinks: some View {
        HStack(spacing: IGITheme.spacingS) {
            NavigationLink {
                FMRadioView()
            } label: {
                quickLinkLabel("FM", icon: "dot.radiowaves.left.and.right")
            }
            NavigationLink {
                DABRadioView()
            } label: {
                quickLinkLabel("DAB", icon: "antenna.radiowaves.left.and.right")
            }
            NavigationLink {
                AudioProfilesView()
            } label: {
                quickLinkLabel("Profilo", icon: "waveform.path.ecg")
            }
            NavigationLink {
                StreamingView()
            } label: {
                quickLinkLabel("Stream", icon: "dot.radiowaves.forward")
            }
        }
    }

    @ViewBuilder
    private func quickLinkLabel(_ title: String, icon: String) -> some View {
        Label(title, systemImage: icon)
            .font(.caption.weight(.semibold))
            .frame(maxWidth: .infinity)
            .padding(.vertical, 12)
            .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
    }

    private var volumeSection: some View {
        VStack(alignment: .leading, spacing: IGITheme.spacingS) {
            HStack {
                Text("Volume")
                    .font(.headline)
                Spacer()
                Text("\(Int(volume))")
                    .font(.title3.weight(.bold).monospacedDigit())
                    .foregroundStyle(IGITheme.accent)
                    .contentTransition(.numericText())
            }
            IGIVolumeSlider(value: $volume) { value in
                Task { await viewModel?.setVolume(value) }
            }
        }
        .igiPremiumCard()
    }

    @ViewBuilder
    private func presetsSection(state: DigiRadioState) -> some View {
        IGISectionHeader(title: "Preset")
        if state.stations.isEmpty {
            IGIEmptyState(
                title: "Nessun preset",
                message: "Salva le tue stazioni preferite dalla schermata Preset.",
                systemImage: "star"
            )
        } else {
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: IGITheme.spacingS) {
                    ForEach(Array(state.stations.enumerated()), id: \.offset) { index, station in
                        IGIPresetChip(
                            name: station.name,
                            subtitle: station.band.rawValue.uppercased()
                        ) {
                            Task { await viewModel?.tunePreset(at: index) }
                        }
                    }
                }
            }
        }
    }

    private func primaryTitle(state: DigiRadioState) -> String {
        switch state.tuner.band {
        case .fm:
            if let name = state.tuner.fm?.stationName, !name.isEmpty { return name }
            if let khz = state.tuner.fm?.frequencyKhz {
                return String(format: "%.2f MHz", Double(khz) / 1000.0)
            }
            return "FM"
        case .dab:
            return state.tuner.dab?.dynamicLabel ?? "DAB"
        }
    }

    private func secondaryLine(state: DigiRadioState) -> String {
        switch state.tuner.band {
        case .fm:
            if let khz = state.tuner.fm?.frequencyKhz {
                let freq = String(format: "%.2f MHz", Double(khz) / 1000.0)
                if let rt = state.tuner.fm?.radiotext, !rt.isEmpty { return "\(freq) · \(rt)" }
                return freq
            }
            return state.tuner.fm?.radiotext ?? "FM Radio"
        case .dab:
            if let cnr = state.tuner.dab?.cnrDb {
                return "CNR \(cnr) dB"
            }
            return "Digital Radio"
        }
    }
}
