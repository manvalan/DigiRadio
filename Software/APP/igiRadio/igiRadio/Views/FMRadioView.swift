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
        Form {
            Section {
                VStack(spacing: IGITheme.spacingM) {
                    Text(String(format: "%.2f", frequencyMHz))
                        .font(.system(size: 56, weight: .bold, design: .rounded))
                        .monospacedDigit()
                        .accessibilityLabel("Frequenza \(frequencyMHz) megahertz")
                    Slider(value: $frequencyMHz, in: 64 ... 108, step: 0.05)
                    HStack {
                        Button("−") { frequencyMHz = max(64, frequencyMHz - 0.1) }
                        Spacer()
                        Button("Sintonizza") { Task { await tune() } }
                            .buttonStyle(.borderedProminent)
                            .disabled(isTuning)
                        Spacer()
                        Button("+") { frequencyMHz = min(108, frequencyMHz + 0.1) }
                    }
                }
                .padding(.vertical, IGITheme.spacingS)
            }

            if let fm {
                Section("Stato") {
                    LabeledContent("Frequenza", value: String(format: "%.2f MHz", Double(fm.frequencyKhz) / 1000))
                    if let rssi = fm.rssiDbuv { LabeledContent("RSSI", value: "\(rssi) dBµV") }
                    if let snr = fm.snrDb { LabeledContent("SNR", value: "\(snr) dB") }
                    if let stereo = fm.stereo {
                        LabeledContent("Stereo", value: stereo ? "Sì" : "Mono")
                    }
                }
                if let ps = fm.stationName, !ps.isEmpty {
                    Section("RDS") {
                        LabeledContent("PS", value: ps)
                        if let rt = fm.radiotext, !rt.isEmpty {
                            Text(rt).font(.body)
                        }
                    }
                }
            }

            Section("Seek") {
                Button("Seek su") { Task { try? await environment.digiRadio.seekFM(direction: "up") } }
                Button("Seek giù") { Task { try? await environment.digiRadio.seekFM(direction: "down") } }
            }

            Section("Scan") {
                if isScanning {
                    HStack {
                        IGIScanningIndicator().frame(width: 24, height: 24)
                        Text("Scansione in corso…")
                    }
                }
                if let scanError {
                    Text(scanError).foregroundStyle(.red)
                }
                Button("Cerca prossima stazione") {
                    Task { await scanNext() }
                }
                .disabled(isScanning)
                Button("Scansione completa banda FM") {
                    Task { await scanFull() }
                }
                .disabled(isScanning)

                if !scanResults.isEmpty {
                    ForEach(scanResults) { hit in
                        Button {
                            Task { try? await environment.digiRadio.tuneFM(frequencyKhz: hit.frequencyKhz) }
                        } label: {
                            HStack {
                                VStack(alignment: .leading) {
                                    Text(hit.stationName ?? "Stazione FM")
                                        .font(.headline)
                                    Text(String(format: "%.2f MHz", Double(hit.frequencyKhz) / 1000))
                                        .font(.caption)
                                        .foregroundStyle(.secondary)
                                }
                                Spacer()
                                if let rssi = hit.rssiDbuv {
                                    Text("\(rssi) dBµV")
                                        .font(.caption)
                                        .foregroundStyle(.secondary)
                                }
                            }
                        }
                    }
                }
            }
        }
        .navigationTitle("FM")
        .onAppear {
            if let khz = environment.state.tuner.fm?.frequencyKhz {
                frequencyMHz = Double(khz) / 1000.0
            }
            Task { try? await environment.digiRadio.refreshTunerStatus() }
        }
        .onChange(of: environment.state.tuner.fm?.frequencyKhz) { _, newValue in
            if let khz = newValue { frequencyMHz = Double(khz) / 1000.0 }
        }
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
