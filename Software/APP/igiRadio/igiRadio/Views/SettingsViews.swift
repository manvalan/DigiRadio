import SwiftUI

struct SettingsRootView: View {
    var body: some View {
        List {
            Section("DigiRadio") {
                NavigationLink("Connessione") { ConnectionView() }
                NavigationLink("Informazioni dispositivo") { DeviceInfoView() }
                NavigationLink("Diagnostica") { DiagnosticsView() }
                NavigationLink("Firmware") { FirmwareView() }
            }
            Section("Radio") {
                NavigationLink("FM") { FMRadioView() }
                NavigationLink("DAB") { DABRadioView() }
                NavigationLink("Preset") { PresetsView() }
            }
            Section("Audio") {
                NavigationLink("Profilo audio") { AudioView() }
                NavigationLink("Bluetooth speaker") { BluetoothView() }
            }
        }
        .navigationTitle("Impostazioni")
    }
}

struct DeviceInfoView: View {
    @Environment(AppEnvironment.self) private var environment

    var body: some View {
        let health = environment.state.health
        let chips = health.chips
        Form {
            LabeledContent("Nome", value: "DigiRadio")
            LabeledContent("Firmware", value: health.firmware.isEmpty ? "—" : health.firmware)
            LabeledContent("Serial number", value: health.serialNumber.isEmpty ? "—" : health.serialNumber)
            LabeledContent("Stato", value: health.status.isEmpty ? "—" : health.status)
            Section("Chip") {
                LabeledContent("Si4684", value: chips.si4684 ? "OK" : "—")
                LabeledContent("ADAU1701", value: chips.adau1701 ? "OK" : "—")
                LabeledContent("BT1035", value: chips.bt1035 ? "OK" : "—")
            }
            Section("Connessione") {
                LabeledContent("Host", value: environment.state.connection.host.isEmpty ? "—" : environment.state.connection.host)
                LabeledContent("HTTP", value: environment.state.connection.isConnected ? "Connesso" : "Non connesso")
            }
        }
        .navigationTitle("Dispositivo")
        .onAppear { Task { try? await environment.digiRadio.refreshHealth() } }
    }
}

struct DiagnosticsView: View {
    @Environment(AppEnvironment.self) private var environment

    var body: some View {
        Form {
            Section("Tuner") {
                LabeledContent("Booted", value: environment.state.tuner.booted ? "Sì" : "No")
                LabeledContent("Banda", value: environment.state.tuner.band.rawValue.uppercased())
                LabeledContent("Locked", value: environment.state.tuner.locked ? "Sì" : "No")
            }
            Section("Bluetooth") {
                LabeledContent("A2DP", value: environment.state.bluetooth.a2dpState.rawValue)
                LabeledContent("Pairing", value: environment.state.bluetooth.pairing ? "Sì" : "No")
            }
            Section {
                Button("Aggiorna snapshot") {
                    Task {
                        try? await environment.digiRadio.refreshHealth()
                        try? await environment.digiRadio.refreshTunerStatus()
                        try? await environment.digiRadio.refreshBluetoothStatus()
                    }
                }
            }
        }
        .navigationTitle("Diagnostica")
    }
}

struct FirmwareView: View {
    @Environment(AppEnvironment.self) private var environment

    var body: some View {
        Form {
            Section("Versione corrente") {
                LabeledContent("Firmware", value: environment.state.health.firmware.isEmpty ? "—" : environment.state.health.firmware)
            }
            Section("Aggiornamento OTA") {
                Text("L'aggiornamento firmware è supportato via POST /api/system/ota con un file .bin ESP-IDF valido. L'interfaccia di upload file sarà aggiunta in una versione successiva.")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                Text("UNKNOWN — specification required: progress reporting durante OTA stream.")
                    .font(.footnote)
                    .foregroundStyle(.orange)
            }
        }
        .navigationTitle("Firmware")
        .onAppear { Task { try? await environment.digiRadio.refreshHealth() } }
    }
}
