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
                transportControls
                volumeSection
                presetsSection(state: state)
            }
            .padding(IGITheme.spacingM)
        }
        .background(IGITheme.screenBackground)
        .navigationTitle("igiRadio")
        .safeAreaInset(edge: .top) {
            if !state.connection.isConnected {
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
        }
        .onDisappear { viewModel?.onDisappear() }
        .onChange(of: environment.state.tuner.volume) { _, newValue in
            volume = Double(newValue)
        }
    }

    @ViewBuilder
    private func header(state: DigiRadioState) -> some View {
        HStack {
            VStack(alignment: .leading, spacing: 4) {
                Text("DigiRadio")
                    .font(.largeTitle.bold())
                HStack(spacing: 8) {
                    IGIStatusDot(isConnected: state.connection.isConnected)
                    Text(state.connection.isConnected ? "Connesso" : "Non connesso")
                        .foregroundStyle(.secondary)
                }
            }
            Spacer()
            if let rssi = state.tuner.fm?.rssiDbuv {
                VStack(alignment: .trailing) {
                    Text("Segnale")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    IGISignalBar(level: Double(rssi) / 80.0)
                        .frame(width: 120)
                }
            }
        }
    }

    @ViewBuilder
    private func nowPlayingCard(state: DigiRadioState) -> some View {
        IGICard {
            VStack(spacing: IGITheme.spacingM) {
                ZStack {
                    RoundedRectangle(cornerRadius: 24, style: .continuous)
                        .fill(
                            LinearGradient(
                                colors: [.accentColor.opacity(0.35), .purple.opacity(0.25)],
                                startPoint: .topLeading,
                                endPoint: .bottomTrailing
                            )
                        )
                        .frame(height: 220)
                    Image(systemName: state.tuner.band == .fm ? "radio.fill" : "antenna.radiowaves.left.and.right")
                        .font(.system(size: 64))
                        .foregroundStyle(.white.opacity(0.9))
                }

                VStack(spacing: 6) {
                    Text(primaryTitle(state: state))
                        .font(.title2.weight(.semibold))
                        .multilineTextAlignment(.center)
                    Text(secondaryLine(state: state))
                        .font(.subheadline)
                        .foregroundStyle(.secondary)
                        .multilineTextAlignment(.center)
                }
            }
        }
    }

    private var transportControls: some View {
        HStack(spacing: IGITheme.spacingL) {
            Button {
                Task { await viewModel?.seekFM("down") }
            } label: {
                Image(systemName: "backward.fill")
                    .font(.title2)
                    .frame(width: 56, height: 56)
            }
            .buttonStyle(.bordered)

            Button {
                Task { await viewModel?.refreshAll() }
            } label: {
                Image(systemName: "play.fill")
                    .font(.largeTitle)
                    .frame(width: 72, height: 72)
            }
            .buttonStyle(.borderedProminent)
            .clipShape(Circle())

            Button {
                Task { await viewModel?.seekFM("up") }
            } label: {
                Image(systemName: "forward.fill")
                    .font(.title2)
                    .frame(width: 56, height: 56)
            }
            .buttonStyle(.bordered)
        }
        .frame(maxWidth: .infinity)
    }

    private var volumeSection: some View {
        IGICard {
            IGIVolumeSlider(value: $volume) { value in
                Task { await viewModel?.setVolume(value) }
            }
        }
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
                        Button {
                            Task { await viewModel?.tunePreset(at: index) }
                        } label: {
                            VStack(alignment: .leading) {
                                Text(station.name)
                                    .font(.headline)
                                Text(station.band.rawValue.uppercased())
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                            }
                            .padding()
                            .background(IGITheme.cardBackground, in: RoundedRectangle(cornerRadius: 14))
                        }
                        .buttonStyle(.plain)
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
                return String(format: "%.2f MHz", Double(khz) / 1000.0)
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
