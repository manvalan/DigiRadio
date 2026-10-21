import Foundation
import Observation

enum StationScanBand: String, CaseIterable, Identifiable, Codable {
    case fm = "FM"
    case dab = "DAB"

    var id: String { rawValue }

    var tunerBand: TunerBand {
        switch self {
        case .fm: .fm
        case .dab: .dab
        }
    }
}

/// Gestisce scan, preferiti e il catalogo stazioni sull'iPhone (il DigiRadio ha solo 20 preset).
@Observable
final class StationScanViewModel {
    private static let minDabFicQuality = 35

    var selectedBand: StationScanBand = .fm {
        didSet {
            guard oldValue != selectedBand, isScanning else { return }
            scanTask?.cancel()
        }
    }
    var isScanning = false
    var progressMessage: String?
    var errorMessage: String?
    var statusMessage: String?
    var favoriteStations: [ScannedStation] = []
    var scannedFM: [ScannedStation] = []
    var scannedDAB: [ScannedStation] = []
    var namingStation: ScannedStation?
    var namingText = ""
    var listeningStationID: String?

    private var service: any DigiRadioService
    private var scanTask: Task<Void, Never>?
    private var afMonitorTask: Task<Void, Never>?
    private var afStationID: String?
    private var lastAFHuntAt: Date?

    init(service: any DigiRadioService) {
        self.service = service
        self.favoriteStations = FMStationMerge.merge(
            Array(FavoriteStationsStore.load().prefix(DigiRadioPresetLimits.maxPresets))
        )
        let stored = ScannedStationStore.load()
        self.scannedFM = FMStationMerge.merge(stored.fm)
        self.scannedDAB = stored.dab
    }

    deinit {
        scanTask?.cancel()
        afMonitorTask?.cancel()
    }

    func replaceService(_ service: any DigiRadioService) {
        self.service = service
    }

    /// Preset hardware sul device (max 20). Non è il catalogo dell'app.
    var deviceStations: [Station] {
        service.state.stations
    }

    var catalogCount: Int { scannedFM.count + scannedDAB.count }

    var listedStations: [ScannedStation] { scannedCatalog }

    /// Catalogo completo della banda selezionata, solo sull'iPhone, senza tetto a 20.
    var results: [Station] {
        listedStations.map { $0.asPreset() }
    }

    private var scannedCatalog: [ScannedStation] {
        selectedBand == .fm ? scannedFM : scannedDAB
    }

    var hasCachedResults: Bool {
        !scannedFM.isEmpty || !scannedDAB.isEmpty
    }

    var isConnected: Bool { service.state.connection.isConnected }

    var filteredFavorites: [ScannedStation] {
        favoriteStations
            .filter { $0.band == selectedBand.tunerBand }
            .sorted { $0.name.localizedCaseInsensitiveCompare($1.name) == .orderedAscending }
    }

    var favoriteCount: Int { filteredFavorites.count }

    func isTuned(to station: ScannedStation) -> Bool {
        guard service.state.audio.activeSource == .radio else { return false }
        switch station.band {
        case .fm:
            guard service.state.tuner.band == .fm,
                  let khz = service.state.tuner.fm?.frequencyKhz else { return false }
            return station.containsFMFrequency(khz)
        case .dab:
            guard service.state.tuner.band == .dab, let dab = service.state.tuner.dab else { return false }
            return station.dabFreqIndex == dab.freqIndex
                && (station.dabServiceId == nil || station.dabServiceId == dab.playingServiceId)
                && (station.dabComponentId == nil || station.dabComponentId == dab.playingComponentId)
        }
    }

    /// Nome da mostrare per la frequenza sintonizzata: RDS live, poi catalogo/preferiti.
    func displayName(forFMFrequencyKhz khz: Int, liveName: String?) -> String? {
        if let live = StationNameDisplay.meaningful(liveName, khz: khz) {
            return live
        }
        let match = favoriteStations.first { $0.band == .fm && $0.containsFMFrequency(khz) }
            ?? scannedFM.first { $0.containsFMFrequency(khz) }
            ?? deviceStations.first { $0.band == .fm && $0.fmFrequencyKhz == khz }.map { ScannedStation.from($0) }
        return StationNameDisplay.meaningful(match?.name, khz: khz)
    }

    func displayName(
        forDAB freqIndex: Int,
        serviceId: Int?,
        componentId: Int?,
        liveLabel: String?
    ) -> String? {
        let match = favoriteStations.first {
            $0.band == .dab
                && $0.dabFreqIndex == freqIndex
                && (serviceId == nil || $0.dabServiceId == serviceId)
                && (componentId == nil || $0.dabComponentId == componentId)
        } ?? scannedDAB.first {
            $0.dabFreqIndex == freqIndex
                && (serviceId == nil || $0.dabServiceId == serviceId)
                && (componentId == nil || $0.dabComponentId == componentId)
        }
        if let named = StationNameDisplay.meaningful(match?.name) {
            return named
        }
        return StationNameDisplay.meaningful(liveLabel)
    }

    /// Aggiorna nome RDS e frequenze alternative nella scheda della stazione
    /// (catalogo e preferiti).
    func ingestLiveFM(khz: Int, stationName: String?, afKhz: [Int] = []) {
        let extras = Self.validFMFrequencies(afKhz + [khz])
        let broadcastName = StationNameDisplay.meaningful(stationName, khz: khz)
        var catalogChanged = false
        var favoritesChanged = false

        if let index = scannedFM.firstIndex(where: { $0.containsFMFrequency(khz) }) {
            catalogChanged = mergeLiveFM(&scannedFM[index], name: broadcastName, extras: extras)
        } else if broadcastName != nil || extras.count > 1 {
            scannedFM.append(
                ScannedStation(
                    id: "fm-\(khz)",
                    name: broadcastName ?? String(format: "FM %.2f", Double(khz) / 1000),
                    band: .fm,
                    fmFrequencyKhz: khz,
                    dabFreqIndex: nil,
                    dabServiceId: nil,
                    dabComponentId: nil,
                    rssiDbuv: service.state.tuner.fm?.rssiDbuv,
                    fmFrequenciesKhz: extras,
                    hasBroadcastName: broadcastName != nil
                )
            )
            catalogChanged = true
        }
        if catalogChanged {
            scannedFM = FMStationMerge.merge(scannedFM)
            persistScanned()
        }

        if let index = favoriteStations.firstIndex(where: { $0.band == .fm && $0.containsFMFrequency(khz) }) {
            favoritesChanged = mergeLiveFM(&favoriteStations[index], name: broadcastName, extras: extras)
            if favoritesChanged {
                favoriteStations = FMStationMerge.merge(favoriteStations)
                persistFavorites()
            }
        }
    }

    func stationCarrying(fmKhz khz: Int) -> ScannedStation? {
        favoriteStations.first { $0.band == .fm && $0.containsFMFrequency(khz) }
            ?? scannedFM.first { $0.containsFMFrequency(khz) }
    }

    private func mergeLiveFM(_ station: inout ScannedStation, name: String?, extras: [Int]) -> Bool {
        var changed = false
        if let name, station.name != name {
            station.name = name
            station.hasBroadcastName = true
            changed = true
        }
        var freqs = Set(station.allFMFrequenciesKhz)
        let before = freqs
        freqs.formUnion(extras)
        if freqs != before {
            station.fmFrequenciesKhz = freqs.sorted()
            changed = true
        }
        return changed
    }

    private static func validFMFrequencies(_ khz: [Int]) -> [Int] {
        Array(Set(khz.filter { (87_500...108_000).contains($0) })).sorted()
    }


    func refreshLibrary() async {
        guard isConnected else { return }
        errorMessage = nil
        do {
            try await service.refreshStations()
            importDevicePresetsIfFavoritesEmpty()
            mergeDeviceStationsIntoCatalog()
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }

    /// Se in app non ci sono preferiti, importa i preset già sul DigiRadio.
    private func importDevicePresetsIfFavoritesEmpty() {
        guard favoriteStations.isEmpty else { return }
        let imported = deviceStations.map { ScannedStation.from($0) }
        guard !imported.isEmpty else { return }
        favoriteStations = FMStationMerge.merge(
            Array(imported.prefix(DigiRadioPresetLimits.maxPresets))
        )
        FavoriteStationsStore.save(favoriteStations)
    }

    /// Stessa lista per FM e DAB: il catalogo iPhone più i preset di `GET /api/stations`.
    private func mergeDeviceStationsIntoCatalog() {
        var changed = false
        for station in deviceStations {
            let scanned = ScannedStation.from(station)
            switch scanned.band {
            case .fm:
                if !scannedFM.contains(where: { $0.matches(station) }) {
                    scannedFM.append(scanned)
                    changed = true
                }
            case .dab:
                if !scannedDAB.contains(where: { $0.matches(station) }) {
                    scannedDAB.append(scanned)
                    changed = true
                }
            }
        }
        if changed {
            scannedFM = FMStationMerge.merge(scannedFM)
            scannedDAB.sort { $0.name.localizedCaseInsensitiveCompare($1.name) == .orderedAscending }
            persistScanned()
        }
    }

    func refreshPresets() async {
        await refreshLibrary()
    }

    func clearResults(for band: StationScanBand? = nil) async {
        let target = band ?? selectedBand
        errorMessage = nil
        switch target {
        case .fm: scannedFM = []
        case .dab: scannedDAB = []
        }
        persistScanned()
        statusMessage = "Elenco \(target.rawValue) svuotato sull'iPhone."
    }

    func startScan() async {
        guard isConnected else {
            errorMessage = "Connetti a DigiRadio."
            return
        }
        stopAlternateFrequencyMonitor()
        scanTask?.cancel()

        scanTask = Task {
            await runScan()
        }
        await scanTask?.value
    }

    func cancelScan() {
        scanTask?.cancel()
    }

    private func runScan() async {
        guard !Task.isCancelled else { return }
        guard !isScanning else { return }

        isScanning = true
        errorMessage = nil
        statusMessage = nil
        progressMessage = selectedBand == .dab
            ? "Preparazione scansione DAB…"
            : "Scansione banda FM 87.5–107.9 MHz…"
        service.pauseBackgroundRefresh()
        defer {
            service.resumeBackgroundRefresh()
            isScanning = false
            progressMessage = nil
        }

        switch selectedBand {
        case .fm:
            await scanFM()
        case .dab:
            await scanDAB()
        }
        guard !Task.isCancelled else { return }
    }

    func removeFavorites(ids: [String]) {
        favoriteStations.removeAll { ids.contains($0.id) }
        persistFavorites()
    }

    func save(_ station: Station) async {
        let scanned = listedStations.first { $0.matches(station) }
            ?? favoriteStations.first { $0.matches(station) }
            ?? ScannedStation.from(station)
        await save(scanned)
    }

    func save(_ station: ScannedStation) async {
        if station.band == .fm, !station.hasBroadcastName {
            namingStation = station
            namingText = station.displayTitle
            return
        }
        await saveConfirmed(station, customName: nil)
    }

    func confirmSaveWithCustomName() async {
        guard let station = namingStation else { return }
        let trimmed = namingText.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else {
            errorMessage = "Inserisci un nome per la stazione."
            return
        }
        namingStation = nil
        await saveConfirmed(station, customName: trimmed)
    }

    func cancelNaming() {
        namingStation = nil
        namingText = ""
    }

    private func saveConfirmed(_ station: ScannedStation, customName: String?) async {
        errorMessage = nil
        statusMessage = nil
        guard !isSaved(station) else { return }
        if addFavorite(station, customName: customName) {
            let label = customName ?? station.name
            statusMessage = "Aggiunta ai preferiti: \(label)"
        }
    }

    @discardableResult
    private func addFavorite(_ station: ScannedStation, customName: String?) -> Bool {
        var copy = station
        if let customName {
            let trimmed = customName.trimmingCharacters(in: .whitespacesAndNewlines)
            if !trimmed.isEmpty { copy.name = trimmed }
        }
        guard !favoriteStations.contains(where: { $0.id == copy.id }) else { return true }
        guard favoriteStations.count < DigiRadioPresetLimits.maxPresets else {
            errorMessage = "Hai già \(DigiRadioPresetLimits.maxPresets) preferiti — il DigiRadio non ne tiene di più."
            return false
        }
        favoriteStations.append(copy)
        favoriteStations = FMStationMerge.merge(favoriteStations)
        persistFavorites()
        return true
    }

    private func persistFavorites() {
        FavoriteStationsStore.save(favoriteStations)
        Task { await syncFavoritesToDevice() }
    }

    /// I preferiti sono i 20 preset hardware sul DigiRadio.
    private func syncFavoritesToDevice() async {
        guard isConnected else { return }
        let presets = Array(favoriteStations.prefix(DigiRadioPresetLimits.maxPresets)).map { $0.asPreset() }
        do {
            try await service.replaceAllStations(presets)
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }

    func listen(_ station: Station) async {
        let scanned = listedStations.first { $0.matches(station) }
            ?? favoriteStations.first { $0.matches(station) }
            ?? ScannedStation.from(station)
        await listen(scanned)
    }

    func listen(_ station: ScannedStation) async {
        guard isConnected else {
            errorMessage = "Connetti a DigiRadio."
            return
        }
        listeningStationID = station.id
        errorMessage = nil
        defer { listeningStationID = nil }
        do {
            try await service.applyAudioSourceRouting(forStreaming: false)
            switch station.band {
            case .fm:
                guard let khz = station.fmFrequencyKhz else { return }
                try await service.tuneFM(frequencyKhz: khz)
                try await service.refreshTunerStatus()
                startAlternateFrequencyMonitor(stationID: station.id)
            case .dab:
                stopAlternateFrequencyMonitor()
                guard let idx = station.dabFreqIndex,
                      let sid = station.dabServiceId,
                      let cid = station.dabComponentId else {
                    errorMessage = "Dati DAB incompleti — riscansiona la banda."
                    return
                }
                try await service.playScannedDAB(ensemble: idx, serviceId: sid, componentId: cid)
            }
            statusMessage = "In ascolto: \(station.displayTitle)"
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }

    func listenFavorite(_ station: ScannedStation) async {
        await listen(station)
    }

    func setFavorite(_ station: ScannedStation, starred: Bool) {
        if starred {
            if let index = favoriteStations.firstIndex(where: { $0.id == station.id || $0.matches(station.asPreset()) }) {
                favoriteStations[index] = station
            } else if favoriteStations.count >= DigiRadioPresetLimits.maxPresets {
                errorMessage = "Hai già \(DigiRadioPresetLimits.maxPresets) preferiti — il DigiRadio non ne tiene di più."
                return
            } else {
                favoriteStations.append(station)
            }
            favoriteStations = FMStationMerge.merge(favoriteStations)
        } else {
            favoriteStations.removeAll { $0.id == station.id || $0.matches(station.asPreset()) }
        }
        persistFavorites()
    }

    func applyStationEdit(_ draft: StationEditDraft) async {
        let scanned = draft.asScannedStation()
        favoriteStations.removeAll { $0.id == draft.originalID }
        setFavorite(scanned, starred: draft.isFavorite)
        upsertScanned(scanned)
        await listen(scanned)
        if let index = deviceStations.firstIndex(where: { scanned.matches($0) }) {
            try? await service.updateStation(at: index, name: draft.name)
            try? await service.refreshStations()
        }
    }

    func deleteStation(_ draft: StationEditDraft) {
        let ids: Set<String> = [draft.originalID, draft.asScannedStation().id]
        favoriteStations.removeAll { ids.contains($0.id) }
        scannedFM.removeAll { ids.contains($0.id) }
        scannedDAB.removeAll { ids.contains($0.id) }
        persistFavorites()
        persistScanned()
        statusMessage = "Stazione eliminata: \(draft.name)"
    }

    func saveCurrentTuner() async {
        guard isConnected else {
            errorMessage = "Connetti a DigiRadio."
            return
        }
        errorMessage = nil
        let tuner = service.state.tuner
        let scanned: ScannedStation?
        switch tuner.band {
        case .fm:
            guard let khz = tuner.fm?.frequencyKhz else {
                errorMessage = "Nessuna frequenza FM sintonizzata."
                return
            }
            let label = tuner.fm?.stationName?.trimmingCharacters(in: .whitespacesAndNewlines)
            let hasName = !(label?.isEmpty ?? true)
            let name = hasName ? label! : String(format: "FM %.2f", Double(khz) / 1000)
            scanned = ScannedStation(
                id: "fm-\(khz)",
                name: name,
                band: .fm,
                fmFrequencyKhz: khz,
                dabFreqIndex: nil,
                dabServiceId: nil,
                dabComponentId: nil,
                rssiDbuv: tuner.fm?.rssiDbuv,
                fmFrequenciesKhz: [khz],
                hasBroadcastName: hasName
            )
        case .dab:
            guard let dab = tuner.dab,
                  let sid = dab.playingServiceId,
                  let cid = dab.playingComponentId else {
                errorMessage = "Nessuna stazione DAB in riproduzione."
                return
            }
            let label = dab.dynamicLabel?.trimmingCharacters(in: .whitespacesAndNewlines)
            let hasName = !(label?.isEmpty ?? true)
            let name = hasName ? label! : "DAB \(sid)"
            scanned = ScannedStation(
                id: "dab-\(dab.freqIndex)-\(sid)-\(cid)",
                name: name,
                band: .dab,
                fmFrequencyKhz: nil,
                dabFreqIndex: dab.freqIndex,
                dabServiceId: sid,
                dabComponentId: cid,
                rssiDbuv: nil,
                hasBroadcastName: hasName
            )
        }
        guard let scanned else { return }
        if addFavorite(scanned, customName: nil) {
            statusMessage = "Aggiunta ai preferiti: \(scanned.name)"
        }
    }

    func isSaved(_ station: Station) -> Bool {
        favoriteStations.contains { $0.matches(station) }
    }

    func isSaved(_ station: ScannedStation) -> Bool {
        favoriteStations.contains { $0.id == station.id || $0.matches(station.asPreset()) }
    }

    private func scanFM() async {
        try? await service.applyAudioSourceRouting(forStreaming: false)

        let seeked = await scanFMBySeek()
        var discovered = seeked.hits
        if !seeked.wrappedBand, !Task.isCancelled {
            let fromKhz = (service.state.tuner.fm?.frequencyKhz ?? seeked.lastKhz)
                + FMBandScan.stepKhz
            let rest = await scanFMByStepping(
                fromKhz: fromKhz,
                skipping: Set(discovered.compactMap(\.fmFrequencyKhz))
            )
            discovered.append(contentsOf: rest)
        }

        guard !Task.isCancelled || !discovered.isEmpty else { return }
        scannedFM = FMStationMerge.merge(discovered)
        persistScanned()
        await refreshLibrary()
        if discovered.isEmpty {
            statusMessage = "Nessuna stazione FM trovata."
        } else {
            statusMessage = "Trovate \(scannedFM.count) stazioni FM (\(discovered.count) frequenze)."
        }
    }

    /// Seek a richieste HTTP corte. Se lo seek HW non avanza (fallback 100 kHz
    /// nel driver) copre comunque tutta la banda, poi si completa a step.
    private func scanFMBySeek() async -> (hits: [ScannedStation], wrappedBand: Bool, lastKhz: Int) {
        do {
            try await service.tuneFM(frequencyKhz: FMBandScan.minKhz)
        } catch {
            return ([], false, FMBandScan.minKhz)
        }

        var discovered: [ScannedStation] = []
        var previous = FMBandScan.minKhz
        var seekFailures = 0
        if let first = await fmHitIfUsable() {
            discovered.append(first)
            publishFMScan(discovered)
        }

        for step in 1 ... FMBandScan.maxSeekSteps {
            guard !Task.isCancelled else {
                return (discovered, false, previous)
            }
            progressMessage = String(format: "FM %.2f MHz…", Double(previous) / 1000)
            do {
                try await service.seekFM(direction: "up")
                seekFailures = 0
            } catch {
                seekFailures += 1
                if seekFailures >= 3 {
                    return (discovered, false, previous)
                }
                continue
            }

            guard let khz = service.state.tuner.fm?.frequencyKhz else { continue }
            if khz <= previous {
                return (discovered, true, previous)
            }
            previous = khz
            progressMessage = String(format: "FM %.2f MHz…", Double(khz) / 1000)
            if let hit = await fmHitIfUsable() {
                discovered.append(hit)
                publishFMScan(discovered)
            }
        }
        return (discovered, false, previous)
    }

    private func scanFMByStepping(
        fromKhz: Int = FMBandScan.minKhz,
        skipping: Set<Int> = []
    ) async -> [ScannedStation] {
        let frequencies = FMBandScan.frequencies.filter { $0 >= fromKhz && !skipping.contains($0) }
        var discovered: [ScannedStation] = []
        for (index, khz) in frequencies.enumerated() {
            guard !Task.isCancelled else { return discovered }
            progressMessage = String(
                format: "FM %.2f MHz (%d/%d)…",
                Double(khz) / 1000,
                index + 1,
                frequencies.count
            )
            do {
                try await service.tuneFM(frequencyKhz: khz)
            } catch {
                continue
            }
            if let hit = await fmHitIfUsable() {
                discovered.append(hit)
                publishFMScan(FMStationMerge.merge(scannedFM + [hit]))
            }
        }
        return discovered
    }

    private func fmHitIfUsable() async -> ScannedStation? {
        try? await Task.sleep(nanoseconds: FMBandScan.tuneSettleMs * 1_000_000)
        do {
            try await service.refreshTunerStatus()
        } catch {
            return nil
        }
        guard service.state.tuner.band == .fm,
              let fm = service.state.tuner.fm,
              FMBandScan.isUsableHit(fm: fm, locked: service.state.tuner.locked) else {
            return nil
        }

        var name = fm.stationName
        var rssi = fm.rssiDbuv
        var snr = fm.snrDb
        for _ in 0 ..< FMBandScan.namePollAttempts {
            if mergeableRDSName(name, khz: fm.frequencyKhz) != nil { break }
            try? await Task.sleep(nanoseconds: FMBandScan.namePollMs * 1_000_000)
            try? await service.refreshTunerStatus()
            name = service.state.tuner.fm?.stationName
            rssi = service.state.tuner.fm?.rssiDbuv ?? rssi
            snr = service.state.tuner.fm?.snrDb ?? snr
        }

        return ScannedStation.fromFM(
            FMScanHit(
                frequencyKhz: fm.frequencyKhz,
                rssiDbuv: rssi,
                snrDb: snr,
                stationName: mergeableRDSName(name, khz: fm.frequencyKhz)
            )
        )
    }

    private func mergeableRDSName(_ raw: String?, khz: Int) -> String? {
        guard let name = StationNameDisplay.meaningful(raw, khz: khz),
              name.count >= FMStationMerge.minNameLength else {
            return nil
        }
        return name
    }

    private func publishFMScan(_ discovered: [ScannedStation]) {
        scannedFM = FMStationMerge.merge(discovered)
    }

    private func scanDAB() async {
        var discovered: [ScannedStation] = []
        progressMessage = "Preparazione scansione DAB…"
        await service.prepareForDABOperations()

        for idx in 0 ... 37 {
            guard !Task.isCancelled else { return }
            progressMessage = "Scansione DAB \(idx + 1)/38…"

            do {
                try await service.tuneDAB(freqIndex: idx)
            } catch is CancellationError {
                return
            } catch {
                continue
            }

            guard service.state.tuner.locked else { continue }
            if let fic = service.state.tuner.dab?.ficQuality, fic < Self.minDabFicQuality { continue }

            let services: [DABService]
            do {
                services = try await service.listDABServices()
            } catch is CancellationError {
                return
            } catch {
                continue
            }

            for svc in services {
                let candidate = ScannedStation.fromDAB(ensemble: idx, service: svc)
                if !discovered.contains(candidate) {
                    discovered.append(candidate)
                }
            }
        }

        let sorted = discovered.sorted {
            $0.name.localizedCaseInsensitiveCompare($1.name) == .orderedAscending
        }

        guard !Task.isCancelled else {
            if !sorted.isEmpty {
                await persistDABScan(sorted)
                statusMessage = "Scansione interrotta — \(sorted.count) stazioni DAB trovate."
            }
            return
        }

        await persistDABScan(sorted)
        statusMessage = sorted.isEmpty
            ? "Nessuna stazione DAB trovata."
            : "Trovate \(sorted.count) stazioni DAB."
    }

    private func persistDABScan(_ stations: [ScannedStation]) async {
        scannedDAB = stations
        persistScanned()
        await refreshLibrary()
    }

    private func upsertScanned(_ station: ScannedStation) {
        switch station.band {
        case .fm:
            scannedFM.removeAll { $0.id == station.id || $0.matches(station.asPreset()) }
            scannedFM.append(station)
            scannedFM = FMStationMerge.merge(scannedFM)
        case .dab:
            scannedDAB.removeAll { $0.id == station.id || $0.matches(station.asPreset()) }
            scannedDAB.append(station)
            scannedDAB.sort { $0.name.localizedCaseInsensitiveCompare($1.name) == .orderedAscending }
        }
        persistScanned()
    }

    /// Avvia (o riallinea) il monitor AF sulla stazione FM che include questa frequenza.
    func syncAlternateFrequencyMonitor(forFMKhz khz: Int?) {
        guard let khz else {
            stopAlternateFrequencyMonitor()
            return
        }
        let station = scannedFM.first { $0.containsFMFrequency(khz) }
            ?? favoriteStations.first { $0.band == .fm && $0.containsFMFrequency(khz) }
        guard let station, station.allFMFrequenciesKhz.count >= 2 else {
            stopAlternateFrequencyMonitor()
            return
        }
        startAlternateFrequencyMonitor(stationID: station.id)
    }

    func stopAlternateFrequencyMonitor() {
        afMonitorTask?.cancel()
        afMonitorTask = nil
        afStationID = nil
    }

    private func startAlternateFrequencyMonitor(stationID: String) {
        let station = stationForAF(id: stationID)
        guard let station, station.allFMFrequenciesKhz.count >= 2 else {
            stopAlternateFrequencyMonitor()
            return
        }
        if afStationID == stationID, afMonitorTask != nil {
            return
        }
        stopAlternateFrequencyMonitor()
        afStationID = stationID
        afMonitorTask = Task { [weak self] in
            try? await Task.sleep(nanoseconds: FMBandScan.afListenSettleMs * 1_000_000)
            while !Task.isCancelled {
                await self?.huntStrongerAlternateIfNeeded()
                try? await Task.sleep(nanoseconds: FMBandScan.afPollMs * 1_000_000)
            }
        }
    }

    private func stationForAF(id: String) -> ScannedStation? {
        scannedFM.first { $0.id == id }
            ?? favoriteStations.first { $0.id == id }
    }

    private func huntStrongerAlternateIfNeeded() async {
        guard isConnected, !isScanning, listeningStationID == nil else { return }
        guard let stationID = afStationID,
              let station = stationForAF(id: stationID) else { return }
        let freqs = station.allFMFrequenciesKhz
        guard freqs.count >= 2 else { return }
        if let last = lastAFHuntAt,
           Date().timeIntervalSince(last) * 1000 < Double(FMBandScan.afCooldownMs) {
            return
        }

        do {
            try await service.refreshTunerStatus()
        } catch {
            return
        }
        guard service.state.tuner.band == .fm,
              let current = currentFMSnapshot() else { return }
        guard AlternateFrequencyHunt.isWeak(current) else { return }
        lastAFHuntAt = Date()

        let samples = await sampleAlternateFrequencies(freqs, current: current)
        guard !Task.isCancelled else { return }
        let best = AlternateFrequencyHunt.best(of: samples) ?? current
        let chosen = AlternateFrequencyHunt.shouldSwitch(from: current, to: best) ? best : current
        await applyAFChoice(chosen, original: current, stationID: stationID)
    }

    private func currentFMSnapshot() -> AlternateFrequencyHunt.Snapshot? {
        guard let khz = service.state.tuner.fm?.frequencyKhz else { return nil }
        return AlternateFrequencyHunt.Snapshot(
            khz: khz,
            rssiDbuv: service.state.tuner.fm?.rssiDbuv ?? -128,
            locked: service.state.tuner.locked
        )
    }

    private func sampleAlternateFrequencies(
        _ freqs: [Int],
        current: AlternateFrequencyHunt.Snapshot
    ) async -> [AlternateFrequencyHunt.Snapshot] {
        var samples = [current]
        for khz in freqs where khz != current.khz {
            guard !Task.isCancelled else { return samples }
            statusMessage = String(format: "Segnale debole — provo %.2f MHz…", Double(khz) / 1000)
            if let sample = await sampleFM(khz) {
                samples.append(sample)
            }
        }
        return samples
    }

    private func sampleFM(_ khz: Int) async -> AlternateFrequencyHunt.Snapshot? {
        do {
            try await service.tuneFM(frequencyKhz: khz)
            try await Task.sleep(nanoseconds: FMBandScan.afSettleMs * 1_000_000)
            try await service.refreshTunerStatus()
            return currentFMSnapshot()
        } catch {
            return nil
        }
    }

    private func applyAFChoice(
        _ chosen: AlternateFrequencyHunt.Snapshot,
        original: AlternateFrequencyHunt.Snapshot,
        stationID: String
    ) async {
        if chosen.khz != service.state.tuner.fm?.frequencyKhz {
            try? await service.tuneFM(frequencyKhz: chosen.khz)
            try? await service.refreshTunerStatus()
        }
        if chosen.khz != original.khz {
            updatePreferredFrequency(stationID: stationID, khz: chosen.khz, rssi: chosen.rssiDbuv)
            statusMessage = String(
                format: "Passato a %.2f MHz (segnale più forte)",
                Double(chosen.khz) / 1000
            )
        } else if let station = stationForAF(id: stationID) {
            statusMessage = "In ascolto: \(station.displayTitle)"
        }
    }

    private func updatePreferredFrequency(stationID: String, khz: Int, rssi: Int) {
        if let index = scannedFM.firstIndex(where: { $0.id == stationID }) {
            scannedFM[index].fmFrequencyKhz = khz
            scannedFM[index].rssiDbuv = rssi
            persistScanned()
        }
        if let index = favoriteStations.firstIndex(where: { $0.id == stationID }) {
            favoriteStations[index].fmFrequencyKhz = khz
            favoriteStations[index].rssiDbuv = rssi
            persistFavorites()
        }
    }

    private func persistScanned() {
        ScannedStationStore.save(fm: scannedFM, dab: scannedDAB)
    }
}
