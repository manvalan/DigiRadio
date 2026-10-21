import SwiftUI

struct ConnectionView: View {
    @Environment(AppEnvironment.self) private var environment
    @State private var viewModel: ConnectionViewModel?
    @State private var discoveryTimer: Timer?
    @State private var confirmWifiReset = false
    @State private var didProbe = false

    private var isLive: Bool {
        didProbe && environment.state.connection.isConnected
    }

    var body: some View {
        Group {
            if let viewModel {
                connectionForm(viewModel, isLive: isLive)
            } else {
                ProgressView()
            }
        }
        .navigationTitle("Connessione")
        .navigationBarTitleDisplayMode(.inline)
        .onAppear {
            if viewModel == nil {
                viewModel = ConnectionViewModel(environment: environment)
            }
            guard let viewModel else { return }
            didProbe = false
            Task {
                await viewModel.verifyReachability()
                if environment.useMockDevice, !environment.state.connection.isConnected {
                    await viewModel.connect()
                } else if !environment.useMockDevice, !environment.state.connection.isConnected {
                    startDiscovery(viewModel)
                }
                didProbe = true
            }
        }
        .confirmationDialog(
            "Dimenticare la rete Wi‑Fi?",
            isPresented: $confirmWifiReset,
            titleVisibility: .visible
        ) {
            Button("Dimentica e riavvia", role: .destructive) {
                Task { await viewModel?.resetWifi() }
            }
            Button("Annulla", role: .cancel) {}
        } message: {
            Text("La scheda cancella le credenziali e torna in modalità setup. Dovrai riconfigurare la Wi‑Fi.")
        }
        .onDisappear {
            if let viewModel { stopDiscovery(viewModel) }
        }
    }

    @ViewBuilder
    private func connectionForm(_ vm: ConnectionViewModel, isLive: Bool) -> some View {
        let state = environment.state

        Form {
            Section("Prima configurazione Wi‑Fi") {
                NavigationLink("Configura Wi‑Fi") {
                    WiFiSetupView()
                }
                Text("Usa il BLE dell’ESP32 (igiRadio-seriale). Non serve l’altoparlante e non cambiare rete sul telefono.")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            }

            if !environment.useMockDevice {
                Section("Trova la scheda") {
                    if vm.discovery.isSearching {
                        HStack {
                            IGIScanningIndicator().frame(width: 28, height: 28)
                            VStack(alignment: .leading, spacing: 4) {
                                Text("Cerca igiRadio…")
                                if let status = vm.discovery.statusMessage {
                                    Text(status)
                                        .font(.caption)
                                        .foregroundStyle(.secondary)
                                }
                            }
                        }
                        Button("Ferma ricerca") { stopDiscovery(vm) }
                    } else {
                        Button("Cerca dispositivi") { startDiscovery(vm) }
                    }

                    if vm.discovery.devices.isEmpty, !vm.discovery.isSearching {
                        Text("Cerca la scheda sulla Wi‑Fi di casa (hostname .local o IP).")
                            .font(.footnote)
                            .foregroundStyle(.secondary)
                    }

                    ForEach(vm.discovery.devices) { device in
                        Button {
                            Task { await vm.connect(to: device.host) }
                        } label: {
                            HStack {
                                VStack(alignment: .leading, spacing: 4) {
                                    Text(device.name).font(.headline)
                                    Text(device.host).font(.caption).foregroundStyle(.secondary)
                                    if let fw = device.firmware {
                                        Text("FW \(fw)").font(.caption2).foregroundStyle(.secondary)
                                    }
                                }
                                Spacer()
                                if device.isReachable {
                                    Image(systemName: "checkmark.circle.fill").foregroundStyle(.green)
                                } else {
                                    Image(systemName: "hourglass").foregroundStyle(.orange)
                                }
                            }
                        }
                        .disabled(vm.isBusy)
                    }
                }
            }

            Section("Connessione HTTP") {
                TextField("igiradio-cc4db4.local", text: Binding(
                    get: { vm.hostInput },
                    set: { vm.hostInput = $0 }
                ))
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled()
                .keyboardType(.URL)
                .textContentType(.URL)

                if !vm.recentHosts.isEmpty {
                    ForEach(vm.recentHosts, id: \.self) { host in
                        Button {
                            Task { await vm.connect(to: host) }
                        } label: {
                            HStack {
                                Text(HostNormalizer.displayHost(from: host))
                                Spacer()
                                Image(systemName: "clock.arrow.circlepath")
                                    .foregroundStyle(.secondary)
                            }
                        }
                        .disabled(vm.isBusy)
                    }
                }

                LabeledContent("Stato", value: isLive ? "Connesso" : "Non connesso")
                LabeledContent(
                    "Host",
                    value: isLive
                        ? HostNormalizer.displayHost(from: state.connection.host)
                        : "—"
                )

                if let error = vm.errorMessage ?? state.connection.lastError {
                    Text(error).foregroundStyle(.red)
                }
                if let reset = vm.wifiResetMessage {
                    Text(reset)
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
            }

            Section {
                if isLive {
                    Button("Disconnetti", role: .destructive) {
                        vm.disconnect()
                    }
                } else {
                    Button {
                        Task { await vm.connect() }
                    } label: {
                        if vm.isBusy {
                            HStack {
                                IGIScanningIndicator().frame(width: 24, height: 24)
                                Text("Connessione...")
                            }
                        } else {
                            Text("Connetti")
                        }
                    }
                    .disabled(vm.isBusy)
                }
            }

            if isLive {
                Section {
                    Button("Dimentica Wi‑Fi e torna al setup", role: .destructive) {
                        confirmWifiReset = true
                    }
                    .disabled(vm.isBusy)
                } footer: {
                    Text("Cancella le credenziali salvate e riavvia la scheda in SoftAP (igiRadio-seriale, 192.168.4.1 o hostname.local).")
                }
            }

            Section {
                Text("iPhone e scheda sulla stessa Wi‑Fi. Alla prima volta consenti Rete locale. Se .local non risponde, usa l’IP.")
            }
            .font(.footnote)
            .foregroundStyle(.secondary)

            Section {
                Toggle("Modalità demo", isOn: Binding(
                    get: { environment.useMockDevice },
                    set: { vm.toggleMock($0) }
                ))
            } footer: {
                Text("Solo per prova senza scheda.")
            }
        }
    }

    private func startDiscovery(_ vm: ConnectionViewModel) {
        vm.startDiscovery()
        discoveryTimer?.invalidate()
        discoveryTimer = Timer.scheduledTimer(withTimeInterval: 2, repeats: true) { _ in
            vm.pollDiscovery()
        }
    }

    private func stopDiscovery(_ vm: ConnectionViewModel) {
        discoveryTimer?.invalidate()
        discoveryTimer = nil
        vm.stopDiscovery()
    }
}
