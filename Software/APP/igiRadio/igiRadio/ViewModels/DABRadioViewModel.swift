import Foundation
import Observation

@Observable
final class DABRadioViewModel {
    private let service: any DigiRadioService
    private var refreshTask: Task<Void, Never>?

    /// Picker locale — non sovrascritto dal polling HTTP.
    var freqIndex: Int = 12
    var services: [DABService] = []
    var selectedServiceID = ""
    var isLoadingServices = false
    var isTuning = false
    var isPlaying = false
    var statusMessage: String?
    var errorMessage: String?

    init(service: any DigiRadioService) {
        self.service = service
    }

    var dabState: DABTunerState? { service.state.tuner.dab }
    var isConnected: Bool { service.state.connection.isConnected }
    var isLocked: Bool { service.state.tuner.locked }

    /// Canale effettivamente sintonizzato sul device (solo lettura).
    var deviceFreqIndex: Int? { dabState?.freqIndex }

    var selectedService: DABService? {
        services.first { $0.id == selectedServiceID }
    }

    var nowPlayingLabel: String {
        if let label = dabState?.dynamicLabel, !label.isEmpty { return label }
        if let selected = selectedService { return selected.label }
        return "Nessuna stazione"
    }

    func onAppear() {
        importDeviceStateOnce()
        refreshTask?.cancel()
        refreshTask = Task {
            await bootstrap()
            while !Task.isCancelled {
                try? await Task.sleep(for: .seconds(3))
                guard service.state.connection.isConnected else { continue }
                guard !service.isBackgroundRefreshPaused else { continue }
                guard !isTuning, !isPlaying else { continue }
                await refreshTunerStatusOnly()
            }
        }
    }

    func onDisappear() {
        refreshTask?.cancel()
        refreshTask = nil
    }

    private func bootstrap() async {
        guard isConnected else { return }
        await refreshTunerStatusOnly()
        if service.state.tuner.band == .dab, isLocked {
            await loadServices(selectCurrent: true)
        }
    }

    /// Importa ensemble/stazione dal device solo al primo caricamento.
    private func importDeviceStateOnce() {
        if let idx = service.state.tuner.dab?.freqIndex {
            freqIndex = idx
        }
        if let sid = service.state.tuner.dab?.playingServiceId,
           let cid = service.state.tuner.dab?.playingComponentId {
            selectedServiceID = "\(sid)-\(cid)"
        }
    }

    /// Aggiorna lo snapshot tuner senza toccare i picker (evita il salto al canale 22).
    private func refreshTunerStatusOnly() async {
        do {
            try await service.refreshTunerStatus()
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }

    func tuneEnsemble() async {
        guard isConnected else {
            errorMessage = "Connetti a DigiRadio."
            return
        }
        isTuning = true
        errorMessage = nil
        statusMessage = "Sintonizzazione ensemble \(freqIndex)…"
        defer { isTuning = false }
        do {
            try await service.tuneDAB(freqIndex: freqIndex)
            if let idx = service.state.tuner.dab?.freqIndex {
                freqIndex = idx
            }
            selectedServiceID = ""
            services = []
            await loadServices(selectCurrent: false)
            if services.isEmpty {
                statusMessage = service.state.tuner.locked
                    ? "Ensemble \(freqIndex) ok — nessun servizio in elenco, prova un altro canale."
                    : "Ensemble \(freqIndex) senza segnale — prova un altro canale."
            } else {
                statusMessage = "Ensemble \(freqIndex): \(services.count) stazioni trovate."
            }
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }

    func playSelected() async {
        guard let service = selectedService else {
            errorMessage = "Seleziona una stazione dal menu."
            return
        }
        await play(service)
    }

    func play(_ dabService: DABService) async {
        guard isConnected else {
            errorMessage = "Connetti a DigiRadio."
            return
        }
        isPlaying = true
        errorMessage = nil
        let chosenID = dabService.id
        defer { isPlaying = false }
        do {
            try await service.playDAB(serviceId: dabService.serviceId, componentId: dabService.componentId)
            selectedServiceID = chosenID
            try await service.refreshTunerStatus()
            statusMessage = "In riproduzione: \(dabService.label)"
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }

    func loadServices(selectCurrent: Bool = false) async {
        guard isConnected else {
            errorMessage = "Connetti a DigiRadio."
            return
        }
        isLoadingServices = true
        errorMessage = nil
        defer { isLoadingServices = false }
        do {
            services = try await service.listDABServices()
            if services.isEmpty {
                statusMessage = "Nessun servizio su questo ensemble — prova un altro canale."
            } else if selectCurrent,
                      let sid = dabState?.playingServiceId,
                      let cid = dabState?.playingComponentId,
                      services.contains(where: { $0.id == "\(sid)-\(cid)" }) {
                selectedServiceID = "\(sid)-\(cid)"
            } else if !services.contains(where: { $0.id == selectedServiceID }),
                      let first = services.first {
                selectedServiceID = first.id
            }
        } catch {
            let msg = DigiRadioFriendlyError.message(for: error)
            if msg.localizedCaseInsensitiveContains("servizi dab") {
                statusMessage = "Nessun servizio leggibile su questo ensemble."
                errorMessage = nil
            } else {
                errorMessage = msg
            }
            services = []
        }
    }
}
