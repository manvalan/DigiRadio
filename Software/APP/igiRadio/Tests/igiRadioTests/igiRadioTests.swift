import XCTest
@testable import igiRadio

final class HTTPDecoderTests: XCTestCase {
    func testDecodeHealth() throws {
        let json = """
        {"status":"ok","fw":"0.8.5","serialNumber":"CC4DB4MOCK01",
         "chips":{"si4684":true,"adau1701":true,"bt1035":true}}
        """.data(using: .utf8)!
        let health = try JSONDecoder.api.decode(HealthResponse.self, from: json)
        XCTAssertEqual(health.status, "ok")
        XCTAssertEqual(health.fw, "0.8.5")
        XCTAssertEqual(health.serialNumber, "CC4DB4MOCK01")
        XCTAssertTrue(health.chips.si4684)
    }

    func testDecodeTunerFM() throws {
        let json = """
        {"booted":true,"band":"fm","locked":true,"volume":50,
         "fm":{"frequency_khz":102300,"rssi_dbuv":48,"snr_db":20,"stereo":true,
               "station_name":"Radio Test","radiotext":"Hello"},
         "dab":null}
        """.data(using: .utf8)!
        let tuner = try JSONDecoder.api.decode(TunerStatusResponse.self, from: json)
        XCTAssertEqual(tuner.band, "fm")
        XCTAssertEqual(tuner.fm?.frequencyKhz, 102300)
        XCTAssertEqual(tuner.fm?.stationName, "Radio Test")
    }

    func testDecodeAudioProfile() throws {
        let json = """
        {"mixer":{"si4684_left_db":0,"si4684_right_db":0,"esp32_left_db":0,"esp32_right_db":0,
                 "mix_left_db":0,"mix_right_db":0},
         "master":{"left_db":10,"right_db":10},
         "eq":[{"gain_db":0,"center_hz":40,"q":1.414}],
         "enhancements":{"stereo_level":5,"bass_level":7}}
        """.data(using: .utf8)!
        let profile = try JSONDecoder.api.decode(AudioProfileDTO.self, from: json)
        XCTAssertEqual(profile.master.leftDb, 10)
        XCTAssertEqual(profile.enhancements.bassLevel, 7)
    }

    func testA2DPStateMapping() {
        XCTAssertEqual(A2DPState(apiValue: "streaming"), .streaming)
        XCTAssertEqual(A2DPState(apiValue: "unknown-value"), .unknown)
    }

    func testBLENameToHost() {
        XCTAssertEqual(
            DigiRadioDiscoveryService.httpHost(fromBLEName: "DigiRadio-CC4DB4"),
            "http://digiradio-cc4db4.local"
        )
        XCTAssertNil(DigiRadioDiscoveryService.httpHost(fromBLEName: "OtherDevice"))
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
}
