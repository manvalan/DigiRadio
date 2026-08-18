import SwiftUI

struct BluetoothView: View {
    @Environment(AppEnvironment.self) private var environment
    @State private var isScanning = false

    var body: some View {
        let bt = environment.state.bluetooth
        Form {
            Section("Stato speaker (BT1035)") {
                LabeledContent("Modulo", value: bt.booted ? "Attivo" : "Non pronto")
                LabeledContent("A2DP", value: bt.a2dpState.rawValue.capitalized)
                LabeledContent("Dispositivo", value: bt.deviceName.isEmpty ? "—" : bt.deviceName)
                if let speaker = bt.savedSpeaker {
                    LabeledContent("Speaker salvato", value: speaker.name)
                    Text(speaker.mac).font(.caption).foregroundStyle(.secondary)
                }
            }

            Section("Azioni") {
                Button(isScanning ? "Scansione..." : "Scansiona speaker vicini") {
                    Task { await scan() }
                }
                .disabled(isScanning)

                Button("Riconnetti speaker salvato") {
                    Task { try? await environment.digiRadio.reconnectBluetooth() }
                }
            }

            if !bt.nearbyDevices.isEmpty {
                Section("Dispositivi trovati") {
                    ForEach(bt.nearbyDevices) { device in
                        HStack {
                            VStack(alignment: .leading) {
                                Text(device.name).font(.headline)
                                Text(device.mac).font(.caption).foregroundStyle(.secondary)
                            }
                            Spacer()
                            Text("\(device.rssiDbm) dBm")
                                .font(.caption)
                                .foregroundStyle(.secondary)
                            Button("Connetti") {
                                Task {
                                    try? await environment.digiRadio.connectBluetooth(
                                        mac: device.mac,
                                        name: device.name,
                                        save: true
                                    )
                                }
                            }
                            .buttonStyle(.borderedProminent)
                        }
                    }
                }
            }

            Section {
                Text("Questa sezione configura il modulo Bluetooth classic (BT1035) che invia audio A2DP verso un altoparlante esterno. È distinta dalla connessione BLE usata solo per il provisioning Wi‑Fi.")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            }
        }
        .navigationTitle("Bluetooth Audio")
        .onAppear { Task { try? await environment.digiRadio.refreshBluetoothStatus() } }
    }

    private func scan() async {
        isScanning = true
        defer { isScanning = false }
        try? await environment.digiRadio.scanBluetooth(seconds: 8)
    }
}
