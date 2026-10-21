import SwiftUI

struct MyRadioView: View {
    @Environment(AppEnvironment.self) private var environment
    @State private var didProbe = false

    private var isLive: Bool {
        didProbe && environment.state.connection.isConnected
    }

    var body: some View {
        let speaker = environment.state.bluetooth.savedSpeaker
        List {
            Section {
                LabeledContent("Scheda", value: isLive ? "Connessa" : "Non connessa")
                if isLive {
                    Text(HostNormalizer.displayHost(from: environment.state.connection.host))
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
                LabeledContent(
                    "Altoparlante",
                    value: speakerLabel(speaker, live: isLive)
                )
            } footer: {
                Text("Due cose: rete di casa e altoparlante. Il resto è in Avanzate.")
            }

            if !isLive {
                Section {
                    NavigationLink {
                        PrepareRadioView(entry: .full)
                    } label: {
                        Label("Prepara igiRadio", systemImage: "sparkles")
                    }
                }
            } else {
                Section {
                    NavigationLink {
                        PrepareRadioView(entry: .full)
                    } label: {
                        Label("Cambia Wi‑Fi", systemImage: "wifi")
                    }
                }
            }

            Section {
                NavigationLink {
                    PrepareRadioView(entry: .speaker)
                } label: {
                    Label("Cambia altoparlante", systemImage: "hifispeaker.fill")
                }
            } footer: {
                Text("Riparte dal collegamento speaker. Serve quando ne aggiungi uno nuovo o dopo un reset del Bose.")
            }

            Section("Avanzate") {
                NavigationLink {
                    ConnectionView()
                } label: {
                    Label("Connessione manuale", systemImage: "link")
                }
                NavigationLink {
                    DeviceInfoView()
                } label: {
                    Label("Informazioni", systemImage: "info.circle")
                }
                NavigationLink {
                    DiagnosticsView()
                } label: {
                    Label("Diagnostica", systemImage: "stethoscope")
                }
                NavigationLink {
                    FirmwareView()
                } label: {
                    Label("Firmware", systemImage: "cpu")
                }
            }
        }
        .listStyle(.insetGrouped)
        .igiThemedScreen()
        .navigationTitle("La mia radio")
        .navigationBarTitleDisplayMode(.large)
        .onAppear {
            didProbe = false
            Task {
                _ = await environment.digiRadio.verifyReachability()
                if environment.state.connection.isConnected {
                    try? await environment.digiRadio.refreshBluetoothStatus()
                }
                didProbe = true
            }
        }
    }

    private func speakerLabel(_ speaker: SavedSpeaker?, live: Bool) -> String {
        guard live else { return "—" }
        guard let speaker else { return "Nessuno" }
        if speaker.name.isEmpty { return speaker.mac }
        return speaker.name
    }
}
