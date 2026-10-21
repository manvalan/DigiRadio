import SwiftUI

enum PrepareEntry: Hashable {
    case full
    case speaker
}

struct PrepareRadioView: View {
    var entry: PrepareEntry = .full

    @Environment(AppEnvironment.self) private var environment
    @Environment(\.dismiss) private var dismiss
    @State private var phase: Phase
    @State private var waitMessage = String(localized: "Attendi, la radio si collega alla rete…")
    @State private var waitError: String?

    private enum Phase: Equatable {
        case wifi
        case waiting(host: String?)
        case speaker
        case done
    }

    init(entry: PrepareEntry) {
        self.entry = entry
        _phase = State(initialValue: entry == .speaker ? .speaker : .wifi)
    }

    var body: some View {
        Group {
            switch phase {
            case .wifi:
                WiFiSetupView { host in
                    phase = .waiting(host: host)
                    Task { await joinAfterWifi(expectedHost: host) }
                }
            case .waiting:
                waitingView
            case .speaker:
                BluetoothView {
                    if entry == .speaker {
                        dismiss()
                    } else {
                        phase = .done
                    }
                }
            case .done:
                doneView
            }
        }
        .navigationTitle(title)
        .navigationBarTitleDisplayMode(.inline)
    }

    private var title: String {
        switch phase {
        case .wifi, .waiting: "Prepara igiRadio"
        case .speaker: "Altoparlante"
        case .done: "Pronto"
        }
    }

    private var waitingView: some View {
        Form {
            Section {
                HStack {
                    ProgressView()
                    Text(waitMessage)
                }
            }
            if let waitError {
                Section {
                    Text(waitError).foregroundStyle(.red)
                    Button("Riprova") {
                        if case let .waiting(host) = phase {
                            Task { await joinAfterWifi(expectedHost: host) }
                        }
                    }
                    Button("Salta, collega dopo") {
                        phase = .speaker
                    }
                }
            }
        }
    }

    private var doneView: some View {
        Form {
            Section {
                Label("igiRadio è pronto", systemImage: "checkmark.seal.fill")
                    .foregroundStyle(.green)
                if environment.state.connection.isConnected {
                    Text(HostNormalizer.displayHost(from: environment.state.connection.host))
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
                if let speaker = environment.state.bluetooth.savedSpeaker {
                    Text(speaker.name.isEmpty ? speaker.mac : speaker.name)
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
            }
            Section {
                Button("Chiudi") { dismiss() }
                    .fontWeight(.semibold)
            }
        }
    }

    private func joinAfterWifi(expectedHost: String?) async {
        waitError = nil
        waitMessage = String(localized: "Attendi, la radio si riavvia…")
        try? await Task.sleep(for: .seconds(4))
        waitMessage = String(localized: "Cerco la radio in rete…")
        var lastError: String?
        var candidates: [String] = []
        if let expectedHost, !expectedHost.isEmpty {
            candidates.append(contentsOf: DeviceIdentity.connectionCandidates(from: expectedHost))
        }
        for host in ConnectionPreferences.recentHosts {
            candidates.append(contentsOf: DeviceIdentity.connectionCandidates(from: host))
        }
        var seen = Set<String>()
        candidates = candidates.filter { seen.insert($0).inserted }
        for host in candidates.prefix(6) {
            do {
                try await environment.digiRadio.connect(host: host)
                try? await environment.digiRadio.refreshBluetoothStatus()
                phase = .speaker
                return
            } catch {
                lastError = DigiRadioFriendlyError.message(for: error)
            }
        }
        waitError = lastError ?? "Radio non trovata in rete. Riprova tra qualche secondo."
    }
}
