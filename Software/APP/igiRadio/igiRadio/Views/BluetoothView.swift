import SwiftUI

struct BluetoothView: View {
    var onFinished: (() -> Void)?
    @Environment(AppEnvironment.self) private var environment
    @State private var viewModel: BluetoothViewModel?
    @State private var confirmDisable = false
    @State private var didProbe = false

    var body: some View {
        Group {
            if let viewModel {
                speakerForm(vm: viewModel)
            } else {
                ProgressView()
            }
        }
        .navigationTitle("Altoparlante")
        .onAppear {
            if viewModel == nil {
                viewModel = BluetoothViewModel(service: environment.digiRadio)
            }
            Task {
                didProbe = false
                _ = await environment.digiRadio.verifyReachability()
                await viewModel?.refresh()
                didProbe = true
            }
        }
        .confirmationDialog(
            "Spegnere il modulo audio?",
            isPresented: $confirmDisable,
            titleVisibility: .visible
        ) {
            Button("Spegni", role: .destructive) {
                Task { await viewModel?.setModuleEnabled(false) }
            }
            Button("Annulla", role: .cancel) {}
        } message: {
            Text("Si spegne solo il BT1035 (uscita verso l’altoparlante). Il BLE ESP32 del setup Wi‑Fi resta a posto.")
        }
    }

    @ViewBuilder
    private func speakerForm(@Bindable vm: BluetoothViewModel) -> some View {
        let bt = vm.bluetooth
        let isLive = didProbe && environment.state.connection.isConnected
        let moduleOn = isLive && bt.booted
        Form {
            Section {
                Text("Collega un altoparlante al modulo audio della radio (BT1035). Non è il BLE usato per configurare il Wi‑Fi.")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            }

            if !isLive {
                Section {
                    Text("Prima collega la scheda da Configurazione → Connessione.")
                        .foregroundStyle(.secondary)
                }
            }

            Section {
                LabeledContent("Scheda", value: isLive ? "Connessa" : "Non connesso")
                LabeledContent("Modulo audio", value: moduleLabel(isLive: isLive, booted: bt.booted))
                if isLive {
                    if bt.booted {
                        Button("Spegni modulo audio", role: .destructive) {
                            confirmDisable = true
                        }
                        .disabled(vm.isBusy || vm.isScanning)
                    } else {
                        Button {
                            Task { await vm.setModuleEnabled(true) }
                        } label: {
                            if vm.isBusy {
                                HStack {
                                    ProgressView()
                                    Text("Accensione…")
                                }
                            } else {
                                Label("Accendi modulo audio", systemImage: "power")
                            }
                        }
                        .disabled(vm.isBusy)
                    }
                }
            } header: {
                Text("BT1035")
            }

            if moduleOn, let speaker = bt.savedSpeaker {
                Section("Salvato") {
                    LabeledContent("Nome", value: speaker.name.isEmpty ? "Altoparlante" : speaker.name)
                    Text(speaker.mac)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    LabeledContent("A2DP", value: bt.a2dpState.rawValue.capitalized)
                    Button {
                        Task { await vm.reconnect() }
                    } label: {
                        if vm.isReconnecting {
                            HStack {
                                ProgressView()
                                Text("Riconnessione…")
                            }
                        } else {
                            Text("Riconnetti")
                        }
                    }
                    .disabled(vm.isScanning || vm.isConnecting || vm.isReconnecting || vm.isBusy)
                    Button("Disconnetti") {
                        Task { await vm.disconnect() }
                    }
                    .disabled(vm.isBusy || vm.isScanning)
                    Button("Dimentica altoparlante", role: .destructive) {
                        Task { await vm.clearSavedSpeaker() }
                    }
                    .disabled(vm.isBusy)
                }
            } else if moduleOn {
                Section("Salvato") {
                    LabeledContent("A2DP", value: bt.a2dpState.rawValue.capitalized)
                    Text("Nessun altoparlante in memoria.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
            }

            Section {
                TextField("MAC (000C8A78AB62)", text: $vm.manualMac)
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled()
                .disabled(!moduleOn)
                TextField("Nome (es. Bose)", text: $vm.manualName)
                .disabled(!moduleOn)
                Button {
                    Task { await vm.connectManual() }
                } label: {
                    if vm.isConnecting {
                        HStack {
                            ProgressView()
                            Text("Connessione…")
                        }
                    } else {
                        Text("Collega questo MAC")
                    }
                }
                .disabled(!moduleOn || vm.isConnecting || vm.isScanning || vm.isBusy)
            } header: {
                Text("Collega (Bose e soundbar)")
            } footer: {
                Text("Metti l’altoparlante in pairing, spegni il Bluetooth del telefono se è già agganciato, poi collega. Lo scan spesso non vede i Bose.")
            }

            Section {
                Button {
                    Task { await vm.scan() }
                } label: {
                    if vm.isScanning {
                        HStack {
                            ProgressView()
                            Text("Ricerca…")
                        }
                    } else {
                        Label("Cerca altri altoparlanti", systemImage: "antenna.radiowaves.left.and.right")
                    }
                }
                .disabled(!moduleOn || vm.isScanning || vm.isConnecting || vm.isReconnecting || vm.isBusy)
            }

            if moduleOn, !bt.nearbyDevices.isEmpty {
                Section("Trovati") {
                    ForEach(bt.nearbyDevices) { device in
                        Button {
                            Task { await vm.connect(device: device) }
                        } label: {
                            VStack(alignment: .leading, spacing: 2) {
                                Text(device.name.isEmpty ? "Altoparlante" : device.name)
                                Text(device.mac)
                                    .font(.footnote)
                                    .foregroundStyle(.secondary)
                            }
                        }
                        .disabled(vm.isConnecting || vm.isScanning)
                    }
                }
            }

            if moduleOn {
                DisclosureGroup("Codec (avanzate)") {
                    Text("SBC è sempre disponibile.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                    ForEach(A2DPCodecMask.allCases) { option in
                        Toggle(option.label, isOn: Binding(
                            get: { vm.enabledCodecOptions.contains(option) },
                            set: { _ in vm.toggleCodecOption(option) }
                        ))
                    }
                    Button {
                        Task { await vm.saveCodecPreferences() }
                    } label: {
                        if vm.isSavingCodec {
                            HStack {
                                ProgressView()
                                Text("Salvataggio…")
                            }
                        } else {
                            Text("Salva codec")
                        }
                    }
                    .disabled(vm.isSavingCodec || vm.isBusy || vm.isScanning)
                }
            }

            if let error = vm.errorMessage {
                Section {
                    Text(error).foregroundStyle(.red)
                }
            }
            if let status = vm.statusMessage {
                Section {
                    Text(status).font(.footnote).foregroundStyle(.secondary)
                }
            }

            if let onFinished {
                Section {
                    Button("Fatto") { onFinished() }
                        .fontWeight(.semibold)
                        .disabled(vm.isConnecting || vm.isScanning || vm.isBusy)
                } footer: {
                    Text("Puoi chiudere anche se l’altoparlante è già collegato, o ritentare dopo.")
                }
            }
        }
    }

    private func moduleLabel(isLive: Bool, booted: Bool) -> String {
        guard isLive else { return "—" }
        return booted ? "Acceso" : "Spento"
    }
}
