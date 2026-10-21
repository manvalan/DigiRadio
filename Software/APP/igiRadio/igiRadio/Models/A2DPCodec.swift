import Foundation

/// Negotiated A2DP codec reported by `GET /api/bluetooth/a2dp-codec`.
enum A2DPCodec: Equatable, Sendable {
    case sbc
    case aptx
    case aptxHD
    case aptxLL
    case aptxAdaptive
    case aac
    case ldac
    case unknown(String)

    init(apiValue: String) {
        switch apiValue.lowercased() {
        case "sbc": self = .sbc
        case "aptx": self = .aptx
        case "aptx-hd": self = .aptxHD
        case "aptx-ll": self = .aptxLL
        case "aptx-adaptive": self = .aptxAdaptive
        case "aac": self = .aac
        case "ldac": self = .ldac
        default: self = .unknown(apiValue)
        }
    }

    var displayName: String {
        switch self {
        case .sbc: "SBC"
        case .aptx: "aptX"
        case .aptxHD: "aptX HD"
        case .aptxLL: "aptX LL"
        case .aptxAdaptive: "aptX Adaptive"
        case .aac: "AAC"
        case .ldac: "LDAC"
        case let .unknown(raw): raw.isEmpty ? "Sconosciuto" : raw
        }
    }
}

/// Bitmask for `POST /api/bluetooth/a2dp-codec` (`codec_mask` 0–63). SBC is always available.
enum A2DPCodecMask: Int, CaseIterable, Identifiable, Sendable {
    case aac = 1
    case aptx = 2
    case aptxLL = 4
    case aptxHD = 8
    case aptxAdaptive = 16
    case ldac = 32

    var id: Int { rawValue }

    var label: String {
        switch self {
        case .aac: "AAC"
        case .aptx: "aptX"
        case .aptxLL: "aptX LL"
        case .aptxHD: "aptX HD"
        case .aptxAdaptive: "aptX Adaptive"
        case .ldac: "LDAC"
        }
    }

    static func mask(from enabled: Set<A2DPCodecMask>) -> Int {
        enabled.reduce(0) { $0 | $1.rawValue }
    }

    static func set(from mask: Int) -> Set<A2DPCodecMask> {
        Set(allCases.filter { mask & $0.rawValue != 0 })
    }

    /// Default permissive mask (all optional codecs enabled).
    static let defaultEnabled: Set<A2DPCodecMask> = Set(allCases)
}
