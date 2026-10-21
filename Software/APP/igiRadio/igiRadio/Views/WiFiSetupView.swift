import SwiftUI

struct WiFiSetupView: View {
    var onFinished: ((String?) -> Void)?
    @State private var viewModel = WiFiSetupViewModel()

    var body: some View {
        Form {
            switch viewModel.step {
            case .findDevice:
                bleDiscoverySection
            case .configureWifi:
                if let name = viewModel.selectedDeviceName {
                    Section("igiRadio") {
                        Label(name, systemImage: "checkmark.circle.fill")
                            .foregroundStyle(.green)
                    }
                }
                scanSection
                if let ssid = viewModel.selectedSSID {
                    credentialsSection(ssid: ssid)
                }
            case let .done(rebootSeconds, expectedHost):
                successSection(rebootSeconds: rebootSeconds, expectedHost: expectedHost)
            }

            if let error = viewModel.errorMessage {
                Section {
                    Text(error).foregroundStyle(.red)
                }
            }
            if let status = viewModel.statusMessage {
                Section {
                    Text(status).font(.footnote).foregroundStyle(.secondary)
                }
            }
        }
        .navigationTitle("Configura Wi‑Fi")
        .navigationBarTitleDisplayMode(.inline)
        .onAppear { viewModel.onAppear() }
        .onDisappear { viewModel.cleanup() }
    }

    @ViewBuilder
    private var bleDiscoverySection: some View {
        Section {
            Text("Resta sulla Wi‑Fi di casa. Il BLE dell’ESP32 trova la scheda (igiRadio-seriale) e le invia le credenziali. Non è l’altoparlante BT1035.")
                .font(.footnote)
                .foregroundStyle(.secondary)
        }

        Section("1. Trova DigiRadio") {
            if viewModel.isSearchingBLE || viewModel.isConnecting {
                HStack {
                    IGIScanningIndicator().frame(width: 28, height: 28)
                    Text(viewModel.isConnecting ? "Connessione alla scheda…" : "Ricerca BLE (ESP32)…")
                }
            }

            if viewModel.bleDevices.isEmpty, !viewModel.isSearchingBLE, !viewModel.isConnecting {
                Text("Il DigiRadio deve essere acceso in setup mode (dopo reset o primo avvio).")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            } else if viewModel.bleDevices.count > 1 {
                ForEach(viewModel.bleDevices) { device in
                    Button {
                        viewModel.selectAndConnect(deviceName: device.name)
                    } label: {
                        HStack {
                            VStack(alignment: .leading, spacing: 4) {
                                Text(device.name).font(.headline)
                                if viewModel.isConnecting, viewModel.selectedDeviceName == device.name {
                                    Text("Connessione…")
                                        .font(.caption)
                                        .foregroundStyle(.secondary)
                                }
                            }
                            Spacer()
                            if viewModel.selectedDeviceName == device.name, viewModel.isConnecting {
                                IGIScanningIndicator().frame(width: 22, height: 22)
                            } else if viewModel.selectedDeviceName == device.name {
                                Image(systemName: "checkmark.circle.fill")
                                    .foregroundStyle(IGITheme.accent)
                            }
                        }
                    }
                    .disabled(viewModel.isConnecting || viewModel.isProvisioning)
                }
            }

            if !viewModel.isSearchingBLE, !viewModel.isConnecting {
                Button("Cerca di nuovo") {
                    viewModel.searchBLEDevices()
                }
            }
        }
    }

    @ViewBuilder
    private var scanSection: some View {
        Section("2. Reti Wi‑Fi disponibili") {
            Button {
                viewModel.scanWifiNetworks()
            } label: {
                if viewModel.isScanningWifi {
                    HStack {
                        IGIScanningIndicator().frame(width: 24, height: 24)
                        Text("Scansione (~5 s)…")
                    }
                } else {
                    Label("Scansiona reti", systemImage: "wifi.circle")
                }
            }
            .disabled(viewModel.isScanningWifi || viewModel.isProvisioning)

            if viewModel.networks.isEmpty, !viewModel.isScanningWifi {
                Text("Il DigiRadio scansiona le reti vicine a sé — premi «Scansiona reti».")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            }

            ForEach(viewModel.networks) { network in
                Button {
                    viewModel.selectNetwork(network)
                } label: {
                    HStack {
                        VStack(alignment: .leading, spacing: 4) {
                            Text(network.ssid).font(.headline)
                            Text(wifiMeta(network))
                                .font(.caption)
                                .foregroundStyle(.secondary)
                        }
                        Spacer()
                        if viewModel.selectedSSID == network.ssid {
                            Image(systemName: "checkmark.circle.fill")
                                .foregroundStyle(IGITheme.accent)
                        }
                    }
                }
            }
        }
    }

    @ViewBuilder
    private func credentialsSection(ssid: String) -> some View {
        Section("3. Password") {
            LabeledContent("Rete", value: ssid)

            let isOpen = viewModel.networks.first(where: { $0.ssid == ssid })?.auth == "open"
            if isOpen {
                Text("Rete aperta — nessuna password.")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            } else {
                SecureField("Password della tua rete", text: $viewModel.password)
                    .textContentType(.password)
            }

            Button {
                viewModel.provisionSelectedNetwork()
            } label: {
                if viewModel.isProvisioning {
                    HStack {
                        IGIScanningIndicator().frame(width: 24, height: 24)
                        Text("Invio…")
                    }
                } else {
                    Text("Connetti")
                        .fontWeight(.semibold)
                }
            }
            .disabled(viewModel.isProvisioning)
        }
    }

    @ViewBuilder
    private func successSection(rebootSeconds: Int, expectedHost: String?) -> some View {
        Section {
            Label("Wi‑Fi configurato", systemImage: "checkmark.seal.fill")
                .foregroundStyle(.green)
            Text("Il DigiRadio si riavvia (~\(rebootSeconds) s) e si unisce alla tua rete.")
            if let onFinished {
                Button("Continua") {
                    onFinished(expectedHost)
                }
                .fontWeight(.semibold)
            } else if let host = expectedHost {
                Text("Poi la radio sarà su \(HostNormalizer.displayHost(from: host)).")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            }
        }
    }

    private func wifiMeta(_ network: WiFiProvisioningClient.ScannedNetwork) -> String {
        var parts: [String] = ["\(network.rssiDbm) dBm", network.auth]
        if let channel = network.channel {
            parts.append("ch \(channel)")
        }
        return parts.joined(separator: " · ")
    }
}
