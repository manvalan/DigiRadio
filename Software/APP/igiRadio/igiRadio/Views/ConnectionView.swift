import SwiftUI

struct ConnectionView: View {
    @Environment(AppEnvironment.self) private var environment
    @State private var viewModel: ConnectionViewModel?
    @State private var discoveryTimer: Timer?

    var body: some View {
        let vm = viewModel ?? ConnectionViewModel(environment: environment)
        let state = environment.state

        Form {
            Section("Dispositivo") {
                Toggle("Modalità demo (Mock)", isOn: Binding(
                    get: { environment.useMockDevice },
                    set: { vm.toggleMock($0) }
                ))
            }

            if !environment.useMockDevice {
                Section("Ricerca in rete") {
                    if vm.discovery.isSearching {
                        HStack {
                            IGIScanningIndicator().frame(width: 28, height: 28)
                            Text("Cerca DigiRadio...")
                        }
                        Button("Ferma ricerca") { stopDiscovery(vm) }
                    } else {
                        Button("Cerca dispositivi") { startDiscovery(vm) }
                    }

                    if vm.discovery.devices.isEmpty {
                        Text("Avvia la ricerca per trovare DigiRadio via BLE (setup) o verificare l'host inserito.")
                            .font(.footnote)
                            .foregroundStyle(.secondary)
                    } else {
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
                                        Image(systemName: "questionmark.circle").foregroundStyle(.orange)
                                    }
                                }
                            }
                            .disabled(vm.isBusy)
                        }
                    }
                }
            }

            Section("Connessione HTTP") {
                TextField("Host", text: Binding(
                    get: { vm.hostInput },
                    set: { vm.hostInput = $0 }
                ))
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled()
                .keyboardType(.URL)

                if state.connection.isConnected {
                    LabeledContent("Stato", value: "Connesso")
                    LabeledContent("Host", value: state.connection.host)
                }

                if let error = vm.errorMessage ?? state.connection.lastError {
                    Text(error).foregroundStyle(.red)
                }
            }

            Section {
                if state.connection.isConnected {
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

            Section("Provisioning Wi‑Fi (BLE)") {
                NavigationLink("Configura Wi‑Fi via BLE") {
                    BLEProvisioningView()
                }
                Text("DigiRadio espone BLE solo per il provisioning Wi‑Fi. Il nome BLE (es. DigiRadio-CC4DB4) corrisponde all'host HTTP digiradio-cc4db4.local.")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            }

            Section("Suggerimenti host") {
                Text("• Setup mode: http://192.168.4.1")
                Text("• STA mode: http://digiradio-<suffix>.local")
                Text("• Oppure IP LAN del dispositivo")
            }
            .font(.footnote)
            .foregroundStyle(.secondary)
        }
        .navigationTitle("Connessione")
        .onAppear {
            if viewModel == nil { viewModel = vm }
            if environment.useMockDevice && !state.connection.isConnected {
                Task { await vm.connect() }
            }
        }
        .onDisappear { stopDiscovery(vm) }
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
