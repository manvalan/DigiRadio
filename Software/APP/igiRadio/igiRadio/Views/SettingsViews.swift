import SwiftUI

enum SettingsHubTab: String, CaseIterable, Identifiable {
    case radio
    case audio

    var id: String { rawValue }

    var title: String {
        switch self {
        case .radio: "Radio"
        case .audio: "Audio"
        }
    }
}

struct SettingsRootView: View {
    @State private var tab: SettingsHubTab = .radio

    var body: some View {
        VStack(spacing: 0) {
            Picker("Sezione", selection: $tab) {
                ForEach(SettingsHubTab.allCases) { item in
                    Text(item.title).tag(item)
                }
            }
            .pickerStyle(.segmented)
            .padding(.horizontal, 16)
            .padding(.vertical, 12)

            List {
                switch tab {
                case .radio:
                    radioSection
                case .audio:
                    audioSection
                }
            }
            .listStyle(.insetGrouped)
        }
        .igiThemedScreen()
        .navigationTitle("Impostazioni")
        .navigationBarTitleDisplayMode(.large)
        .animation(.snappy, value: tab)
    }

    @ViewBuilder
    private var radioSection: some View {
        settingsLink("Scansione FM", systemName: "slider.horizontal.2.magnifyingglass", color: .orange) {
            FMScanSettingsView()
        }
        settingsLink("Stream", systemName: "dot.radiowaves.forward", color: .indigo) {
            StreamingView()
        }
        settingsLink("Info", systemName: "info.circle", color: .gray) {
            AboutView()
        }
    }

    @ViewBuilder
    private var audioSection: some View {
        settingsLink("Altoparlante", systemName: "hifispeaker.fill", color: .blue) {
            PrepareRadioView(entry: .speaker)
        }
        settingsLink("Profili", systemName: "waveform.path.ecg", color: .teal) {
            AudioProfilesView()
        }
    }

    private func settingsLink<Destination: View>(
        _ title: String,
        systemName: String,
        color: Color,
        @ViewBuilder destination: () -> Destination
    ) -> some View {
        NavigationLink {
            destination()
        } label: {
            Label {
                Text(title)
            } icon: {
                IGISettingsIcon(systemName: systemName, color: color)
            }
        }
    }
}

struct ConfigurationRootView: View {
    var body: some View {
        MyRadioView()
    }
}

struct DeviceInfoView: View {
    @Environment(AppEnvironment.self) private var environment
    @State private var didProbe = false

    private var isLive: Bool {
        didProbe && environment.state.connection.isConnected
    }

    var body: some View {
        let health = environment.state.health
        let chips = health.chips
        Form {
            Section {
                LabeledContent("Nome", value: DeviceIdentity.productName)
                LabeledContent("Firmware", value: live(health.firmware))
                LabeledContent("Serial number", value: live(health.serialNumber))
                LabeledContent("Stato", value: live(health.status))
            }
            Section("Chip") {
                LabeledContent("Si4684", value: chip(chips.si4684))
                LabeledContent("ADAU1701", value: chip(chips.adau1701))
                LabeledContent("BT1035", value: chip(chips.bt1035))
            }
            Section("Connessione") {
                LabeledContent(
                    "Host",
                    value: isLive
                        ? HostNormalizer.displayHost(from: environment.state.connection.host)
                        : "—"
                )
                LabeledContent("HTTP", value: isLive ? "Connesso" : "Non connesso")
            }
        }
        .navigationTitle("Informazioni")
        .navigationBarTitleDisplayMode(.inline)
        .onAppear {
            didProbe = false
            Task {
                _ = await environment.digiRadio.verifyReachability()
                didProbe = true
            }
        }
    }

    private func live(_ value: String) -> String {
        isLive && !value.isEmpty ? value : "—"
    }

    private func chip(_ ok: Bool) -> String {
        isLive && ok ? "OK" : "—"
    }
}

struct DiagnosticsView: View {
    @Environment(AppEnvironment.self) private var environment
    @State private var viewModel: DiagnosticsViewModel?

    var body: some View {
        Group {
            if let viewModel {
                diagnosticsForm(vm: viewModel)
            } else {
                ProgressView()
            }
        }
        .navigationTitle("Diagnostica")
        .navigationBarTitleDisplayMode(.inline)
        .onAppear {
            if viewModel == nil {
                viewModel = DiagnosticsViewModel(service: environment.digiRadio)
            }
        }
    }

    @ViewBuilder
    private func diagnosticsForm(@Bindable vm: DiagnosticsViewModel) -> some View {
        Form {
            Section("Tuner") {
                LabeledContent("Booted", value: environment.state.tuner.booted ? "Sì" : "No")
                LabeledContent("Banda", value: environment.state.tuner.band.rawValue.uppercased())
                LabeledContent("Locked", value: environment.state.tuner.locked ? "Sì" : "No")
            }
            Section("Altoparlante (BT1035)") {
                LabeledContent("A2DP", value: environment.state.bluetooth.a2dpState.rawValue)
                LabeledContent("Modulo", value: environment.state.bluetooth.booted ? "Acceso" : "Spento")
            }
            Section("Test audio") {
                Button("Beep ON") {
                    Task { await vm.testBeep(enabled: true) }
                }
                .disabled(vm.isBusy)
                Button("Beep OFF") {
                    Task { await vm.testBeep(enabled: false) }
                }
                .disabled(vm.isBusy)
                Button("Elenco parametri DSP") {
                    Task { await vm.loadDspParams() }
                }
                .disabled(vm.isBusy)
                if let count = vm.dspParamCount {
                    LabeledContent("Parametri DSP", value: "\(count)")
                }
            }
            Section("Altoparlante — avanzate") {
                Stepper("Auto-riconnessione: \(vm.autoReconnectTimes)", value: $vm.autoReconnectTimes, in: 0...15)
                Button("Salva auto-riconnessione") {
                    Task { await vm.saveAutoReconnect() }
                }
                .disabled(vm.isBusy)
            }
            if let error = vm.errorMessage {
                Section {
                    Text(error).foregroundStyle(.red)
                }
            }
            if let status = vm.statusMessage {
                Section {
                    Text(status)
                        .foregroundStyle(.secondary)
                }
            }
            Section {
                Button("Aggiorna snapshot") {
                    Task { await vm.refreshSnapshot() }
                }
                .disabled(vm.isBusy)
            }
        }
    }
}

struct FirmwareView: View {
    @Environment(AppEnvironment.self) private var environment
    @State private var didProbe = false

    private var isLive: Bool {
        didProbe && environment.state.connection.isConnected
    }

    var body: some View {
        Form {
            Section("Versione corrente") {
                LabeledContent(
                    "Firmware",
                    value: isLive && !environment.state.health.firmware.isEmpty
                        ? environment.state.health.firmware
                        : "—"
                )
                LabeledContent("HTTP", value: isLive ? "Connesso" : "Non connesso")
            }
            Section {
                Text("L'aggiornamento firmware è supportato via POST /api/system/ota con un file .bin ESP-IDF valido. L'interfaccia di upload sarà aggiunta in una versione successiva.")
                    .foregroundStyle(.secondary)
            } header: {
                Text("Aggiornamento OTA")
            }
        }
        .navigationTitle("Firmware")
        .navigationBarTitleDisplayMode(.inline)
        .onAppear {
            didProbe = false
            Task {
                _ = await environment.digiRadio.verifyReachability()
                didProbe = true
            }
        }
    }
}

struct FMScanSettingsView: View {
    @AppStorage(FMScanPreferences.rssiKey) private var minRssi = FMScanPreferences.defaultRssiDbuv
    @AppStorage(FMScanPreferences.snrKey) private var minSnr = FMScanPreferences.defaultSnrDb

    var body: some View {
        Form {
            Section {
                Stepper(value: $minRssi, in: FMScanPreferences.rssiRange) {
                    LabeledContent("RSSI minimo", value: "\(minRssi) dBµV")
                }
                Slider(
                    value: Binding(
                        get: { Double(minRssi) },
                        set: { minRssi = Int($0.rounded()) }
                    ),
                    in: Double(FMScanPreferences.rssiRange.lowerBound)
                        ... Double(FMScanPreferences.rssiRange.upperBound),
                    step: 1
                )
                .accessibilityLabel("RSSI minimo")
            } header: {
                Text("Soglia segnale")
            } footer: {
                Text("Più bassa: più stazioni, anche deboli. Più alta: solo emittenti chiare. Default \(FMScanPreferences.defaultRssiDbuv) dBµV.")
            }

            Section {
                Stepper(value: $minSnr, in: FMScanPreferences.snrRange) {
                    LabeledContent("SNR minimo", value: minSnr == 0 ? "Off" : "\(minSnr) dB")
                }
                Slider(
                    value: Binding(
                        get: { Double(minSnr) },
                        set: { minSnr = Int($0.rounded()) }
                    ),
                    in: Double(FMScanPreferences.snrRange.lowerBound)
                        ... Double(FMScanPreferences.snrRange.upperBound),
                    step: 1
                )
                .accessibilityLabel("SNR minimo")
            } header: {
                Text("Soglia rumore")
            } footer: {
                Text("0 disattiva il filtro SNR e trova più frequenze. Alza se vuoi scartare i segnali rumorosi.")
            }

            Section {
                Button("Ripristina default") {
                    minRssi = FMScanPreferences.defaultRssiDbuv
                    minSnr = FMScanPreferences.defaultSnrDb
                    FMScanPreferences.resetToDefault()
                }
            }
        }
        .navigationTitle("Scansione FM")
        .navigationBarTitleDisplayMode(.inline)
    }
}

struct AboutView: View {
    private var appVersion: String {
        let info = Bundle.main.infoDictionary
        let version = info?["CFBundleShortVersionString"] as? String ?? "—"
        let build = info?["CFBundleVersion"] as? String ?? "—"
        return "\(version) (\(build))"
    }

    var body: some View {
        Form {
            Section("igiRadio") {
                LabeledContent("App", value: "igiRadio")
                LabeledContent("Versione", value: appVersion)
                LabeledContent("Dispositivo", value: "DigiRadio DAB+/FM")
            }
            Section("Ringraziamenti") {
                Text("Un grazie di cuore a **PCBWay**, che ha sponsorizzato parte della produzione e dell'assemblaggio dei prototipi di DigiRadio.")
                Text("Grazie al team PCBWay per aver creduto in questo progetto open hardware e averci aiutato a dare vita alle prime schede.")
                Link(destination: URL(string: "https://www.pcbway.com")!) {
                    Label("www.pcbway.com", systemImage: "safari")
                }
            }
        }
        .navigationTitle("Info")
    }
}
