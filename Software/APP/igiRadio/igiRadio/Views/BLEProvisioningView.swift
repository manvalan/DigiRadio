import SwiftUI

struct BLEProvisioningView: View {
    @State private var service = BLEProvisioningService()

    var body: some View {
        List {
            Section("Ricerca") {
                switch service.phase {
                case .idle:
                    Text("Premi Avvia per cercare dispositivi DigiRadio in setup mode.")
                        .foregroundStyle(.secondary)
                case .scanning:
                    HStack {
                        IGIScanningIndicator().frame(width: 28, height: 28)
                        Text("Cerca DigiRadio...")
                    }
                case let .found(name, _):
                    Label("Trovato: \(name)", systemImage: "checkmark.circle.fill")
                        .foregroundStyle(.green)
                case .unsupported:
                    Text("Provisioning BLE non ancora integrato in igiRadio.")
                case let .error(message):
                    Text(message).foregroundStyle(.red)
                }
            }

            if !service.discoveredDevices.isEmpty {
                Section("Dispositivi") {
                    ForEach(service.discoveredDevices, id: \.id) { device in
                        VStack(alignment: .leading) {
                            Text(device.name).font(.headline)
                            Text("RSSI \(device.rssi) dBm").font(.caption).foregroundStyle(.secondary)
                        }
                    }
                }
            }

            Section {
                Button("Avvia scansione") { service.startScan() }
                Button("Ferma scansione") { service.stopScan() }
            }

            Section("Istruzioni") {
                Text("1. Metti DigiRadio in setup mode (SoftAP DigiRadio-<suffix>).")
                Text("2. Il dispositivo espone anche BLE provisioning ESP-IDF.")
                Text("3. Proof of Possession (PoP) = serial number del dispositivo.")
                Text("4. Dopo il join Wi‑Fi, usa la connessione HTTP in igiRadio.")
            }
            .font(.footnote)
            .foregroundStyle(.secondary)

            Section {
                Text("UNKNOWN — specification required: implementazione completa protocomm Security1 in-app (attualmente consigliata app ESP BLE Provisioning di Espressif).")
                    .font(.footnote)
                    .foregroundStyle(.orange)
            }
        }
        .navigationTitle("BLE Provisioning")
        .onDisappear { service.stopScan() }
    }
}
