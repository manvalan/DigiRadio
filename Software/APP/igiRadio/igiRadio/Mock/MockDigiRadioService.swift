import Foundation
import Observation

@Observable
final class MockDigiRadioService: DigiRadioService {
    private(set) var state = DigiRadioState()
    private(set) var isBackgroundRefreshPaused = false

    private static let fmHits: [Int: (rssi: Int, snr: Int, name: String)] = [
        97_500: (55, 20, "RDS"),
        101_500: (48, 18, "Radio Mock"),
        105_200: (51, 19, "Radio Italia"),
    ]

    init() {
        seed()
        state.connection.host = "mock.local"
        state.connection.isConnected = true
    }

    func pauseBackgroundRefresh() {
        isBackgroundRefreshPaused = true
    }

    func resumeBackgroundRefresh() {
        isBackgroundRefreshPaused = false
    }

    func connect(host: String) async throws {
        try await delay()
        seed()
        state.connection.host = host
        state.connection.isConnected = true
        state.connection.lastError = nil
    }

    func disconnect() {
        state.connection = ConnectionState()
        state.health = HealthState()
    }

    func refreshHealth() async throws {
        try await delay()
        guard state.connection.isConnected else {
            throw DigiRadioError.notConnected
        }
    }

    func verifyReachability() async -> Bool {
        do {
            try await refreshHealth()
            return true
        } catch {
            state.connection.isConnected = false
            state.health = HealthState()
            return false
        }
    }

    func resetWifi() async throws -> Int {
        try await delay()
        guard state.connection.isConnected else {
            throw DigiRadioError.notConnected
        }
        state.connection = ConnectionState()
        state.health = HealthState()
        return 3
    }

    func refreshTunerStatus() async throws {
        try await delay()
    }

    func refreshAudioProfile() async throws {
        try await delay()
    }

    func refreshBluetoothStatus() async throws {
        try await delay()
        state.bluetooth.pairedDevices = [
            BluetoothPairedDevice(index: 1, mac: "AABBCCDDEEFF", name: "iPhone Mock")
        ]
    }

    func refreshStations() async throws {
        try await delay()
    }

    func replaceStations(for band: TunerBand, with stations: [Station]) async throws {
        try await delay()
        let keep = state.stations.filter { $0.band != band }
        let incoming = stations.filter { $0.band == band }
        let capacity = max(0, DigiRadioPresetLimits.maxPresets - keep.count)
        state.stations = keep + Array(incoming.prefix(capacity))
    }

    func replaceAllStations(_ stations: [Station]) async throws {
        try await delay()
        state.stations = Array(stations.prefix(DigiRadioPresetLimits.maxPresets))
    }

    func refreshStreaming() async throws {
        try await delay()
    }

    func setStreaming(enabled: Bool, url: String) async throws {
        try await delay()
        state.streaming = StreamingState(enabled: enabled, url: url)
        try await applyAudioSourceRouting(forStreaming: enabled)
    }

    func refreshPower() async throws {
        try await delay()
    }

    func setPower(_ power: DevicePowerState) async throws {
        try await delay()
        state.power = power
    }

    func applyAudioSourceRouting(forStreaming: Bool) async throws {
        try await delay(0.1)
        if !forStreaming { state.phoneAudioActive = false }
        state.audio.activeSource = forStreaming ? .bluetooth : .radio
    }

    func prepareForPhoneAudio() async throws {
        try await delay(0.1)
        state.phoneAudioActive = true
        state.streaming.enabled = false
        state.audio.activeSource = .bluetooth
    }

    func endPhoneAudio() async throws {
        try await delay(0.1)
        state.phoneAudioActive = false
        state.audio.activeSource = .radio
    }

    func prepareForDABOperations() async {
        try? await delay(0.1)
        state.streaming.enabled = false
    }

    func tuneFM(frequencyKhz: Int, antcap: Int? = nil) async throws {
        try await delay(0.02)
        state.tuner.band = .fm
        if let hit = Self.fmHits[frequencyKhz] {
            state.tuner.locked = true
            state.tuner.fm = FMTunerState(
                frequencyKhz: frequencyKhz,
                rssiDbuv: hit.rssi,
                snrDb: hit.snr,
                stereo: true,
                stationName: hit.name,
                radiotext: "igiRadio demo mode"
            )
        } else {
            state.tuner.locked = false
            state.tuner.fm = FMTunerState(
                frequencyKhz: frequencyKhz,
                rssiDbuv: 2,
                snrDb: 1,
                stereo: false,
                stationName: nil,
                radiotext: nil
            )
        }
    }

    func tuneDAB(freqIndex: Int) async throws {
        try await delay()
        state.tuner.band = .dab
        state.tuner.locked = true
        state.tuner.dab = DABTunerState(
            freqIndex: freqIndex,
            ficQuality: 78,
            cnrDb: 24,
            playingServiceId: 101,
            playingComponentId: 1,
            dynamicLabel: "DAB Mock Ensemble"
        )
    }

    func playDAB(serviceId: Int, componentId: Int) async throws {
        try await delay()
        state.tuner.dab?.playingServiceId = serviceId
        state.tuner.dab?.playingComponentId = componentId
        state.tuner.dab?.dynamicLabel = "Now playing mock service"
    }

    func playScannedDAB(ensemble: Int, serviceId: Int, componentId: Int) async throws {
        try await tuneDAB(freqIndex: ensemble)
        try await playDAB(serviceId: serviceId, componentId: componentId)
    }

    func seekFM(direction: String) async throws {
        try await delay(0.02)
        let ordered = Self.fmHits.keys.sorted()
        let current = state.tuner.fm?.frequencyKhz ?? FMBandScan.minKhz
        let next: Int
        if direction == "down" {
            next = ordered.last(where: { $0 < current }) ?? ordered.last ?? FMBandScan.minKhz
        } else {
            next = ordered.first(where: { $0 > current }) ?? FMBandScan.minKhz
        }
        try await tuneFM(frequencyKhz: next)
    }

    func scanFM(maxSteps: Int, name: String?) async throws -> TunerScanResponse {
        try await delay(1.2)
        return TunerScanResponse(
            status: "found",
            band: "fm",
            steps: 8,
            frequencyKhz: 101_500,
            stationName: name ?? "Mock FM",
            freqIndex: nil,
            serviceId: nil,
            componentId: nil
        )
    }

    func scanDAB(maxSteps: Int, name: String?) async throws -> TunerScanResponse {
        try await delay(2.0)
        try await tuneDAB(freqIndex: 12)
        return TunerScanResponse(
            status: "found",
            band: "dab",
            steps: 6,
            frequencyKhz: nil,
            stationName: name ?? "Rai Radio 1",
            freqIndex: 12,
            serviceId: 101,
            componentId: 1
        )
    }

    func scanFullFM(progress: (@Sendable (Int, Int) -> Void)? = nil) async throws -> [FMScanHit] {
        try await delay(1.5)
        progress?(3, 3)
        return [
            FMScanHit(frequencyKhz: 97_500, rssiDbuv: 55, snrDb: 20, stationName: "Mock R1"),
            FMScanHit(frequencyKhz: 101_500, rssiDbuv: 48, snrDb: 18, stationName: "Mock R2"),
            FMScanHit(frequencyKhz: 105_200, rssiDbuv: 51, snrDb: 19, stationName: "Radio Italia")
        ]
    }

    func calibrateAntenna(antcap: Int) async throws {
        try await delay(0.5)
    }

    func setVolume(_ volume: Int) async throws {
        try await delay()
        let clamped = max(0, min(100, volume))
        state.tuner.volume = clamped
        let db = VolumeMapping.masterDb(forVolumePercent: clamped)
        state.audio.master.leftDb = db
        state.audio.master.rightDb = db
    }

    func listDABServices() async throws -> [DABService] {
        try await delay()
        return [
            DABService(serviceId: 101, componentId: 1, label: "Mock Radio 1"),
            DABService(serviceId: 102, componentId: 1, label: "Mock Radio 2"),
            DABService(serviceId: 103, componentId: 1, label: "Mock Jazz")
        ]
    }

    func saveStation(_ station: Station) async throws {
        try await delay()
        state.stations.append(station)
    }

    func updateStation(at index: Int, name: String) async throws {
        try await delay()
        guard state.stations.indices.contains(index) else { return }
        state.stations[index].name = name.trimmingCharacters(in: .whitespacesAndNewlines)
    }

    func removeStation(at index: Int) async throws {
        try await delay()
        guard state.stations.indices.contains(index) else { return }
        state.stations.remove(at: index)
    }

    func reorderStation(from: Int, to: Int) async throws {
        try await delay()
        guard state.stations.indices.contains(from), state.stations.indices.contains(to) else { return }
        let item = state.stations.remove(at: from)
        state.stations.insert(item, at: to)
    }

    func tuneStation(at index: Int) async throws {
        try await delay()
        guard state.stations.indices.contains(index) else { return }
        let station = state.stations[index]
        switch station.band {
        case .fm:
            if let freq = station.fmFrequencyKhz { try await tuneFM(frequencyKhz: freq) }
        case .dab:
            if let idx = station.dabFreqIndex { try await tuneDAB(freqIndex: idx) }
            if let sid = station.dabServiceId, let cid = station.dabComponentId {
                try await playDAB(serviceId: sid, componentId: cid)
            }
        }
    }

    func applyAudioProfile(_ profile: AudioProfileDTO) async throws {
        try await delay()
        let safe = profile.clampedForDevice()
        state.audio.activeSource = safe.activeSource
        state.audio.master = safe.master
        state.audio.eq = safe.eq
        state.audio.enhancements = safe.enhancements
    }

    func applyAudioPreset(_ preset: AudioProfileTemplate) async throws {
        try await delay()
        let master = state.audio.master
        let source = state.streaming.enabled ? state.audio.activeSource : .radio
        var dto = preset.profileDTO(
            activeSource: source,
            master: master,
            deviceEQ: state.audio.eq
        )
        dto.master = master
        dto.enhancements = EnhancementsState(stereoLevel: 0, bassLevel: 0)
        try await applyAudioProfile(dto)
        state.audio.enhancements = preset.enhancements
    }

    func setStereoEnhance(level: Int) async throws {
        try await delay()
        state.audio.enhancements.stereoLevel = level
    }

    func setBassEnhance(level: Int) async throws {
        try await delay()
        state.audio.enhancements.bassLevel = level
    }

    func resetAudio() async throws {
        try await delay()
        state.audio = AudioState()
    }

    func fetchAudioLevels() async throws -> AudioLevelsDTO {
        try await delay(0.02)
        let live = state.connection.isConnected
        let output = live ? -12.0 - Double((state.tuner.volume % 20)) / 4.0 : -60
        return AudioLevelsDTO(
            radioInLeftDb: output - 0.4,
            radioInRightDb: output - 0.3,
            bluetoothInLeftDb: output - 1.0,
            bluetoothInRightDb: output - 1.1,
            outputLeftDb: output,
            outputRightDb: output - 0.2
        )
    }

    func scanBluetooth(seconds: Int) async throws {
        try await delay(0.8)
        guard state.bluetooth.booted else {
            throw DigiRadioError.httpStatus(400, "{\"reason\":\"not_booted\"}")
        }
        state.bluetooth.nearbyDevices = [
            BluetoothDevice(index: 0, mac: "AA:BB:CC:DD:EE:FF", name: "Bose Solo II", rssiDbm: -52),
            BluetoothDevice(index: 1, mac: "11:22:33:44:55:66", name: "Living Room", rssiDbm: -68)
        ]
    }

    func setBluetoothEnabled(_ enabled: Bool) async throws {
        try await delay()
        state.bluetooth.booted = enabled
        if enabled {
            state.bluetooth.a2dpState = .standby
        } else {
            state.bluetooth.pairing = false
            state.bluetooth.a2dpState = .unknown
            state.bluetooth.deviceName = ""
            state.bluetooth.nearbyDevices = []
        }
    }

    func connectBluetooth(mac: String, name: String?, save: Bool) async throws {
        try await delay()
        state.bluetooth.a2dpState = .streaming
        state.bluetooth.deviceName = name ?? "Speaker"
        if save {
            state.bluetooth.savedSpeaker = SavedSpeaker(mac: mac, name: name ?? "Speaker")
        }
    }

    func reconnectBluetooth() async throws {
        try await delay()
        state.bluetooth.a2dpState = .streaming
    }

    func startBluetoothPairing() async throws {
        try await delay()
        state.bluetooth.pairing = true
    }

    func stopBluetoothPairing() async throws {
        try await delay()
        state.bluetooth.pairing = false
    }

    func disconnectBluetoothA2DP() async throws {
        try await delay()
        state.bluetooth.a2dpState = .standby
    }

    func clearBluetoothSpeaker() async throws {
        try await delay()
        state.bluetooth.savedSpeaker = nil
    }

    func playAudioBeep(enabled: Bool) async throws {
        try await delay(0.1)
    }

    func fetchDspParams() async throws -> DspParamsResponse {
        try await delay(0.1)
        let params = AdauParamCatalog.entries.map {
            DspParamEntry(name: $0.name, address: $0.address)
        }
        return DspParamsResponse(params: params)
    }

    func setDspParam(name: String, value: Double) async throws {
        try await delay(0.1)
    }

    func setBluetoothAutoReconnect(times: Int) async throws {
        try await delay()
        state.bluetooth.autoReconnect = times
    }

    func fetchA2DPCodec() async throws {
        try await delay(0.1)
    }

    func setA2DPCodec(mask: Int) async throws -> String? {
        try await delay(0.1)
        a2dpCodecMask = mask
        state.bluetooth.negotiatedCodec = mask == 0 ? .sbc : .aptxHD
        return nil
    }

    private var a2dpCodecMask = A2DPCodecMask.mask(from: A2DPCodecMask.defaultEnabled)

    private func delay(_ seconds: Double = 0.35) async throws {
        try await Task.sleep(for: .seconds(seconds))
    }

    private func seed() {
        state.health = HealthState(
            status: "ok",
            firmware: "0.9.0-mock",
            serialNumber: "CC4DB4MOCK01",
            chips: ChipHealth(si4684: true, adau1701: true, bt1035: true)
        )
        state.tuner = TunerState(
            booted: true,
            band: .fm,
            locked: true,
            volume: 42,
            fm: FMTunerState(
                frequencyKhz: 102_300,
                rssiDbuv: 51,
                snrDb: 19,
                stereo: true,
                stationName: "Radio Italia",
                radiotext: "Mock mode — nessun hardware collegato"
            ),
            dab: nil
        )
        state.audio = AudioState(
            activeSource: .radio,
            master: MasterVolumeState(leftDb: -6, rightDb: -6),
            eq: (0 ..< 6).map { i in
                EQBandState(
                    index: i,
                    gainDb: 0,
                    centerHz: AudioProfileTemplate.bandCenters[i],
                    q: AudioProfileTemplate.bandQ[i]
                )
            },
            enhancements: EnhancementsState(stereoLevel: 20, bassLevel: 15)
        )
        state.bluetooth = BluetoothState(
            booted: true,
            pairing: false,
            a2dpState: .streaming,
            deviceName: "Bose Solo II",
            autoReconnect: 5,
            savedSpeaker: SavedSpeaker(mac: "BC:87:FA:E6:9D:6E", name: "Bose Solo II"),
            negotiatedCodec: .aptxHD
        )
        state.stations = [
            Station(name: "Radio Italia", band: .fm, fmFrequencyKhz: 102_300),
            Station(name: "RDS", band: .fm, fmFrequencyKhz: 97_500)
        ]
        state.streaming = StreamingState(enabled: false, url: "")
        state.connection.isConnected = true
        state.connection.host = "digiradio-mock.local"
    }
}
