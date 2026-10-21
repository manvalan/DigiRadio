import XCTest
@testable import igiRadio

final class HTTPDecoderTests: XCTestCase {
    func testDecodeHealth() throws {
        let json = """
        {"status":"ok","fw":"0.9.0","serialNumber":"CC4DB4MOCK01",
         "chips":{"si4684":true,"adau1701":true,"bt1035":true}}
        """.data(using: .utf8)!
        let health = try JSONDecoder.api.decode(HealthResponse.self, from: json)
        XCTAssertEqual(health.status, "ok")
        XCTAssertEqual(health.fw, "0.9.0")
        XCTAssertEqual(health.serialNumber, "CC4DB4MOCK01")
        XCTAssertTrue(health.chips.si4684)
    }

    func testDecodeFirmwareAFFollow() throws {
        let json = """
        {"enabled":true,"switches":2,"last_from_khz":98400,"last_to_khz":87600}
        """.data(using: .utf8)!
        let af = try JSONDecoder.api.decode(FirmwareAFState.self, from: json)
        XCTAssertTrue(af.enabled)
        XCTAssertEqual(af.switches, 2)
        XCTAssertEqual(af.lastFromKhz, 98400)
        XCTAssertEqual(af.lastToKhz, 87600)

        let fresh = try JSONDecoder.api.decode(
            FirmwareAFState.self,
            from: #"{"enabled":false,"switches":0}"#.data(using: .utf8)!
        )
        XCTAssertFalse(fresh.enabled)
        XCTAssertNil(fresh.lastToKhz)
    }

    func testDecodeBluetoothPaired() throws {
        let json = """
        {"devices":[{"index":1,"mac":"001122334455","name":"Phone"}]}
        """.data(using: .utf8)!
        let paired = try JSONDecoder.api.decode(BluetoothPairedResponse.self, from: json)
        XCTAssertEqual(paired.devices.count, 1)
        XCTAssertEqual(paired.devices[0].name, "Phone")
    }

    func testDecodeDABScanResult() throws {
        let json = """
        {"status":"found","band":"dab","steps":6,"freq_index":12,
         "station_name":"Rai Radio 1","service_id":12345,"component_id":1}
        """.data(using: .utf8)!
        let scan = try JSONDecoder.api.decode(TunerScanResponse.self, from: json)
        XCTAssertEqual(scan.status, "found")
        XCTAssertEqual(scan.freqIndex, 12)
        XCTAssertEqual(scan.serviceId, 12345)
        XCTAssertEqual(scan.componentId, 1)
    }

    func testDecodeTunerFM() throws {
        let json = """
        {"booted":true,"band":"fm","locked":true,"volume":50,
         "fm":{"frequency_khz":102300,"rssi_dbuv":48,"snr_db":20,"stereo":true,
               "station_name":"Radio Test","radiotext":"Hello","af_khz":[105300,102700]},
         "dab":null}
        """.data(using: .utf8)!
        let tuner = try JSONDecoder.api.decode(TunerStatusResponse.self, from: json)
        XCTAssertEqual(tuner.band, "fm")
        XCTAssertEqual(tuner.fm?.frequencyKhz, 102300)
        XCTAssertEqual(tuner.fm?.stationName, "Radio Test")
        XCTAssertEqual(tuner.fm?.afKhz, [105300, 102700])
    }

    func testDecodeTunerDABWithoutFreqIndex() throws {
        let json = """
        {"booted":true,"band":"dab","locked":true,"volume":40,
         "dab":{"fic_quality":80,"cnr_db":25},"fm":null}
        """.data(using: .utf8)!
        let tuner = try JSONDecoder.api.decode(TunerStatusResponse.self, from: json)
        XCTAssertEqual(tuner.band, "dab")
        XCTAssertEqual(tuner.dab?.freqIndex, 0)
        XCTAssertEqual(tuner.dab?.ficQuality, 80)
    }

    func testDecodeAudioProfile() throws {
        let json = """
        {"active_source":"radio",
         "master":{"left_db":10,"right_db":10},
         "eq":[{"gain_db":0,"center_hz":40,"q":1.414,"locked":true}],
         "enhancements":{"stereo_level":5,"bass_level":7}}
        """.data(using: .utf8)!
        let profile = try JSONDecoder.api.decode(AudioProfileDTO.self, from: json)
        XCTAssertEqual(profile.activeSource, .radio)
        XCTAssertEqual(profile.master.leftDb, 10)
        XCTAssertEqual(profile.enhancements.bassLevel, 7)
        XCTAssertTrue(profile.eq[0].isLocked)
    }

    func testDecodeFMScanFull() throws {
        let json = """
        {"stations":[{"frequency_khz":98300,"rssi_dbuv":12,"snr_db":13,"station_name":"RADIO 1"},
         {"frequency_khz":105500,"rssi_dbuv":-4,"snr_db":-2}]}
        """.data(using: .utf8)!
        let scan = try JSONDecoder.api.decode(FMScanFullResponse.self, from: json)
        XCTAssertEqual(scan.stations.count, 2)
        XCTAssertEqual(scan.stations[0].stationName, "RADIO 1")
        XCTAssertNil(scan.stations[1].stationName)
        XCTAssertEqual(ScannedStation.fromFM(scan.stations[0]).name, "RADIO 1")
    }

    func testActiveSourceBluetoothDecode() throws {
        let json = """
        {"active_source":"bluetooth","master":{"left_db":0,"right_db":0},
         "eq":[],"enhancements":{"stereo_level":0,"bass_level":0}}
        """.data(using: .utf8)!
        let profile = try JSONDecoder.api.decode(AudioProfileDTO.self, from: json)
        XCTAssertEqual(profile.activeSource, .bluetooth)
    }

    func testDecodeBluetoothStatusPoweredOff() throws {
        let json = """
        {"booted":false,"pairing":false,"a2dp":"standby",
         "device_name":"","auto_reconnect":0}
        """.data(using: .utf8)!
        let status = try JSONDecoder.api.decode(BluetoothStatusResponse.self, from: json)
        XCTAssertFalse(status.booted)
        XCTAssertEqual(status.a2dp, "standby")
        XCTAssertEqual(A2DPState(apiValue: status.a2dp), .standby)
    }

    func testDecodeWifiResetResponse() throws {
        let json = #"{"status":"reset","reboot_in_sec":3}"#.data(using: .utf8)!
        let response = try JSONDecoder.api.decode(WiFiResetResponse.self, from: json)
        XCTAssertEqual(response.status, "reset")
        XCTAssertEqual(response.rebootInSec, 3)
    }

    func testDecodeBluetoothPowerResponses() throws {
        let enabled = try JSONDecoder.api.decode(
            StatusResponse.self,
            from: #"{"status":"enabled"}"#.data(using: .utf8)!
        )
        let disabled = try JSONDecoder.api.decode(
            StatusResponse.self,
            from: #"{"status":"disabled"}"#.data(using: .utf8)!
        )
        XCTAssertEqual(enabled.status, "enabled")
        XCTAssertEqual(disabled.status, "disabled")
    }

    func testA2DPStateMapping() {
        XCTAssertEqual(A2DPState(apiValue: "streaming"), .streaming)
        XCTAssertEqual(A2DPState(apiValue: "unknown-value"), .unknown)
    }

    func testBLENameToHost() {
        XCTAssertEqual(
            DigiRadioDiscoveryService.httpHost(fromBLEName: "igiRadio-CC4DB4"),
            "http://igiradio-cc4db4.local"
        )
        XCTAssertEqual(
            DigiRadioDiscoveryService.httpHost(fromBLEName: "DigiRadio-CC4DB4"),
            "http://digiradio-cc4db4.local"
        )
        XCTAssertNil(DigiRadioDiscoveryService.httpHost(fromBLEName: "igiRadio"))
        XCTAssertNil(DigiRadioDiscoveryService.httpHost(fromBLEName: "OtherDevice"))
        XCTAssertTrue(DeviceIdentity.matchesAdvertisedName("igiRadio-CC4DB4"))
        XCTAssertTrue(DeviceIdentity.matchesBonjourServiceName("igiradio-CC4DB4"))
        XCTAssertFalse(DeviceIdentity.matchesAdvertisedName("OtherDevice"))
        XCTAssertEqual(
            DeviceIdentity.connectionCandidates(from: "http://digiradio-cc4db4.local").contains("http://igiradio-cc4db4.local"),
            true
        )
    }

    func testHostNormalizer() {
        XCTAssertEqual(
            HostNormalizer.httpURL(from: "igiRadio-CC4DB4.local"),
            "http://igiradio-cc4db4.local"
        )
        XCTAssertEqual(
            HostNormalizer.httpURL(from: "DigiRadio-CC4DB4.local"),
            "http://digiradio-cc4db4.local"
        )
        XCTAssertEqual(
            HostNormalizer.httpURL(from: "http://192.168.1.56"),
            "http://192.168.1.56"
        )
        XCTAssertEqual(
            HostNormalizer.displayHost(from: "http://digiradio-cc4db4.local"),
            "digiradio-cc4db4.local"
        )
    }

    func testBtMacNormalizer() {
        XCTAssertEqual(BtMacNormalizer.forApi("AA:BB:CC:DD:EE:FF"), "AABBCCDDEEFF")
        XCTAssertEqual(BtMacNormalizer.forApi("112233445566"), "112233445566")
        XCTAssertEqual(BtMacNormalizer.forApi("000C8A78AB62"), "000C8A78AB62")
        XCTAssertNil(BtMacNormalizer.forApi("invalid"))
    }

    func testFriendlyBluetoothErrorsPointToSpeakerNotSetupBLE() {
        let notBooted = DigiRadioError.httpStatus(400, "not_booted")
        let timeout = DigiRadioError.httpStatus(500, "at_timeout")
        XCTAssertTrue(
            DigiRadioFriendlyError.message(for: notBooted)
                .contains("Altoparlante")
        )
        XCTAssertFalse(
            DigiRadioFriendlyError.message(for: timeout)
                .localizedCaseInsensitiveContains("associazione")
        )
        XCTAssertTrue(
            DigiRadioFriendlyError.message(for: timeout)
                .contains("pairing")
        )
    }

    func testDecodeA2DPCodec() throws {
        let json = """
        {"codec":"aptx-hd"}
        """.data(using: .utf8)!
        let response = try JSONDecoder.api.decode(A2DPCodecResponse.self, from: json)
        XCTAssertEqual(A2DPCodec(apiValue: response.codec), .aptxHD)
        XCTAssertEqual(A2DPCodec(apiValue: response.codec).displayName, "aptX HD")
    }

    func testA2DPCodecUnknownValue() {
        let codec = A2DPCodec(apiValue: "some-new-codec")
        if case let .unknown(raw) = codec {
            XCTAssertEqual(raw, "some-new-codec")
        } else {
            XCTFail("Expected unknown codec")
        }
    }

    func testA2DPCodecMask() {
        let enabled: Set<A2DPCodecMask> = [.aptx, .aptxHD]
        XCTAssertEqual(A2DPCodecMask.mask(from: enabled), 10)
        XCTAssertEqual(A2DPCodecMask.set(from: 10), enabled)
    }

    func testScannedStationMatchesPreset() {
        let withRDS = ScannedStation.fromFM(
            FMScanHit(frequencyKhz: 102_300, rssiDbuv: 50, snrDb: 18, stationName: "Radio Test")
        )
        XCTAssertTrue(withRDS.hasBroadcastName)
        XCTAssertEqual(withRDS.name, "Radio Test")

        let withoutRDS = ScannedStation.fromFM(
            FMScanHit(frequencyKhz: 101_500, rssiDbuv: 48, snrDb: 16, stationName: nil)
        )
        XCTAssertFalse(withoutRDS.hasBroadcastName)
        XCTAssertEqual(withoutRDS.asPreset(named: "La mia radio").name, "La mia radio")

        let saved = Station(name: "Radio Test", band: .fm, fmFrequencyKhz: 102_300)
        XCTAssertTrue(withRDS.matches(saved))
    }

    func testStationNameDisplayIgnoresFrequencyLabels() {
        XCTAssertEqual(StationNameDisplay.meaningful("Radio Test", khz: 102_300), "Radio Test")
        XCTAssertNil(StationNameDisplay.meaningful("FM 102.30", khz: 102_300))
        XCTAssertNil(StationNameDisplay.meaningful("102.30 MHz", khz: 102_300))
        XCTAssertNil(StationNameDisplay.meaningful("  ", khz: 102_300))
        XCTAssertEqual(
            Station(name: "Radio Test", band: .fm, fmFrequencyKhz: 102_300).displayLabel,
            "Radio Test"
        )
        XCTAssertEqual(
            Station(name: "FM 101.50", band: .fm, fmFrequencyKhz: 101_500).displayLabel,
            "FM 101.50 MHz"
        )
    }

    func testRDSTextStripsNullBytes() {
        XCTAssertEqual(RDSText.sanitized("RMC"), "RMC")
        XCTAssertEqual(
            RDSText.sanitized("\0\0\0 Get You\0\0y"),
            "Get You y"
        )
        XCTAssertNil(RDSText.sanitized("\0\0\0"))
        XCTAssertEqual(StationNameDisplay.meaningful("\0RMC\0", khz: 106_700), "RMC")
    }

    func testIngestLiveFMUpdatesCatalogAndPlaceholderFavorite() {
        let mock = MockDigiRadioService()
        let vm = StationScanViewModel(service: mock)
        vm.scannedFM = [
            ScannedStation.fromFM(FMScanHit(frequencyKhz: 106_700, rssiDbuv: 25, snrDb: 13, stationName: nil))
        ]
        vm.favoriteStations = vm.scannedFM
        vm.ingestLiveFM(khz: 106_700, stationName: "RMC")
        XCTAssertEqual(vm.scannedFM.first?.name, "RMC")
        XCTAssertEqual(vm.favoriteStations.first?.name, "RMC")
        XCTAssertTrue(vm.scannedFM.first?.hasBroadcastName ?? false)
        vm.favoriteStations[0].name = "La mia radio"
        vm.favoriteStations[0].hasBroadcastName = false
        vm.ingestLiveFM(khz: 106_700, stationName: "MONTECARLO")
        XCTAssertEqual(vm.favoriteStations.first?.name, "La mia radio")
        XCTAssertEqual(vm.scannedFM.first?.name, "MONTECARLO")
    }

    func testFMStationMergeGroupsSameRDSName() {
        let weaker = ScannedStation.fromFM(
            FMScanHit(frequencyKhz: 98_100, rssiDbuv: 22, snrDb: 14, stationName: "RMC")
        )
        let stronger = ScannedStation.fromFM(
            FMScanHit(frequencyKhz: 102_300, rssiDbuv: 48, snrDb: 18, stationName: "RMC")
        )
        let merged = FMStationMerge.merge([weaker, stronger])
        XCTAssertEqual(merged.count, 1)
        XCTAssertEqual(merged[0].name, "RMC")
        XCTAssertEqual(merged[0].fmFrequencyKhz, 102_300)
        XCTAssertEqual(merged[0].allFMFrequenciesKhz, [98_100, 102_300])
        XCTAssertEqual(merged[0].subtitle, "FM 98.10 · 102.30 MHz")
    }

    func testFMStationMergeDoesNotGroupShortRDSNames() {
        let a = ScannedStation.fromFM(
            FMScanHit(frequencyKhz: 98_100, rssiDbuv: 22, snrDb: 14, stationName: "RA")
        )
        let b = ScannedStation.fromFM(
            FMScanHit(frequencyKhz: 102_300, rssiDbuv: 48, snrDb: 18, stationName: "RA")
        )
        XCTAssertNil(FMStationMerge.groupingKey(a))
        XCTAssertEqual(FMStationMerge.merge([a, b]).count, 2)
    }

    func testFMSeekScanCollectsDistinctRDSStations() async throws {
        let mock = MockDigiRadioService()
        try await mock.connect(host: "mock.local")
        let vm = StationScanViewModel(service: mock)
        vm.scannedFM = []
        await vm.startScan()
        XCTAssertFalse(vm.isScanning)
        XCTAssertNil(vm.errorMessage)
        XCTAssertEqual(vm.scannedFM.count, 3)
        XCTAssertTrue(vm.scannedFM.contains { $0.name == "RDS" })
        XCTAssertTrue(vm.scannedFM.contains { $0.name == "Radio Mock" })
        XCTAssertTrue(vm.scannedFM.contains { $0.name == "Radio Italia" })
    }

    func testFMStationMergeKeepsUnnamedFrequenciesSeparate() {
        let a = ScannedStation.fromFM(
            FMScanHit(frequencyKhz: 98_100, rssiDbuv: 22, snrDb: 14, stationName: nil)
        )
        let b = ScannedStation.fromFM(
            FMScanHit(frequencyKhz: 102_300, rssiDbuv: 48, snrDb: 18, stationName: nil)
        )
        XCTAssertEqual(FMStationMerge.merge([a, b]).count, 2)
    }

    func testIngestLiveFMAddsAlternateFrequencyToSameName() {
        let mock = MockDigiRadioService()
        let vm = StationScanViewModel(service: mock)
        vm.scannedFM = [
            ScannedStation.fromFM(
                FMScanHit(frequencyKhz: 98_100, rssiDbuv: 22, snrDb: 14, stationName: "RMC")
            )
        ]
        vm.ingestLiveFM(khz: 102_300, stationName: "RMC")
        XCTAssertEqual(vm.scannedFM.count, 1)
        XCTAssertTrue(vm.scannedFM[0].containsFMFrequency(98_100))
        XCTAssertTrue(vm.scannedFM[0].containsFMFrequency(102_300))
    }

    func testAlternateFrequencyHuntSwitchesWhenMuchStronger() {
        let current = AlternateFrequencyHunt.Snapshot(khz: 98_100, rssiDbuv: 8, locked: true)
        let better = AlternateFrequencyHunt.Snapshot(khz: 102_300, rssiDbuv: 40, locked: true)
        XCTAssertTrue(AlternateFrequencyHunt.isWeak(current))
        XCTAssertTrue(AlternateFrequencyHunt.shouldSwitch(from: current, to: better))
        let similar = AlternateFrequencyHunt.Snapshot(khz: 102_300, rssiDbuv: 10, locked: true)
        XCTAssertFalse(AlternateFrequencyHunt.shouldSwitch(from: current, to: similar))
        let strong = AlternateFrequencyHunt.Snapshot(khz: 98_100, rssiDbuv: 40, locked: true)
        XCTAssertFalse(AlternateFrequencyHunt.isWeak(strong))
    }

    func testMergingPresetEQPreservesEnhancements() {
        let live = AudioProfileDTO(
            activeSource: .radio,
            master: MasterVolumeState(leftDb: -12, rightDb: -12),
            eq: AudioProfileTemplate.eqBands(gains: [0, 0, 0, 0, 0, 0]),
            enhancements: EnhancementsState(stereoLevel: 5, bassLevel: 10)
        )
        let rock = AudioProfileLibrary.builtIn.first { $0.id == "builtin.rock" }!
        let merged = live.mergingPresetEQ(rock)
        XCTAssertEqual(merged.enhancements.stereoLevel, 5)
        XCTAssertEqual(merged.eq[1].gainDb, 4, accuracy: 0.01)
    }

    func testFriendlyError503() {
        let message = DigiRadioFriendlyError.message(
            for: DigiRadioError.httpStatus(503, nil)
        )
        XCTAssertTrue(message.contains("non disponibile"))
    }

    func testVolumeMappingWithinGainDbRange() {
        XCTAssertEqual(VolumeMapping.masterDb(forVolumeLevel: 0), -96, accuracy: 0.01)
        XCTAssertEqual(VolumeMapping.masterDb(forVolumeLevel: 100), 12, accuracy: 0.01)
        XCTAssertEqual(VolumeMapping.volumeLevel(forMasterDb: -96), 0, accuracy: 0.01)
        XCTAssertEqual(VolumeMapping.volumeLevel(forMasterDb: 12), 100, accuracy: 0.01)
        // Metà fader = unity (0 dB); la zona d’ascolto non è più in fondo.
        XCTAssertEqual(VolumeMapping.masterDb(forVolumeLevel: 50), 0, accuracy: 0.2)
        XCTAssertEqual(VolumeMapping.volumeLevel(forMasterDb: 0), 50, accuracy: 0.5)
        XCTAssertGreaterThan(VolumeMapping.masterDb(forVolumeLevel: 25), -30)
        XCTAssertLessThan(VolumeMapping.masterDb(forVolumeLevel: 25), -12)
        XCTAssertEqual(GainDbClamp.clamp(-100), -96, accuracy: 0.01)
        XCTAssertEqual(GainDbClamp.clamp(20), 12, accuracy: 0.01)
        for level in [1.0, 25.0, 50.0, 75.0, 99.0] {
            let db = VolumeMapping.masterDb(forVolumeLevel: level)
            XCTAssertGreaterThanOrEqual(db, -96)
            XCTAssertLessThanOrEqual(db, 12)
            XCTAssertEqual(VolumeMapping.volumeLevel(forMasterDb: db), level, accuracy: 0.6)
        }
    }

    func testEncodeAudioProfilePUTBody() throws {
        let profile = AudioProfileDTO(
            activeSource: .radio,
            master: MasterVolumeState(leftDb: -6, rightDb: -6),
            eq: [
                EQBandState(index: 0, gainDb: 0, centerHz: 20, q: 1.414, isLocked: true),
                EQBandState(index: 1, gainDb: 0, centerHz: 100, q: 1, isLocked: false),
                EQBandState(index: 2, gainDb: 0, centerHz: 400, q: 1, isLocked: false),
                EQBandState(index: 3, gainDb: 0, centerHz: 1000, q: 1, isLocked: false),
                EQBandState(index: 4, gainDb: 0, centerHz: 3000, q: 1, isLocked: false),
                EQBandState(index: 5, gainDb: 0, centerHz: 8000, q: 1, isLocked: false),
            ],
            enhancements: EnhancementsState(stereoLevel: 0, bassLevel: 0, voiceClean: true)
        )
        let data = try JSONEncoder.api.encode(profile)
        let json = try XCTUnwrap(String(data: data, encoding: .utf8))
        XCTAssertTrue(json.contains("\"active_source\":\"radio\""))
        XCTAssertTrue(json.contains("\"left_db\":"))
        XCTAssertTrue(json.contains("\"right_db\":"))
        XCTAssertTrue(json.contains("\"stereo_level\":0"))
        XCTAssertTrue(json.contains("\"bass_level\":0"))
        XCTAssertFalse(json.contains("voice_clean"))
        XCTAssertFalse(json.contains("\"locked\""))
        let object = try XCTUnwrap(JSONSerialization.jsonObject(with: data) as? [String: Any])
        XCTAssertEqual(Set(object.keys), ["active_source", "master", "eq", "enhancements"])
        let enhancements = try XCTUnwrap(object["enhancements"] as? [String: Any])
        XCTAssertEqual(Set(enhancements.keys), ["stereo_level", "bass_level"])
        let eq = try XCTUnwrap(object["eq"] as? [[String: Any]])
        XCTAssertEqual(eq.count, 6)
        XCTAssertEqual(Set(eq[0].keys), ["gain_db", "center_hz", "q"])
    }

    func testAudioLevelsJSONDecoding() throws {
        let json = """
        {"radio_in_left_db":-1.5,"radio_in_right_db":-1.5,"bluetooth_in_left_db":-0.9,"bluetooth_in_right_db":-1.0,"output_left_db":-1.9,"output_right_db":-2.0}
        """.data(using: .utf8)!
        let levels = try JSONDecoder().decode(AudioLevelsDTO.self, from: json)
        XCTAssertEqual(levels.outputLeftDb, -1.9, accuracy: 0.01)
        XCTAssertEqual(levels.outputRightDb, -2.0, accuracy: 0.01)
        XCTAssertEqual(AudioLevelsDTO.meterFill(dbfs: -20), 0, accuracy: 0.01)
        XCTAssertEqual(AudioLevelsDTO.meterFill(dbfs: -6), 0.5, accuracy: 0.05)
        XCTAssertEqual(AudioLevelsDTO.meterFill(dbfs: 0), 0.72, accuracy: 0.02)
        XCTAssertLessThan(AudioLevelsDTO.meterFill(dbfs: -1.9), 0.75)
        XCTAssertGreaterThan(AudioLevelsDTO.meterFill(dbfs: -1.9), 0.55)
    }
}

final class MockDigiRadioServiceTests: XCTestCase {
    func testMockConnectAndTune() async throws {
        let mock = MockDigiRadioService()
        try await mock.connect(host: "mock.local")
        XCTAssertTrue(mock.state.connection.isConnected)
        try await mock.tuneFM(frequencyKhz: 101_500)
        XCTAssertEqual(mock.state.tuner.fm?.frequencyKhz, 101_500)
    }

    func testMockVerifyReachabilityRequiresConnection() async {
        let mock = MockDigiRadioService()
        mock.disconnect()
        let disconnected = await mock.verifyReachability()
        XCTAssertFalse(disconnected)
        XCTAssertTrue(mock.state.health.firmware.isEmpty)
        try? await mock.connect(host: "mock.local")
        let connected = await mock.verifyReachability()
        XCTAssertTrue(connected)
        XCTAssertFalse(mock.state.health.firmware.isEmpty)
    }

    func testMockWifiResetClearsConnection() async throws {
        let mock = MockDigiRadioService()
        try await mock.connect(host: "mock.local")
        let reboot = try await mock.resetWifi()
        XCTAssertEqual(reboot, 3)
        XCTAssertFalse(mock.state.connection.isConnected)
        XCTAssertTrue(mock.state.health.firmware.isEmpty)
    }

    func testMockBluetoothEnableDisable() async throws {
        let mock = MockDigiRadioService()
        try await mock.connect(host: "mock.local")
        XCTAssertTrue(mock.state.bluetooth.booted)
        try await mock.setBluetoothEnabled(false)
        XCTAssertFalse(mock.state.bluetooth.booted)
        try await mock.setBluetoothEnabled(true)
        XCTAssertTrue(mock.state.bluetooth.booted)
    }

    func testSelectDABPlaysAServiceFromEnsemble() async throws {
        let mock = MockDigiRadioService()
        try await mock.connect(host: "mock.local")
        try await mock.tuneFM(frequencyKhz: 101_500)
        XCTAssertEqual(mock.state.tuner.band, .fm)
        let vm = HomeViewModel(service: mock)
        await vm.selectDAB()
        XCTAssertEqual(mock.state.tuner.band, .dab)
        XCTAssertEqual(mock.state.tuner.dab?.playingServiceId, 101)
        XCTAssertEqual(mock.state.tuner.dab?.playingComponentId, 1)
    }

    func testApplyBuiltInPresetUsesFirmwareBandCenters() async throws {
        let mock = MockDigiRadioService()
        try await mock.connect(host: "mock.local")
        let rock = AudioProfileLibrary.builtIn.first { $0.id == "builtin.rock" }!
        try await mock.applyAudioProfile(
            rock.profileDTO(
                activeSource: mock.state.audio.activeSource,
                master: mock.state.audio.master,
                deviceEQ: mock.state.audio.eq
            )
        )
        XCTAssertEqual(mock.state.audio.eq.map(\.centerHz), AudioProfileTemplate.bandCenters)
        XCTAssertEqual(mock.state.audio.eq[1].gainDb, 4, accuracy: 0.01)
        XCTAssertEqual(mock.state.audio.enhancements.stereoLevel, 30)
    }

    func testStreamingDeviceSafeURL() {
        XCTAssertEqual(
            StreamingState.deviceSafeURL("http://stream.example.com/radio.mp3"),
            "http://stream.example.com/radio.mp3"
        )
        XCTAssertEqual(StreamingState.deviceSafeURL(""), StreamingState.defaultURL)
        XCTAssertEqual(
            StreamingState.deviceSafeURL("https://secure.example.com"),
            "https://secure.example.com"
        )
    }

    func testEQPresetNameClearsWhenUserEdits() async throws {
        let mock = MockDigiRadioService()
        try await mock.connect(host: "mock.local")
        let vm = AdauStudioViewModel(service: mock)
        vm.syncFromDevice()
        let rock = try XCTUnwrap(AudioProfileLibrary.builtIn.first { $0.id == "builtin.rock" })
        await vm.applyEQPreset(rock)
        XCTAssertEqual(vm.selectedEQPresetID, "builtin.rock")
        XCTAssertEqual(vm.selectedEQPresetName, "Rock")
        XCTAssertEqual(vm.bassLevel, 25, accuracy: 0.01)
        XCTAssertEqual(vm.stereoLevel, 30, accuracy: 0.01)
        XCTAssertEqual(vm.eqBands[1].gainDb, 4, accuracy: 0.01)
        vm.notePresetEdited()
        XCTAssertNil(vm.selectedEQPresetID)
        XCTAssertNil(vm.selectedEQPresetName)
        XCTAssertEqual(vm.eqBands[1].gainDb, 4, accuracy: 0.01)
    }

    func testFMBandScanCoversEuropeanBand() {
        XCTAssertEqual(FMBandScan.frequencies.first, 87_500)
        XCTAssertEqual(FMBandScan.frequencies.last, 107_900)
        XCTAssertEqual(FMBandScan.frequencies.count, 205)
    }

    func testFMBandScanUsableHitThresholds() {
        let weak = FMTunerState(frequencyKhz: 101_500, rssiDbuv: 4, snrDb: 12)
        let lockedLowSnr = FMTunerState(frequencyKhz: 101_500, rssiDbuv: 8, snrDb: 2)
        let strong = FMTunerState(frequencyKhz: 101_500, rssiDbuv: 30, snrDb: 15)
        XCTAssertFalse(FMBandScan.isUsableHit(fm: weak, locked: true, minRssiDbuv: 5, minSnrDb: 0))
        XCTAssertTrue(FMBandScan.isUsableHit(fm: lockedLowSnr, locked: true, minRssiDbuv: 5, minSnrDb: 0))
        XCTAssertFalse(FMBandScan.isUsableHit(fm: lockedLowSnr, locked: true, minRssiDbuv: 5, minSnrDb: 10))
        XCTAssertTrue(FMBandScan.isUsableHit(fm: strong, locked: true, minRssiDbuv: 5, minSnrDb: 0))
        XCTAssertFalse(FMBandScan.isUsableHit(fm: strong, locked: false, minRssiDbuv: 5, minSnrDb: 0))
    }

    func testFMScanPreferencesClamp() {
        FMScanPreferences.save(minRssiDbuv: -4, minSnrDb: 99)
        XCTAssertEqual(FMScanPreferences.minRssiDbuv, 0)
        XCTAssertEqual(FMScanPreferences.minSnrDb, 20)
        FMScanPreferences.resetToDefault()
        XCTAssertEqual(FMScanPreferences.minRssiDbuv, 5)
        XCTAssertEqual(FMScanPreferences.minSnrDb, 0)
    }

    func testScannedStationPersistenceRoundTrip() throws {
        let station = ScannedStation(
            id: "fm-101500",
            name: "Radio Test",
            band: .fm,
            fmFrequencyKhz: 101_500,
            dabFreqIndex: nil,
            dabServiceId: nil,
            dabComponentId: nil,
            rssiDbuv: 40,
            hasBroadcastName: true
        )
        let data = try JSONEncoder().encode(ScannedStationLibraryProbe(fm: [station], dab: []))
        let decoded = try JSONDecoder().decode(ScannedStationLibraryProbe.self, from: data)
        XCTAssertEqual(decoded.fm.first?.name, "Radio Test")
        XCTAssertEqual(decoded.fm.first?.fmFrequencyKhz, 101_500)
        XCTAssertEqual(decoded.fm.first?.allFMFrequenciesKhz, [101_500])
    }

    func testScannedStationDecodesLegacyCatalogWithoutFrequenciesArray() throws {
        let json = """
        {"fm":[{"id":"fm-101500","name":"Radio Test","band":"fm","fmFrequencyKhz":101500,"hasBroadcastName":true}],"dab":[]}
        """
        let decoded = try JSONDecoder().decode(ScannedStationLibraryProbe.self, from: Data(json.utf8))
        XCTAssertEqual(decoded.fm.first?.allFMFrequenciesKhz, [101_500])
    }

    func testVoiceClarifierEQOverlayReversible() {
        let base = AudioProfileTemplate.eqBands(gains: [0, 5, 2, 0, -1, -2])
        let voiced = AudioEnhancementsDesign.applyingVoice(base)
        XCTAssertEqual(voiced[2].gainDb, 0, accuracy: 0.01)
        XCTAssertEqual(voiced[3].gainDb, 1.5, accuracy: 0.01)
        XCTAssertEqual(voiced[4].gainDb, 2.5, accuracy: 0.01)
        let restored = AudioEnhancementsDesign.removingVoice(voiced)
        for (a, b) in zip(base, restored) {
            XCTAssertEqual(a.gainDb, b.gainDb, accuracy: 0.01)
        }
    }

    func testIncomingShareClassifiesYouTubeMusicLinkAsOtherApp() throws {
        let shared = try XCTUnwrap(URL(string: "https://music.youtube.com/watch?v=abc123"))
        XCTAssertEqual(IncomingShare.classify(shared), .otherApp(shared))

        let open = try XCTUnwrap(URL(string: "igiradio://share?u=https%3A%2F%2Fopen.spotify.com%2Ftrack%2Fxyz"))
        XCTAssertEqual(
            IncomingShare.parse(openURL: open),
            .otherApp(try XCTUnwrap(URL(string: "https://open.spotify.com/track/xyz")))
        )
    }

    func testIncomingShareClassifiesDirectAudioURLAsWebStream() throws {
        let stream = try XCTUnwrap(URL(string: "http://edge.radiomontecarlo.net/RMC.mp3"))
        XCTAssertEqual(IncomingShare.classify(stream), .webStream(stream))

        let open = try XCTUnwrap(URL(string: "igiradio://share?u=http%3A%2F%2Ficecast.unitedradio.it%2FRadio105.mp3"))
        XCTAssertEqual(
            IncomingShare.parse(openURL: open),
            .webStream(try XCTUnwrap(URL(string: "http://icecast.unitedradio.it/Radio105.mp3")))
        )
    }

    func testIncomingShareExtractsURLFromSharedText() {
        let extracted = IncomingShare.urlFromSharedText("Ascolta qui https://music.youtube.com/watch?v=zz")
        XCTAssertEqual(extracted?.host, "music.youtube.com")
    }

    func testIncomingShareOpensBareShareURLAsOtherApp() throws {
        let open = try XCTUnwrap(URL(string: "igiradio://share"))
        XCTAssertEqual(
            IncomingShare.parse(openURL: open),
            .otherApp(try XCTUnwrap(URL(string: "https://music.youtube.com")))
        )
    }

    func testIncomingShareIgnoresPDFAndGenericLinks() throws {
        let pdf = try XCTUnwrap(URL(string: "https://example.com/manuale.pdf"))
        XCTAssertNil(IncomingShare.classify(pdf))

        let article = try XCTUnwrap(URL(string: "https://www.repubblica.it/cronaca/foo"))
        XCTAssertNil(IncomingShare.classify(article))

        let open = try XCTUnwrap(URL(string: "igiradio://share?u=https%3A%2F%2Fexample.com%2Fdoc.pdf"))
        XCTAssertNil(IncomingShare.parse(openURL: open))
    }

    func testPrepareForPhoneAudioSwitchesToStream() async throws {
        let mock = MockDigiRadioService()
        XCTAssertEqual(mock.state.audio.activeSource, .radio)
        try await mock.setStreaming(enabled: true, url: StreamPresets.catalog[0].url)
        try await mock.prepareForPhoneAudio()
        XCTAssertEqual(mock.state.audio.activeSource, .bluetooth)
        XCTAssertTrue(mock.state.phoneAudioActive)
        XCTAssertFalse(mock.state.streaming.enabled)
        XCTAssertEqual(mock.state.nowPlayingTitle, "Altre app")
    }
}

private struct ScannedStationLibraryProbe: Codable {
    var fm: [ScannedStation]
    var dab: [ScannedStation]
}

