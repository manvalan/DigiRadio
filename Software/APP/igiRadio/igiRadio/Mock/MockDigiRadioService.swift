import Foundation
import Observation

@Observable
final class MockDigiRadioService: DigiRadioService {
    private(set) var state = DigiRadioState()

    init() {
        seed()
    }

    func connect(host: String) async throws {
        try await delay()
        state.connection.host = host
        state.connection.isConnected = true
        state.connection.lastError = nil
    }

    func disconnect() {
        state.connection = ConnectionState()
        seed()
    }

    func refreshHealth() async throws {
        try await delay()
    }

    func refreshTunerStatus() async throws {
        try await delay()
    }

    func refreshAudioProfile() async throws {
        try await delay()
    }

    func refreshBluetoothStatus() async throws {
        try await delay()
    }

    func refreshStations() async throws {
        try await delay()
    }

    func refreshStreaming() async throws {
        try await delay()
    }

    func setStreaming(enabled: Bool, url: String) async throws {
        try await delay()
        state.streaming = StreamingState(enabled: enabled, url: url)
    }

    func tuneFM(frequencyKhz: Int) async throws {
        try await delay()
        state.tuner.band = .fm
        state.tuner.locked = true
        state.tuner.fm = FMTunerState(
            frequencyKhz: frequencyKhz,
            rssiDbuv: 48,
            snrDb: 22,
            stereo: true,
            stationName: "Radio Mock",
            radiotext: "igiRadio demo mode"
        )
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

    func seekFM(direction: String) async throws {
        try await delay()
        guard var fm = state.tuner.fm else { return }
        let step = direction == "down" ? -100 : 100
        fm.frequencyKhz = max(64_000, min(108_000, fm.frequencyKhz + step))
        state.tuner.fm = fm
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

    func scanFullFM() async throws -> [FMScanHit] {
        try await delay(1.5)
        return [
            FMScanHit(frequencyKhz: 97_500, rssiDbuv: 55, snrDb: 20, stationName: "Mock R1"),
            FMScanHit(frequencyKhz: 101_500, rssiDbuv: 48, snrDb: 18, stationName: "Mock R2"),
            FMScanHit(frequencyKhz: 102_300, rssiDbuv: 51, snrDb: 19, stationName: "Radio Italia")
        ]
    }

    func setVolume(_ volume: Int) async throws {
        try await delay()
        let clamped = max(0, min(100, volume))
        state.tuner.volume = clamped
        state.audio.master.leftDb = Double(clamped)
        state.audio.master.rightDb = Double(clamped)
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
        state.audio.mixer = profile.mixer
        state.audio.master = profile.master
        state.audio.eq = profile.eq
        state.audio.enhancements = profile.enhancements
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

    func scanBluetooth(seconds: Int) async throws {
        try await delay(0.8)
        state.bluetooth.nearbyDevices = [
            BluetoothDevice(index: 0, mac: "AA:BB:CC:DD:EE:FF", name: "Bose Solo II", rssiDbm: -52),
            BluetoothDevice(index: 1, mac: "11:22:33:44:55:66", name: "Living Room", rssiDbm: -68)
        ]
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

    private func delay(_ seconds: Double = 0.35) async throws {
        try await Task.sleep(for: .seconds(seconds))
    }

    private func seed() {
        state.health = HealthState(
            status: "ok",
            firmware: "0.8.5-mock",
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
            mixer: MixerState(),
            master: MasterVolumeState(leftDb: 42, rightDb: 42),
            eq: (0 ..< 6).map { i in
                EQBandState(index: i, gainDb: 0, centerHz: [40, 120, 400, 1000, 3000, 10000][i], q: 1.414)
            },
            enhancements: EnhancementsState(stereoLevel: 20, bassLevel: 15)
        )
        state.bluetooth = BluetoothState(
            booted: true,
            pairing: false,
            a2dpState: .streaming,
            deviceName: "Bose Solo II",
            autoReconnect: 5,
            savedSpeaker: SavedSpeaker(mac: "BC:87:FA:E6:9D:6E", name: "Bose Solo II")
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
