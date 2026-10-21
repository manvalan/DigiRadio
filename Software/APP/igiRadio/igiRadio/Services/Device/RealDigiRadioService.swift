import Foundation
import Observation
import OSLog

@Observable
final class RealDigiRadioService: DigiRadioService {
    private static let dabSettleMs: UInt64 = 1_000

    private(set) var state = DigiRadioState()
    private(set) var isBackgroundRefreshPaused = false
    private let client = HTTPDigiRadioClient()
    private let audioGate = DigiRadioOperationGate()
    private let logger = Logger(subsystem: "com.digiradio.igiRadio", category: "Device")

    func pauseBackgroundRefresh() {
        isBackgroundRefreshPaused = true
    }

    func resumeBackgroundRefresh() {
        isBackgroundRefreshPaused = false
    }

    func connect(host: String) async throws {
        state.connection.isConnecting = true
        state.connection.lastError = nil
        defer { state.connection.isConnecting = false }

        try client.setHost(host)
        state.connection.host = HostNormalizer.httpURL(from: host)
        try await refreshHealth()
        state.connection.isConnected = true
        ConnectionPreferences.saveLastHost(state.connection.host)
        logger.info("Connected to \(self.state.connection.host, privacy: .public)")
    }

    func disconnect() {
        client.clearHost()
        state.connection = ConnectionState()
        clearLiveDeviceInfo()
    }

    func refreshHealth() async throws {
        let health = try await client.fetchHealth()
        state.health.status = health.status
        state.health.firmware = health.fw
        state.health.serialNumber = health.serialNumber
        state.health.chips = ChipHealth(
            si4684: health.chips.si4684,
            adau1701: health.chips.adau1701,
            bt1035: health.chips.bt1035
        )
    }

    func verifyReachability() async -> Bool {
        do {
            try await refreshHealth()
            state.connection.isConnected = true
            return true
        } catch {
            state.connection.isConnected = false
            clearLiveDeviceInfo()
            return false
        }
    }

    func resetWifi() async throws -> Int {
        let response = try await client.resetWifi()
        if response.status == "error" {
            throw DigiRadioError.httpStatus(400, response.reason)
        }
        let seconds = response.rebootInSec ?? 3
        disconnect()
        return seconds
    }

    func refreshTunerStatus() async throws {
        let tuner = try await client.fetchTunerStatus()
        applyTuner(tuner)
    }

    func refreshAudioProfile() async throws {
        try await audioGate.run {
            let profile = try await client.fetchAudioProfile()
            applyAudioProfileState(profile)
        }
    }

    func refreshBluetoothStatus() async throws {
        let bt = try await client.fetchBluetoothStatus()
        state.bluetooth.booted = bt.booted
        state.bluetooth.pairing = bt.pairing
        state.bluetooth.a2dpState = A2DPState(apiValue: bt.a2dp)
        state.bluetooth.deviceName = bt.deviceName
        state.bluetooth.autoReconnect = bt.autoReconnect

        if let speaker = try? await client.fetchSavedSpeaker(),
           speaker.configured, let mac = speaker.mac, let name = speaker.name {
            state.bluetooth.savedSpeaker = SavedSpeaker(mac: mac, name: name)
        } else {
            state.bluetooth.savedSpeaker = nil
        }

        guard bt.booted else {
            state.bluetooth.pairedDevices = []
            state.bluetooth.nearbyDevices = []
            return
        }

        let paired = try? await client.fetchBluetoothPaired()
        state.bluetooth.pairedDevices = paired?.devices.map {
            BluetoothPairedDevice(index: $0.index, mac: $0.mac, name: $0.name)
        } ?? []

        if let codec = try? await client.fetchA2DPCodec() {
            state.bluetooth.negotiatedCodec = A2DPCodec(apiValue: codec.codec)
        }
    }

    func refreshStations() async throws {
        let list = try await client.fetchStations()
        state.stations = list.stations
    }

    func replaceStations(for band: TunerBand, with stations: [Station]) async throws {
        try await refreshStations()
        let keep = state.stations.filter { $0.band != band }
        let incoming = stations.filter { $0.band == band }
        let capacity = max(0, DigiRadioPresetLimits.maxPresets - keep.count)
        var target = keep + Array(incoming.prefix(capacity))
        if target.count > DigiRadioPresetLimits.maxPresets {
            target = Array(target.prefix(DigiRadioPresetLimits.maxPresets))
        }
        while !state.stations.isEmpty {
            try await removeStation(at: state.stations.count - 1)
        }
        for station in target {
            try await client.addStation(station)
        }
        try await refreshStations()
    }

    func replaceAllStations(_ stations: [Station]) async throws {
        try await refreshStations()
        let target = Array(stations.prefix(DigiRadioPresetLimits.maxPresets))
        while !state.stations.isEmpty {
            try await removeStation(at: state.stations.count - 1)
        }
        for station in target {
            try await client.addStation(station)
        }
        try await refreshStations()
    }

    func refreshStreaming() async throws {
        state.streaming = try await client.fetchStreaming()
    }

    func setStreaming(enabled: Bool, url: String) async throws {
        try await audioGate.run {
            let config = StreamingState(
                enabled: enabled,
                url: StreamingState.deviceSafeURL(url)
            )
            state.streaming = try await client.setStreaming(config)
            try await applyAudioSourceRoutingLocked(forStreaming: enabled)
        }
    }

    func refreshPower() async throws {
        state.power = try await client.fetchPower().state
    }

    func setPower(_ power: DevicePowerState) async throws {
        state.power = try await client.setPower(power).state
    }

    func refreshFirmwareAF() async {
        state.firmwareAF = try? await client.fetchAfFollow()
    }

    func setFirmwareAF(enabled: Bool) async throws {
        state.firmwareAF = try await client.setAfFollow(enabled: enabled)
    }

    func applyAudioSourceRouting(forStreaming: Bool) async throws {
        try await audioGate.run {
            try await applyAudioSourceRoutingLocked(forStreaming: forStreaming)
        }
    }

    /// Ferma lo streaming se attivo (non blocca il DAB se la richiesta fallisce).
    func prepareForDABOperations() async {
        do {
            try await refreshStreaming()
            guard state.streaming.enabled else { return }
            state.streaming = try await client.setStreaming(
                StreamingState(
                    enabled: false,
                    url: StreamingState.deviceSafeURL(state.streaming.url)
                )
            )
        } catch {
            logger.warning("prepareForDAB skipped: \(error.localizedDescription, privacy: .public)")
        }
    }

    private func applyAudioSourceRoutingLocked(forStreaming: Bool) async throws {
        if !forStreaming { state.phoneAudioActive = false }
        var profile = try await client.fetchAudioProfile()
        profile.activeSource = forStreaming ? .bluetooth : .radio
        try await applyProfileSeparatingEnhance(profile)
    }

    func prepareForPhoneAudio() async throws {
        try await audioGate.run {
            state.phoneAudioActive = true
            if state.streaming.enabled {
                state.streaming = try await client.setStreaming(
                    StreamingState(
                        enabled: false,
                        url: StreamingState.deviceSafeURL(state.streaming.url)
                    )
                )
            }
            var profile = try await client.fetchAudioProfile()
            profile.activeSource = .bluetooth
            let avgDb = (profile.master.leftDb + profile.master.rightDb) / 2
            if avgDb < -30 {
                let db = VolumeMapping.masterDb(forVolumeLevel: 55)
                profile.master.leftDb = db
                profile.master.rightDb = db
            }
            try await applyProfileSeparatingEnhance(profile)
        }
    }

    func endPhoneAudio() async throws {
        try await audioGate.run {
            state.phoneAudioActive = false
            var profile = try await client.fetchAudioProfile()
            profile.activeSource = .radio
            try await applyProfileSeparatingEnhance(profile)
        }
    }

    func tuneFM(frequencyKhz: Int, antcap: Int? = nil) async throws {
        let tuner = try await client.tune(body: TuneRequest(
            band: "fm",
            freqIndex: nil,
            frequencyKhz: frequencyKhz,
            antcap: antcap
        ))
        applyTuner(tuner)
    }

    func tuneDAB(freqIndex: Int) async throws {
        let tuner = try await client.tune(
            body: TuneRequest(band: "dab", freqIndex: freqIndex, frequencyKhz: nil),
            requestTimeout: 45
        )
        applyTuner(tuner)
        try await Task.sleep(nanoseconds: Self.dabSettleMs * 1_000_000)
    }

    func playDAB(serviceId: Int, componentId: Int) async throws {
        do {
            try await client.playDAB(body: PlayRequest(serviceId: serviceId, componentId: componentId))
        } catch let DigiRadioError.httpStatus(409, _) {
            try await refreshTunerStatus()
            let playing = state.tuner.band == .dab
                && state.tuner.dab?.playingServiceId == serviceId
                && state.tuner.dab?.playingComponentId == componentId
            if !playing {
                throw DigiRadioError.httpStatus(409, "{\"reason\":\"play_failed\"}")
            }
            return
        }
        try await Task.sleep(nanoseconds: 200 * 1_000_000)
        try await refreshTunerStatus()
    }

    func playScannedDAB(ensemble: Int, serviceId: Int, componentId: Int) async throws {
        try await playDABIdentified(ensemble: ensemble, serviceId: serviceId, componentId: componentId)
    }

    /// Tune ensemble → verifica servizio in elenco → play (con retry).
    private func playDABIdentified(ensemble: Int, serviceId: Int, componentId: Int) async throws {
        await prepareForDABOperations()
        try await refreshTunerStatus()

        let onEnsemble = state.tuner.band == .dab
            && state.tuner.locked
            && state.tuner.dab?.freqIndex == ensemble
        if !onEnsemble {
            try await tuneDAB(freqIndex: ensemble)
        }

        guard state.tuner.band == .dab, state.tuner.locked else {
            throw DigiRadioError.httpStatus(409, "{\"reason\":\"tune_failed\"}")
        }

        let services = try await listDABServices()
        guard services.contains(where: { $0.serviceId == serviceId && $0.componentId == componentId }) else {
            throw DigiRadioError.httpStatus(409, "{\"reason\":\"dab_service_not_found\"}")
        }

        var lastError: Error = DigiRadioError.httpStatus(409, "{\"reason\":\"play_failed\"}")
        for attempt in 0 ..< 3 {
            do {
                try await playDAB(serviceId: serviceId, componentId: componentId)
                return
            } catch {
                lastError = error
                guard attempt < 2, Self.isRetryableDABPlayError(error) else { break }
                try await Task.sleep(nanoseconds: UInt64(400 + attempt * 300) * 1_000_000)
                try? await refreshTunerStatus()
            }
        }
        throw lastError
    }

    private static func isRetryableDABPlayError(_ error: Error) -> Bool {
        guard let digi = error as? DigiRadioError,
              case let .httpStatus(409, reason) = digi,
              let body = reason?.lowercased() else { return false }
        return body.contains("service_list_empty")
            || body.contains("wrong_band")
            || body.contains("not_booted")
            || body.contains("play_failed")
            || body.contains("hardware_failed")
    }

    func seekFM(direction: String) async throws {
        _ = try await client.seekFM(direction: direction)
        try await refreshTunerStatus()
    }

    func scanFM(maxSteps: Int, name: String?) async throws -> TunerScanResponse {
        try await client.scanStation(body: TunerScanRequest(band: "fm", maxSteps: maxSteps, name: name), requestTimeout: 60)
    }

    func scanDAB(maxSteps: Int, name: String?) async throws -> TunerScanResponse {
        let result = try await client.scanDAB(maxSteps: maxSteps, name: name)
        // Firmware 0.9.0: scan già sintonizza l'ensemble e avvia play — non richiamare tune.
        if result.status == "found" {
            try await refreshTunerStatus()
        }
        return result
    }

    /// `POST /api/tuner/scan/full` — una sola risposta a fine banda (~86 s). Timeout HTTP 120 s.
    func scanFullFM(progress: (@Sendable (Int, Int) -> Void)? = nil) async throws -> [FMScanHit] {
        progress?(0, 1)
        let response = try await client.scanFullFM()
        try await refreshTunerStatus()
        return response.stations.sorted { $0.frequencyKhz < $1.frequencyKhz }
    }

    func calibrateAntenna(antcap: Int) async throws {
        _ = try await client.calibrateAntenna(antcap: antcap)
    }

    func setVolume(_ volume: Int) async throws {
        try await audioGate.run {
            var profile = try await client.fetchAudioProfile()
            let clamped = max(0, min(100, volume))
            let db = VolumeMapping.masterDb(forVolumePercent: clamped)
            profile.master.leftDb = db
            profile.master.rightDb = db
            try await applyProfileSeparatingEnhance(profile)
            state.tuner.volume = clamped
        }
    }

    func listDABServices() async throws -> [DABService] {
        var lastError: Error = DigiRadioError.httpStatus(409, "{\"reason\":\"service_list_empty\"}")
        for attempt in 0 ..< 5 {
            do {
                let response = try await client.fetchDABServices()
                return response.services
            } catch {
                lastError = error
                guard Self.isRetryableDABListError(error), attempt < 4 else { break }
                try await Task.sleep(nanoseconds: UInt64(350 + attempt * 250) * 1_000_000)
                try? await refreshTunerStatus()
            }
        }
        throw lastError
    }

    private static func isRetryableDABListError(_ error: Error) -> Bool {
        guard let digi = error as? DigiRadioError,
              case let .httpStatus(409, reason) = digi,
              let body = reason?.lowercased() else { return false }
        return body.contains("service_list_empty")
            || body.contains("wrong_band")
            || body.contains("not_booted")
            || body.contains("hardware_failed")
    }

    func saveStation(_ station: Station) async throws {
        try await client.addStation(station)
        try await refreshStations()
    }

    func updateStation(at index: Int, name: String) async throws {
        let trimmed = name.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else {
            throw DigiRadioError.httpStatus(400, "{\"reason\":\"invalid_name\"}")
        }
        guard state.stations.indices.contains(index) else { return }
        var updated = state.stations[index]
        updated.name = trimmed
        try await client.removeStation(index: index)
        try await client.addStation(updated)
        try await refreshStations()
        let newIndex = state.stations.count - 1
        if newIndex != index, state.stations.indices.contains(newIndex) {
            try await client.reorderStation(from: newIndex, to: index)
            try await refreshStations()
        }
    }

    func removeStation(at index: Int) async throws {
        try await client.removeStation(index: index)
        try await refreshStations()
    }

    func reorderStation(from: Int, to: Int) async throws {
        try await client.reorderStation(from: from, to: to)
        try await refreshStations()
    }

    func tuneStation(at index: Int) async throws {
        guard state.stations.indices.contains(index) else { return }
        let station = state.stations[index]

        switch station.band {
        case .fm:
            guard let khz = station.fmFrequencyKhz else {
                throw DigiRadioError.httpStatus(400, "{\"reason\":\"missing_frequency\"}")
            }
            try await tuneFM(frequencyKhz: khz)
        case .dab:
            try await playDABStation(station)
        }
    }

    /// Sintonizza e avvia un preset DAB (stesso flusso del web: tune → play).
    private func playDABStation(_ station: Station) async throws {
        guard let ensemble = station.dabFreqIndex ?? state.tuner.dab?.freqIndex else {
            if state.tuner.band == .dab, state.tuner.locked,
               let sid = station.dabServiceId, let cid = station.dabComponentId {
                try await playDABIdentified(
                    ensemble: state.tuner.dab?.freqIndex ?? 0,
                    serviceId: sid,
                    componentId: cid
                )
                return
            }
            throw DigiRadioError.httpStatus(400, "{\"reason\":\"missing_ensemble\"}")
        }

        if let sid = station.dabServiceId, let cid = station.dabComponentId {
            try await playDABIdentified(ensemble: ensemble, serviceId: sid, componentId: cid)
            return
        }

        try await tuneDAB(freqIndex: ensemble)
        let services = try await listDABServices()
        let trimmed = station.name.trimmingCharacters(in: .whitespacesAndNewlines)
        if !trimmed.isEmpty,
           let match = services.first(where: {
               $0.label.compare(trimmed, options: .caseInsensitive) == .orderedSame
           }) {
            try await playDAB(serviceId: match.serviceId, componentId: match.componentId)
            return
        }

        throw DigiRadioError.httpStatus(409, "{\"reason\":\"dab_service_not_found\"}")
    }

    func applyAudioProfile(_ profile: AudioProfileDTO) async throws {
        try await audioGate.run {
            try await applyProfileSeparatingEnhance(profile)
        }
    }

    func applyAudioPreset(_ preset: AudioProfileTemplate) async throws {
        try await audioGate.run {
            let live = try await client.fetchAudioProfile()
            let preservedMaster = live.master
            var merged = normalizedProfile(live).mergingPresetEQ(preset)
            if !state.streaming.enabled {
                merged.activeSource = .radio
            }
            merged.master = preservedMaster
            merged.enhancements = preset.enhancements
            try await applyProfileSeparatingEnhance(merged)
            state.audio.master = preservedMaster
        }
    }

    func setStereoEnhance(level: Int) async throws {
        try await audioGate.run {
            try await client.setEnhance(path: "/api/audio/stereo-enhance", level: level)
            state.audio.enhancements.stereoLevel = level
            try await ensureRadioAudibleLocked()
        }
    }

    func setBassEnhance(level: Int) async throws {
        try await audioGate.run {
            try await client.setEnhance(path: "/api/audio/bass-enhance", level: level)
            state.audio.enhancements.bassLevel = level
            try await ensureRadioAudibleLocked()
        }
    }

    func resetAudio() async throws {
        try await audioGate.run {
            try await client.resetAudio()
            let profile = try await client.fetchAudioProfile()
            applyAudioProfileState(profile)
        }
    }

    func fetchAudioLevels() async throws -> AudioLevelsDTO {
        try await client.fetchAudioLevels()
    }

    func scanBluetooth(seconds: Int) async throws {
        let response = try await client.scanBluetooth(seconds: seconds)
        state.bluetooth.nearbyDevices = response.devices.map {
            BluetoothDevice(index: $0.index, mac: $0.mac, name: $0.name, rssiDbm: $0.rssiDbm)
        }
    }

    func setBluetoothEnabled(_ enabled: Bool) async throws {
        if enabled {
            try await client.enableBluetooth()
        } else {
            try await client.disableBluetooth()
        }
        try await refreshBluetoothStatus()
    }

    func connectBluetooth(mac: String, name: String?, save: Bool) async throws {
        guard let normalizedMac = BtMacNormalizer.forApi(mac) else {
            throw DigiRadioError.httpStatus(400, "{\"reason\":\"invalid_mac\"}")
        }
        try await client.connectBluetooth(mac: normalizedMac, name: name, save: save)
        try await refreshBluetoothStatus()
    }

    func reconnectBluetooth() async throws {
        try await client.reconnectBluetooth()
        try await refreshBluetoothStatus()
    }

    func startBluetoothPairing() async throws {
        try await client.startBluetoothPairing()
        try await refreshBluetoothStatus()
    }

    func stopBluetoothPairing() async throws {
        try await client.stopBluetoothPairing()
        try await refreshBluetoothStatus()
    }

    func disconnectBluetoothA2DP() async throws {
        try await client.disconnectBluetooth()
        try await refreshBluetoothStatus()
    }

    func clearBluetoothSpeaker() async throws {
        try await client.clearBluetoothSpeaker()
        try await refreshBluetoothStatus()
    }

    func playAudioBeep(enabled: Bool) async throws {
        try await audioGate.run {
            try await client.setAudioBeep(enabled: enabled)
        }
    }

    func fetchDspParams() async throws -> DspParamsResponse {
        try await client.fetchDspParams()
    }

    func setDspParam(name: String, value: Double) async throws {
        try await audioGate.run {
            try await client.putDspParam(name: name, value: value)
        }
    }

    func setBluetoothAutoReconnect(times: Int) async throws {
        try await client.setBluetoothAutoReconnect(times: times)
        try await refreshBluetoothStatus()
    }

    func fetchA2DPCodec() async throws {
        let response = try await client.fetchA2DPCodec()
        state.bluetooth.negotiatedCodec = A2DPCodec(apiValue: response.codec)
    }

    func setA2DPCodec(mask: Int) async throws -> String? {
        let response = try await client.setA2DPCodec(mask: mask)
        if response.status == "saved" {
            return nil
        }
        return response.reason ?? "Errore salvataggio codec."
    }

    private func applyTuner(_ tuner: TunerStatusResponse) {
        state.tuner.booted = tuner.booted
        state.tuner.band = TunerBand(rawValue: tuner.band) ?? .fm
        state.tuner.locked = tuner.locked
        state.tuner.volume = tuner.volume
        state.tuner.fm = tuner.fm
        state.tuner.dab = tuner.dab
    }

    private func applyAudioProfileState(_ profile: AudioProfileDTO) {
        state.audio.activeSource = profile.activeSource
        state.audio.master = profile.master
        state.audio.eq = profile.eq.enumerated().map { index, band in
            EQBandState(
                index: index,
                gainDb: band.gainDb,
                centerHz: band.centerHz,
                q: band.q,
                isLocked: band.isLocked
            )
        }
        state.audio.enhancements = profile.enhancements
        let avgDb = (profile.master.leftDb + profile.master.rightDb) / 2
        state.tuner.volume = VolumeMapping.volumePercent(forMasterDb: avgDb)
    }

    private func normalizedProfile(_ profile: AudioProfileDTO) -> AudioProfileDTO {
        var copy = profile.clampedForDevice()
        if copy.eq.count != AudioProfileTemplate.bandCenters.count {
            copy.eq = AudioProfileTemplate.eqBands(gains: Array(repeating: 0, count: AudioProfileTemplate.bandCenters.count))
        }
        return copy
    }

    /// Ripristina la sorgente radio se era su bluetooth/beep mentre non si fa streaming.
    private func ensureRadioAudibleLocked() async throws {
        guard !state.streaming.enabled, !state.phoneAudioActive else { return }
        var profile = try await client.fetchAudioProfile()
        guard profile.activeSource != .radio else { return }
        profile.activeSource = .radio
        try await applyProfileSeparatingEnhance(profile)
    }

    /// PUT profilo con enhancements a 0, poi POST stereo/bass.
    /// Il PUT con SuperPhat/Bass ≠ 0 sul firmware 0.9 silenzia l'ADAU.
    /// Voice Clarifier non ha API: overlay EQ presence, reversibile.
    private func applyProfileSeparatingEnhance(_ profile: AudioProfileDTO) async throws {
        var safe = normalizedProfile(profile)
        let stereo = safe.enhancements.stereoLevel
        let bass = safe.enhancements.bassLevel
        let voice = safe.enhancements.voiceClean
        safe.enhancements = EnhancementsState(stereoLevel: 0, bassLevel: 0)
        try await client.putAudioProfile(safe)
        try await client.setEnhance(path: "/api/audio/stereo-enhance", level: stereo)
        try await client.setEnhance(path: "/api/audio/bass-enhance", level: bass)

        safe.enhancements = EnhancementsState(
            stereoLevel: stereo,
            bassLevel: bass,
            voiceClean: voice
        )
        applyAudioProfileState(safe)
    }

    private func clearLiveDeviceInfo() {
        state.health = HealthState()
    }
}
