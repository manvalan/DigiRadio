import SwiftUI

struct PresetsView: View {
    @Environment(AppEnvironment.self) private var environment
    @State private var newName = ""
    @State private var newBand: TunerBand = .fm
    @State private var newFMKhz = "102300"

    var body: some View {
        List {
            Section {
                ForEach(Array(environment.state.stations.enumerated()), id: \.offset) { index, station in
                    HStack {
                        VStack(alignment: .leading) {
                            Text(station.name).font(.headline)
                            Text(detail(for: station)).font(.caption).foregroundStyle(.secondary)
                        }
                        Spacer()
                        Button("Play") {
                            Task { try? await environment.digiRadio.tuneStation(at: index) }
                        }
                        .buttonStyle(.bordered)
                    }
                }
                .onDelete { offsets in
                    for index in offsets {
                        Task { try? await environment.digiRadio.removeStation(at: index) }
                    }
                }
                .onMove { from, to in
                    guard let source = from.first else { return }
                    Task { try? await environment.digiRadio.reorderStation(from: source, to: to) }
                }
            }

            Section("Aggiungi preset") {
                TextField("Nome", text: $newName)
                Picker("Banda", selection: $newBand) {
                    ForEach(TunerBand.allCases, id: \.self) { band in
                        Text(band.rawValue.uppercased()).tag(band)
                    }
                }
                if newBand == .fm {
                    TextField("Frequenza kHz", text: $newFMKhz)
                        .keyboardType(.numberPad)
                }
                Button("Salva") { Task { await savePreset() } }
                    .disabled(newName.isEmpty)
            }
        }
        .navigationTitle("Preset")
        .toolbar { EditButton() }
        .onAppear { Task { try? await environment.digiRadio.refreshStations() } }
    }

    private func detail(for station: Station) -> String {
        switch station.band {
        case .fm:
            if let khz = station.fmFrequencyKhz {
                return String(format: "FM %.2f MHz", Double(khz) / 1000)
            }
            return "FM"
        case .dab:
            return "DAB index \(station.dabFreqIndex ?? 0)"
        }
    }

    private func savePreset() async {
        let station: Station
        switch newBand {
        case .fm:
            guard let khz = Int(newFMKhz) else { return }
            station = Station(name: newName, band: .fm, fmFrequencyKhz: khz)
        case .dab:
            station = Station(
                name: newName,
                band: .dab,
                dabFreqIndex: Int(environment.state.tuner.dab?.freqIndex ?? 0),
                dabServiceId: environment.state.tuner.dab?.playingServiceId,
                dabComponentId: environment.state.tuner.dab?.playingComponentId
            )
        }
        try? await environment.digiRadio.saveStation(station)
        newName = ""
    }
}
