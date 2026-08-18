import Foundation

/// Central device snapshot — only documented API fields.
struct DigiRadioState: Equatable, Sendable {
    var connection = ConnectionState()
    var health = HealthState()
    var tuner = TunerState()
    var audio = AudioState()
    var bluetooth = BluetoothState()
    var stations: [Station] = []
    var streaming = StreamingState()
}

struct ConnectionState: Equatable, Sendable {
    var host: String = ""
    var isConnected = false
    var isConnecting = false
    var lastError: String?
}

struct HealthState: Equatable, Sendable {
    var status: String = ""
    var firmware: String = ""
    var serialNumber: String = ""
    var chips = ChipHealth()
}

struct ChipHealth: Equatable, Sendable {
    var si4684 = false
    var adau1701 = false
    var bt1035 = false
}

struct TunerState: Equatable, Sendable {
    var booted = false
    var band: TunerBand = .fm
    var locked = false
    var volume: Int = 0
    var fm: FMTunerState?
    var dab: DABTunerState?
}

enum TunerBand: String, Codable, Sendable, CaseIterable {
    case fm
    case dab
}

struct FMTunerState: Equatable, Sendable, Codable {
    var frequencyKhz: Int
    var rssiDbuv: Int?
    var snrDb: Int?
    var stereo: Bool?
    var stationName: String?
    var radiotext: String?

    enum CodingKeys: String, CodingKey {
        case frequencyKhz = "frequency_khz"
        case rssiDbuv = "rssi_dbuv"
        case snrDb = "snr_db"
        case stereo
        case stationName = "station_name"
        case radiotext
    }
}

struct DABTunerState: Equatable, Sendable, Codable {
    var freqIndex: Int
    var ficQuality: Int?
    var cnrDb: Int?
    var playingServiceId: Int?
    var playingComponentId: Int?
    var dynamicLabel: String?

    enum CodingKeys: String, CodingKey {
        case freqIndex = "freq_index"
        case ficQuality = "fic_quality"
        case cnrDb = "cnr_db"
        case playingServiceId = "playing_service_id"
        case playingComponentId = "playing_component_id"
        case dynamicLabel = "dynamic_label"
    }
}

struct AudioState: Equatable, Sendable {
    var mixer = MixerState()
    var master = MasterVolumeState()
    var eq: [EQBandState] = []
    var enhancements = EnhancementsState()
}

struct MixerState: Equatable, Sendable, Codable {
    var si4684LeftDb: Double = 0
    var si4684RightDb: Double = 0
    var esp32LeftDb: Double = 0
    var esp32RightDb: Double = 0
    var mixLeftDb: Double = 0
    var mixRightDb: Double = 0

    enum CodingKeys: String, CodingKey {
        case si4684LeftDb = "si4684_left_db"
        case si4684RightDb = "si4684_right_db"
        case esp32LeftDb = "esp32_left_db"
        case esp32RightDb = "esp32_right_db"
        case mixLeftDb = "mix_left_db"
        case mixRightDb = "mix_right_db"
    }
}

struct MasterVolumeState: Equatable, Sendable, Codable {
    var leftDb: Double = 0
    var rightDb: Double = 0

    enum CodingKeys: String, CodingKey {
        case leftDb = "left_db"
        case rightDb = "right_db"
    }
}

struct EQBandState: Equatable, Sendable, Codable, Identifiable {
    var id: Int { index }
    var index: Int
    var gainDb: Double
    var centerHz: Double
    var q: Double

    enum CodingKeys: String, CodingKey {
        case gainDb = "gain_db"
        case centerHz = "center_hz"
        case q
    }

    init(index: Int, gainDb: Double, centerHz: Double, q: Double) {
        self.index = index
        self.gainDb = gainDb
        self.centerHz = centerHz
        self.q = q
    }

    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        index = 0
        gainDb = try container.decode(Double.self, forKey: .gainDb)
        centerHz = try container.decode(Double.self, forKey: .centerHz)
        q = try container.decode(Double.self, forKey: .q)
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(gainDb, forKey: .gainDb)
        try container.encode(centerHz, forKey: .centerHz)
        try container.encode(q, forKey: .q)
    }
}

struct EnhancementsState: Equatable, Sendable, Codable {
    var stereoLevel: Int = 0
    var bassLevel: Int = 0

    enum CodingKeys: String, CodingKey {
        case stereoLevel = "stereo_level"
        case bassLevel = "bass_level"
    }
}

struct BluetoothState: Equatable, Sendable {
    var booted = false
    var pairing = false
    var a2dpState: A2DPState = .unknown
    var deviceName: String = ""
    var autoReconnect: Int = 0
    var savedSpeaker: SavedSpeaker?
    var nearbyDevices: [BluetoothDevice] = []
    var pairedDevices: [BluetoothPairedDevice] = []
}

enum A2DPState: String, Sendable {
    case standby, connecting, connected, streaming, paused, unknown

    init(apiValue: String) {
        switch apiValue.lowercased() {
        case "standby": self = .standby
        case "connecting": self = .connecting
        case "connected": self = .connected
        case "streaming": self = .streaming
        case "paused": self = .paused
        default: self = .unknown
        }
    }
}

struct SavedSpeaker: Equatable, Sendable {
    var mac: String
    var name: String
}

struct BluetoothDevice: Equatable, Sendable, Identifiable {
    var id: String { mac }
    var index: Int
    var mac: String
    var name: String
    var rssiDbm: Int
}

struct BluetoothPairedDevice: Equatable, Sendable, Identifiable {
    var id: String { mac }
    var index: Int
    var mac: String
    var name: String
}

struct Station: Equatable, Sendable, Codable, Identifiable {
    var id: String { "\(band.rawValue)-\(name)-\(fmFrequencyKhz ?? dabFreqIndex ?? 0)" }
    var name: String
    var band: TunerBand
    var dabFreqIndex: Int?
    var dabServiceId: Int?
    var dabComponentId: Int?
    var fmFrequencyKhz: Int?

    enum CodingKeys: String, CodingKey {
        case name, band
        case dabFreqIndex = "dab_freq_index"
        case dabServiceId = "dab_service_id"
        case dabComponentId = "dab_component_id"
        case fmFrequencyKhz = "fm_frequency_khz"
    }
}

struct StreamingState: Equatable, Sendable, Codable {
    var enabled = false
    var url: String = ""
}

struct DABService: Equatable, Sendable, Identifiable, Codable {
    var id: String { "\(serviceId)-\(componentId)" }
    var serviceId: Int
    var componentId: Int
    var label: String

    enum CodingKeys: String, CodingKey {
        case serviceId = "service_id"
        case componentId = "component_id"
        case label
    }
}
