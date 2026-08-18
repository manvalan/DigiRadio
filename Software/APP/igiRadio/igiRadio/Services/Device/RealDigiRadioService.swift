import Foundation
import Observation
import OSLog

@Observable
final class RealDigiRadioService: DigiRadioService {
    private(set) var state = DigiRadioState()
    private let client = HTTPDigiRadioClient()
    private let logger = Logger(subsystem: "com.digiradio.igiRadio", category: "Device")

    func connect(host: String) async throws {
        state.connection.isConnecting = true
        state.connection.lastError = nil
        defer { state.connection.isConnecting = false }

        try client.setHost(host)
        state.connection.host = host
        try await refreshHealth()
        state.connection.isConnected = true
        logger.info("Connected to \(host, privacy: .public)")
    }

    func disconnect() {
        client.clearHost()
        state.connection = ConnectionState()
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

    func refreshTunerStatus() async throws {
        let tuner = try await client.fetchTunerStatus()
        applyTuner(tuner)
    }

    func refreshAudioProfile() async throws {
        let profile = try await client.fetchAudioProfile()
        state.audio.mixer = profile.mixer
        state.audio.master = profile.master
        state.audio.eq = profile.eq.enumerated().map { index, band in
            EQBandState(index: index, gainDb: band.gainDb, centerHz: band.centerHz, q: band.q)
        }
        state.audio.enhancements = profile.enhancements
    }

    func refreshBluetoothStatus() async throws {
        let bt = try await client.fetchBluetoothStatus()
        state.bluetooth.booted = bt.booted
        state.bluetooth.pairing = bt.pairing
        state.bluetooth.a2dpState = A2DPState(apiValue: bt.a2dp)
        state.bluetooth.deviceName = bt.deviceName
        state.bluetooth.autoReconnect = bt.autoReconnect

        let speaker = try await client.fetchSavedSpeaker()
        if speaker.configured, let mac = speaker.mac, let name = speaker.name {
            state.bluetooth.savedSpeaker = SavedSpeaker(mac: mac, name: name)
        } else {
            state.bluetooth.savedSpeaker = nil
        }
    }

    func refreshStations() async throws {
        let list = try await client.fetchStations()
        state.stations = list.stations
    }

    func refreshStreaming() async throws {
        state.streaming = try await client.fetchStreaming()
    }

    func tuneFM(frequencyKhz: Int) async throws {
        let tuner = try await client.tune(body: TuneRequest(band: "fm", freqIndex: nil, frequencyKhz: frequencyKhz))
        applyTuner(tuner)
    }

    func tuneDAB(freqIndex: Int) async throws {
        let tuner = try await client.tune(body: TuneRequest(band: "dab", freqIndex: freqIndex, frequencyKhz: nil))
        applyTuner(tuner)
    }

    func playDAB(serviceId: Int, componentId: Int) async throws {
        try await client.playDAB(body: PlayRequest(serviceId: serviceId, componentId: componentId))
        try await refreshTunerStatus()
    }

    func seekFM(direction: String) async throws {
        _ = try await client.seekFM(direction: direction)
        try await refreshTunerStatus()
    }

    func scanFM(maxSteps: Int, name: String?) async throws -> TunerScanResponse {
        try await client.scanStation(body: TunerScanRequest(band: "fm", maxSteps: maxSteps, name: name))
    }

    func scanFullFM() async throws -> [FMScanHit] {
        let response = try await client.scanFullFM()
        return response.stations
    }

    func setVolume(_ volume: Int) async throws {
        var profile = try await client.fetchAudioProfile()
        let clamped = max(0, min(100, volume))
        profile.master.leftDb = Double(clamped)
        profile.master.rightDb = Double(clamped)
        try await client.putAudioProfile(profile)
        state.tuner.volume = clamped
        state.audio.master = profile.master
    }

    func listDABServices() async throws -> [DABService] {
        let response = try await client.fetchDABServices()
        return response.services
    }

    func saveStation(_ station: Station) async throws {
        try await client.addStation(station)
        try await refreshStations()
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
        try await client.tuneStation(index: index)
        try await refreshTunerStatus()
    }

    func applyAudioProfile(_ profile: AudioProfileDTO) async throws {
        try await client.putAudioProfile(profile)
        state.audio.mixer = profile.mixer
        state.audio.master = profile.master
        state.audio.eq = profile.eq
        state.audio.enhancements = profile.enhancements
    }

    func setStereoEnhance(level: Int) async throws {
        try await client.setEnhance(path: "/api/audio/stereo-enhance", level: level)
        state.audio.enhancements.stereoLevel = level
    }

    func setBassEnhance(level: Int) async throws {
        try await client.setEnhance(path: "/api/audio/bass-enhance", level: level)
        state.audio.enhancements.bassLevel = level
    }

    func resetAudio() async throws {
        try await client.resetAudio()
        try await refreshAudioProfile()
    }

    func scanBluetooth(seconds: Int) async throws {
        let response = try await client.scanBluetooth(seconds: seconds)
        state.bluetooth.nearbyDevices = response.devices.map {
            BluetoothDevice(index: $0.index, mac: $0.mac, name: $0.name, rssiDbm: $0.rssiDbm)
        }
    }

    func connectBluetooth(mac: String, name: String?, save: Bool) async throws {
        try await client.connectBluetooth(mac: mac, name: name, save: save)
        try await refreshBluetoothStatus()
    }

    func reconnectBluetooth() async throws {
        try await client.reconnectBluetooth()
        try await refreshBluetoothStatus()
    }

    private func applyTuner(_ tuner: TunerStatusResponse) {
        state.tuner.booted = tuner.booted
        state.tuner.band = TunerBand(rawValue: tuner.band) ?? .fm
        state.tuner.locked = tuner.locked
        state.tuner.volume = tuner.volume
        state.tuner.fm = tuner.fm
        state.tuner.dab = tuner.dab
    }
}
