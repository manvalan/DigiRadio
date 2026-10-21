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
    /// PCM dal telefono (ReplayKit / file), distinto dalla webradio HTTP.
    var phoneAudioActive = false
    var power: DevicePowerState = .on
    /// GET /api/tuner/af — nil when the firmware does not follow AFs itself.
    var firmwareAF: FirmwareAFState?
}

/// RDS alternate-frequency follow done by the radio (firmware 0.9.2+).
struct FirmwareAFState: Equatable, Sendable, Codable {
    var enabled: Bool
    var switches: Int
    var lastFromKhz: Int?
    var lastToKhz: Int?

    enum CodingKeys: String, CodingKey {
        case enabled
        case switches
        case lastFromKhz = "last_from_khz"
        case lastToKhz = "last_to_khz"
    }
}

/// GET/POST /api/system/power — soft standby (master muted, Wi-Fi stays up).
enum DevicePowerState: String, Codable, Sendable {
    case on
    case standby
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
    /// Carrier offset in ppm (omitted by older firmware).
    var freqOffPpm: Int? = nil
    /// RDS alternate frequencies (group 0A), kHz; omitted when empty.
    var afKhz: [Int]? = nil

    enum CodingKeys: String, CodingKey {
        case frequencyKhz = "frequency_khz"
        case rssiDbuv = "rssi_dbuv"
        case snrDb = "snr_db"
        case stereo
        case stationName = "station_name"
        case radiotext
        case freqOffPpm = "freqoff_ppm"
        case afKhz = "af_khz"
    }
}

struct DABTunerState: Equatable, Sendable, Codable {
    var freqIndex: Int
    var ficQuality: Int?
    var cnrDb: Int?
    var playingServiceId: Int?
    var playingComponentId: Int?
    var dynamicLabel: String?

    init(
        freqIndex: Int,
        ficQuality: Int? = nil,
        cnrDb: Int? = nil,
        playingServiceId: Int? = nil,
        playingComponentId: Int? = nil,
        dynamicLabel: String? = nil
    ) {
        self.freqIndex = freqIndex
        self.ficQuality = ficQuality
        self.cnrDb = cnrDb
        self.playingServiceId = playingServiceId
        self.playingComponentId = playingComponentId
        self.dynamicLabel = dynamicLabel
    }

    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        // Il firmware può omettere freq_index subito dopo una sintonizzazione DAB.
        freqIndex = try container.decodeIfPresent(Int.self, forKey: .freqIndex) ?? 0
        ficQuality = try container.decodeIfPresent(Int.self, forKey: .ficQuality)
        cnrDb = try container.decodeIfPresent(Int.self, forKey: .cnrDb)
        playingServiceId = try container.decodeIfPresent(Int.self, forKey: .playingServiceId)
        playingComponentId = try container.decodeIfPresent(Int.self, forKey: .playingComponentId)
        dynamicLabel = try container.decodeIfPresent(String.self, forKey: .dynamicLabel)
    }

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
    var activeSource: AudioActiveSource = .radio
    var master = MasterVolumeState()
    var eq: [EQBandState] = []
    var enhancements = EnhancementsState()
    var baseEQ: [EQBandState]?
}

struct MasterVolumeState: Equatable, Sendable, Codable {
    var leftDb: Double = 0
    var rightDb: Double = 0

    enum CodingKeys: String, CodingKey {
        case leftDb = "left_db"
        case rightDb = "right_db"
    }
}

struct EQBandState: Equatable, Hashable, Sendable, Codable, Identifiable {
    var id: Int { index }
    var index: Int
    var gainDb: Double
    var centerHz: Double
    var q: Double
    /// `true` se il firmware ignora questa banda (passa-alto fisso o enhancement attivo).
    var isLocked: Bool = false

    enum CodingKeys: String, CodingKey {
        case gainDb = "gain_db"
        case centerHz = "center_hz"
        case q
        case isLocked = "locked"
    }

    init(index: Int, gainDb: Double, centerHz: Double, q: Double, isLocked: Bool = false) {
        self.index = index
        self.gainDb = gainDb
        self.centerHz = centerHz
        self.q = q
        self.isLocked = isLocked
    }

    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        index = 0
        gainDb = try container.decode(Double.self, forKey: .gainDb)
        centerHz = try container.decode(Double.self, forKey: .centerHz)
        q = try container.decode(Double.self, forKey: .q)
        isLocked = try container.decodeIfPresent(Bool.self, forKey: .isLocked) ?? false
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(gainDb, forKey: .gainDb)
        try container.encode(centerHz, forKey: .centerHz)
        try container.encode(q, forKey: .q)
    }
}

struct EnhancementsState: Equatable, Hashable, Sendable, Codable {
    var stereoLevel: Int = 0
    var bassLevel: Int = 0
    var voiceClean: Bool = false

    enum CodingKeys: String, CodingKey {
        case stereoLevel = "stereo_level"
        case bassLevel = "bass_level"
        case voiceClean = "voice_clean"
    }

    init(stereoLevel: Int = 0, bassLevel: Int = 0, voiceClean: Bool = false) {
        self.stereoLevel = stereoLevel
        self.bassLevel = bassLevel
        self.voiceClean = voiceClean
    }

    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        stereoLevel = try container.decodeIfPresent(Int.self, forKey: .stereoLevel) ?? 0
        bassLevel = try container.decodeIfPresent(Int.self, forKey: .bassLevel) ?? 0
        voiceClean = try container.decodeIfPresent(Bool.self, forKey: .voiceClean) ?? false
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(stereoLevel, forKey: .stereoLevel)
        try container.encode(bassLevel, forKey: .bassLevel)
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
    var negotiatedCodec: A2DPCodec = .sbc
}

enum A2DPState: String, Sendable {
    case standby, connecting, connected, streaming, paused, unknown

    var title: String {
        switch self {
        case .standby: "In attesa"
        case .connecting: "Connessione…"
        case .connected: "Collegato"
        case .streaming: "In uscita"
        case .paused: "In pausa"
        case .unknown: "—"
        }
    }

    var isLinked: Bool {
        self == .connected || self == .streaming || self == .paused
    }

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

    /// Etichetta da mostrare in UI: nome stazione se c’è, altrimenti la frequenza.
    var displayLabel: String {
        if let named = StationNameDisplay.meaningful(name, khz: fmFrequencyKhz) {
            return named
        }
        switch band {
        case .fm:
            if let khz = fmFrequencyKhz {
                return String(format: "FM %.2f MHz", Double(khz) / 1000)
            }
            return "FM"
        case .dab:
            if let idx = dabFreqIndex {
                return String(localized: "DAB ensemble \(idx)")
            }
            return "DAB"
        }
    }

    /// Etichetta secondaria per le liste stazioni.
    var listSubtitle: String {
        switch band {
        case .fm:
            if let khz = fmFrequencyKhz {
                return String(format: "FM %.2f MHz", Double(khz) / 1000)
            }
            return "FM"
        case .dab:
            if let idx = dabFreqIndex {
                return String(localized: "DAB · ensemble \(idx)")
            }
            return "DAB"
        }
    }

    enum CodingKeys: String, CodingKey {
        case name, band
        case dabFreqIndex = "dab_freq_index"
        case dabServiceId = "dab_service_id"
        case dabComponentId = "dab_component_id"
        case fmFrequencyKhz = "fm_frequency_khz"
    }
}

extension ScannedStation {
    static func from(_ station: Station) -> ScannedStation {
        ScannedStation(
            id: station.id,
            name: station.displayLabel,
            band: station.band,
            fmFrequencyKhz: station.fmFrequencyKhz,
            dabFreqIndex: station.dabFreqIndex,
            dabServiceId: station.dabServiceId,
            dabComponentId: station.dabComponentId,
            rssiDbuv: nil,
            fmFrequenciesKhz: station.fmFrequencyKhz.map { [$0] } ?? [],
            hasBroadcastName: !station.name.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
        )
    }
}

struct StreamingState: Equatable, Sendable, Codable {
    /// Stesso default del firmware (`WebRadioConfig::factoryDefault`).
    static let defaultURL = "http://edge.radiomontecarlo.net/RMC.mp3"

    var enabled = false
    var url: String = ""

    /// Il POST `/api/streaming` accetta `http://` e `https://`.
    static func deviceSafeURL(_ url: String) -> String {
        let trimmed = url.trimmingCharacters(in: .whitespacesAndNewlines)
        if trimmed.hasPrefix("http://") || trimmed.hasPrefix("https://") { return trimmed }
        return defaultURL
    }
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

extension DigiRadioState {
    var nowPlayingTitle: String {
        if audio.activeSource == .bluetooth {
            return phoneAudioActive ? "Altre app" : "Stream"
        }
        switch tuner.band {
        case .fm:
            if let khz = tuner.fm?.frequencyKhz,
               let name = StationNameDisplay.meaningful(tuner.fm?.stationName, khz: khz) {
                return name
            }
            if let khz = tuner.fm?.frequencyKhz {
                return String(format: "%.2f MHz", Double(khz) / 1000.0)
            }
            return String(localized: "Radio FM")
        case .dab:
            if let label = tuner.dab?.dynamicLabel?.trimmingCharacters(in: .whitespacesAndNewlines), !label.isEmpty {
                return label
            }
            return String(localized: "Radio DAB")
        }
    }

    var nowPlayingSubtitle: String {
        switch audio.activeSource {
        case .bluetooth:
            if streaming.enabled, !streaming.url.isEmpty { return streaming.url }
            return "Stream"
        case .beep:
            return String(localized: "Tono di test")
        case .radio:
            switch tuner.band {
            case .fm:
                if let khz = tuner.fm?.frequencyKhz {
                    let freq = String(format: "%.2f MHz", Double(khz) / 1000.0)
                    if let rt = RDSText.sanitized(tuner.fm?.radiotext) { return String(localized: "\(freq) · \(rt)") }
                    return freq
                }
                return "FM"
            case .dab:
                if let idx = tuner.dab?.freqIndex {
                    return String(localized: "DAB · canale \(idx)")
                }
                return "DAB"
            }
        }
    }

    var nowPlayingSymbol: String {
        switch audio.activeSource {
        case .bluetooth: "dot.radiowaves.forward"
        case .beep: "waveform"
        case .radio: tuner.band == .dab ? "antenna.radiowaves.left.and.right" : "radio.fill"
        }
    }
}
