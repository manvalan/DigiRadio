import SwiftUI

struct DABRadioView: View {
    @Environment(AppEnvironment.self) private var environment
    @State private var freqIndex: Double = 12
    @State private var services: [DABService] = []
    @State private var loadError: String?

    var body: some View {
        let dab = environment.state.tuner.dab
        Form {
            Section("Ensemble") {
                Stepper("Freq index: \(Int(freqIndex))", value: Binding(
                    get: { Int(freqIndex) },
                    set: { freqIndex = Double($0) }
                ), in: 0 ... 37)
                Button("Sintonizza ensemble") {
                    Task { try? await environment.digiRadio.tuneDAB(freqIndex: Int(freqIndex)) }
                }
            }

            if let dab {
                Section("Stato DAB") {
                    LabeledContent("Freq index", value: "\(dab.freqIndex)")
                    if let fic = dab.ficQuality { LabeledContent("FIC quality", value: "\(fic)") }
                    if let cnr = dab.cnrDb { LabeledContent("CNR", value: "\(cnr) dB") }
                    if let label = dab.dynamicLabel { LabeledContent("DLS", value: label) }
                }
            }

            Section("Servizi") {
                if let loadError {
                    Text(loadError).foregroundStyle(.red)
                }
                if services.isEmpty {
                    Text("Nessun servizio caricato")
                        .foregroundStyle(.secondary)
                } else {
                    ForEach(services) { service in
                        Button {
                            Task {
                                try? await environment.digiRadio.playDAB(
                                    serviceId: service.serviceId,
                                    componentId: service.componentId
                                )
                            }
                        } label: {
                            VStack(alignment: .leading) {
                                Text(service.label).font(.headline)
                                Text("SID \(service.serviceId) · CID \(service.componentId)")
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                            }
                        }
                    }
                }
                Button("Carica servizi") { Task { await loadServices() } }
            }
        }
        .navigationTitle("DAB")
        .onAppear {
            if let idx = environment.state.tuner.dab?.freqIndex {
                freqIndex = Double(idx)
            }
            Task { try? await environment.digiRadio.refreshTunerStatus() }
        }
    }

    private func loadServices() async {
        loadError = nil
        do {
            services = try await environment.digiRadio.listDABServices()
        } catch {
            loadError = error.localizedDescription
        }
    }
}
