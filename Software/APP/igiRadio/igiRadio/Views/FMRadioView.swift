import SwiftUI

struct FMRadioView: View {
    @Environment(AppEnvironment.self) private var environment
    @State private var frequencyMHz: Double = 102.3
    @State private var isTuning = false
    @State private var isScanning = false
    @State private var scanResults: [FMScanHit] = []
    @State private var scanError: String?

    var body: some View {
        let fm = environment.state.tuner.fm
        ScrollView {
            VStack(alignment: .leading, spacing: IGITheme.spacingL) {
                IGIFrequencyHero(
                    frequencyMHz: $frequencyMHz,
                    stationName: fm?.stationName,
                    isTuning: isTuning,
                    onTune: { Task { await tune() } }
                )

                if let fm {
                    signalCard(fm)
                    if hasRds(fm) {
                        rdsCard(fm)
                    }
                }

                seekSection
                scanSection
            }
            .padding(IGITheme.spacingM)
        }
        .background(IGIHeroBackground())
        .navigationTitle("FM")
        .navigationBarTitleDisplayMode(.large)
        .onAppear {
            if let khz = environment.state.tuner.fm?.frequencyKhz {
                frequencyMHz = Double(khz) / 1000.0
            }
        }
        .task {
            // RDS PS/radiotext arrive over several seconds after a tune:
            // keep polling while this screen is visible (same 3 s as Home).
            while !Task.isCancelled {
                if environment.state.connection.isConnected {
                    try? await environment.digiRadio.refreshTunerStatus()
                }
                try? await Task.sleep(for: .seconds(3))
            }
        }
        .onChange(of: environment.state.tuner.fm?.frequencyKhz) { _, newValue in
            if let khz = newValue {
                withAnimation(.snappy) {
                    frequencyMHz = Double(khz) / 1000.0
                }
            }
        }
    }

    @ViewBuilder
    private func signalCard(_ fm: FMTunerState) -> some View {
        VStack(alignment: .leading, spacing: IGITheme.spacingM) {
            Text("Segnale")
                .font(.headline)
            if let rssi = fm.rssiDbuv {
                IGIMetricBar(label: "RSSI", value: "\(rssi) dBµV", level: Double(rssi) / 80.0)
            }
            if let snr = fm.snrDb {
                IGIMetricBar(label: "SNR", value: "\(snr) dB", level: Double(snr) / 40.0)
            }
            if let stereo = fm.stereo {
                HStack {
                    Text("Stereo")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    Spacer()
                    Text(stereo ? "Sì" : "Mono")
                        .font(.caption.weight(.semibold))
                        .foregroundStyle(stereo ? .green : .secondary)
                }
            }
            if let offset = fm.freqOffPpm {
                HStack {
                    Text("Offset frequenza")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    Spacer()
                    Text("\(offset) ppm")
                        .font(.caption.weight(.semibold).monospacedDigit())
                }
            }
        }
        .igiPremiumCard()
    }

    private func hasRds(_ fm: FMTunerState) -> Bool {
        !(fm.stationName ?? "").isEmpty || !(fm.radiotext ?? "").isEmpty
            || !(fm.afKhz ?? []).isEmpty
    }

    @ViewBuilder
    private func rdsCard(_ fm: FMTunerState) -> some View {
        VStack(alignment: .leading, spacing: IGITheme.spacingS) {
            HStack {
                Image(systemName: "text.bubble.fill")
                    .foregroundStyle(IGITheme.accent)
                Text("RDS")
                    .font(.headline)
            }
            if let ps = fm.stationName, !ps.isEmpty {
                Text(ps)
                    .font(.title3.weight(.semibold))
                    .contentTransition(.interpolate)
            }
            if let rt = fm.radiotext, !rt.isEmpty {
                Text(rt)
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
                    .italic()
                    .contentTransition(.interpolate)
            }
            if let afs = fm.afKhz, !afs.isEmpty {
                alternateFrequencies(afs)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .igiPremiumCard()
        .animation(.snappy, value: fm.stationName)
        .animation(.snappy, value: fm.radiotext)
        .animation(.snappy, value: fm.afKhz)
    }

    /// RDS AF list: tapping one retunes to that transmitter of the same station.
    @ViewBuilder
    private func alternateFrequencies(_ afs: [Int]) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            Text("Frequenze alternative")
                .font(.caption)
                .foregroundStyle(.secondary)
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: IGITheme.spacingS) {
                    ForEach(afs, id: \.self) { khz in
                        Button {
                            frequencyMHz = Double(khz) / 1000.0
                            Task { await tune() }
                        } label: {
                            Text(String(format: "%.2f", Double(khz) / 1000.0))
                                .font(.caption.weight(.semibold).monospacedDigit())
                                .padding(.horizontal, 10)
                                .padding(.vertical, 6)
                                .background(.ultraThinMaterial, in: Capsule())
                        }
                        .buttonStyle(.plain)
                        .accessibilityLabel("Sintonizza \(String(format: "%.2f", Double(khz) / 1000.0)) MHz")
                    }
                }
            }
        }
    }

    private var seekSection: some View {
        HStack(spacing: IGITheme.spacingM) {
            Button {
                Task { try? await environment.digiRadio.seekFM(direction: "down") }
            } label: {
                Label("Giù", systemImage: "chevron.down")
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 14)
            }
            .buttonStyle(.bordered)

            Button {
                Task { try? await environment.digiRadio.seekFM(direction: "up") }
            } label: {
                Label("Su", systemImage: "chevron.up")
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 14)
            }
            .buttonStyle(.bordered)
        }
    }

    @ViewBuilder
    private var scanSection: some View {
        VStack(alignment: .leading, spacing: IGITheme.spacingM) {
            Text("Scan")
                .font(.headline)

            if isScanning {
                HStack(spacing: IGITheme.spacingS) {
                    IGIScanningIndicator().frame(width: 28, height: 28)
                    Text("Scansione in corso…")
                        .foregroundStyle(.secondary)
                }
            }

            if let scanError {
                Text(scanError).font(.caption).foregroundStyle(.red)
            }

            HStack(spacing: IGITheme.spacingS) {
                Button("Prossima stazione") { Task { await scanNext() } }
                    .buttonStyle(.borderedProminent)
                    .disabled(isScanning)
                Button("Banda completa") { Task { await scanFull() } }
                    .buttonStyle(.bordered)
                    .disabled(isScanning)
            }

            if !scanResults.isEmpty {
                VStack(spacing: IGITheme.spacingS) {
                    ForEach(scanResults) { hit in
                        IGIScanResultRow(
                            title: hit.stationName ?? "Stazione FM",
                            frequencyMHz: Double(hit.frequencyKhz) / 1000,
                            rssi: hit.rssiDbuv
                        ) {
                            Task { try? await environment.digiRadio.tuneFM(frequencyKhz: hit.frequencyKhz) }
                        }
                    }
                }
            }
        }
        .igiPremiumCard()
    }

    private func tune() async {
        isTuning = true
        defer { isTuning = false }
        let khz = Int((frequencyMHz * 1000).rounded())
        try? await environment.digiRadio.tuneFM(frequencyKhz: khz)
    }

    private func scanNext() async {
        isScanning = true
        scanError = nil
        defer { isScanning = false }
        do {
            let result = try await environment.digiRadio.scanFM(maxSteps: 45, name: nil)
            if result.status == "found", let khz = result.frequencyKhz {
                try? await environment.digiRadio.tuneFM(frequencyKhz: khz)
            } else {
                scanError = "Nessuna stazione trovata"
            }
        } catch {
            scanError = error.localizedDescription
        }
    }

    private func scanFull() async {
        isScanning = true
        scanError = nil
        defer { isScanning = false }
        do {
            scanResults = try await environment.digiRadio.scanFullFM()
            if scanResults.isEmpty {
                scanError = "Nessuna stazione nella banda"
            }
        } catch {
            scanError = error.localizedDescription
        }
    }
}
