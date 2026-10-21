import Foundation

/// Stazione trovata da una scansione — memorizzata sull'iPhone, non sul DigiRadio.
struct ScannedStation: Identifiable, Equatable, Sendable, Codable {
    var id: String
    var name: String
    var band: TunerBand
    var fmFrequencyKhz: Int?
    var dabFreqIndex: Int?
    var dabServiceId: Int?
    var dabComponentId: Int?
    var rssiDbuv: Int?
    /// Altre frequenze FM della stessa emittente (RDS con lo stesso nome).
    var fmFrequenciesKhz: [Int] = []
    /// `true` se il nome proviene dal broadcast (RDS FM o etichetta DAB).
    var hasBroadcastName: Bool

    static func fromFM(_ hit: FMScanHit) -> ScannedStation {
        let title = hit.stationName?.trimmingCharacters(in: .whitespacesAndNewlines)
        let hasName = !(title?.isEmpty ?? true)
        let name = hasName ? title! : fmPlaceholderName(khz: hit.frequencyKhz)
        return ScannedStation(
            id: "fm-\(hit.frequencyKhz)",
            name: name,
            band: .fm,
            fmFrequencyKhz: hit.frequencyKhz,
            dabFreqIndex: nil,
            dabServiceId: nil,
            dabComponentId: nil,
            rssiDbuv: hit.rssiDbuv,
            fmFrequenciesKhz: [hit.frequencyKhz],
            hasBroadcastName: hasName
        )
    }

    static func fromDAB(ensemble: Int, service: DABService) -> ScannedStation {
        ScannedStation(
            id: "dab-\(ensemble)-\(service.serviceId)-\(service.componentId)",
            name: service.label,
            band: .dab,
            fmFrequencyKhz: nil,
            dabFreqIndex: ensemble,
            dabServiceId: service.serviceId,
            dabComponentId: service.componentId,
            rssiDbuv: nil,
            hasBroadcastName: true
        )
    }

    func asPreset(named customName: String? = nil) -> Station {
        let resolvedName: String
        if let customName, !customName.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
            resolvedName = customName.trimmingCharacters(in: .whitespacesAndNewlines)
        } else {
            resolvedName = name
        }
        switch band {
        case .fm:
            return Station(name: resolvedName, band: .fm, fmFrequencyKhz: fmFrequencyKhz)
        case .dab:
            return Station(
                name: resolvedName,
                band: .dab,
                dabFreqIndex: dabFreqIndex,
                dabServiceId: dabServiceId,
                dabComponentId: dabComponentId
            )
        }
    }

    private static func fmPlaceholderName(khz: Int) -> String {
        String(format: "FM %.2f", Double(khz) / 1000)
    }

    /// True while the name is still the automatic "FM 98.40" placeholder
    /// (on any of the station's frequencies), not one the user typed.
    var hasPlaceholderName: Bool {
        name.isEmpty || allFMFrequenciesKhz.contains { Self.fmPlaceholderName(khz: $0) == name }
    }

    func matches(_ saved: Station) -> Bool {
        switch band {
        case .fm:
            guard saved.band == .fm, let khz = saved.fmFrequencyKhz else { return false }
            return containsFMFrequency(khz)
        case .dab:
            return saved.band == .dab
                && saved.dabServiceId == dabServiceId
                && saved.dabComponentId == dabComponentId
        }
    }

    var allFMFrequenciesKhz: [Int] {
        var values = Set(fmFrequenciesKhz)
        if let khz = fmFrequencyKhz { values.insert(khz) }
        return values.sorted()
    }

    func containsFMFrequency(_ khz: Int) -> Bool {
        allFMFrequenciesKhz.contains(khz)
    }

    /// Nome da mostrare: etichetta RDS/utente se non è solo la frequenza.
    var displayTitle: String {
        StationNameDisplay.meaningful(name, khz: fmFrequencyKhz) ?? subtitle
    }

    var subtitle: String {
        switch band {
        case .fm:
            let freqs = allFMFrequenciesKhz
            guard !freqs.isEmpty else { return "FM" }
            let labels = freqs.map { String(format: "%.2f", Double($0) / 1000) }
            if labels.count == 1 {
                return String(localized: "FM \(labels[0]) MHz")
            }
            return "FM " + labels.joined(separator: " · ") + " MHz"
        case .dab:
            if let idx = dabFreqIndex {
                return String(localized: "DAB · ensemble \(idx)")
            }
            return "DAB"
        }
    }
}

/// Pulisce PS/Radiotext RDS (il firmware può includere byte NUL).
enum RDSText {
    static func sanitized(_ raw: String?) -> String? {
        guard let raw else { return nil }
        let withoutNulls = raw.replacingOccurrences(of: "\0", with: " ")
        let collapsed = withoutNulls.replacingOccurrences(
            of: #"\s+"#,
            with: " ",
            options: .regularExpression
        )
        .trimmingCharacters(in: .whitespacesAndNewlines)
        return collapsed.isEmpty ? nil : collapsed
    }
}

/// Distingue un vero nome stazione da un’etichetta che è solo la frequenza.
enum StationNameDisplay {
    static func meaningful(_ name: String?, khz: Int? = nil) -> String? {
        guard let trimmed = RDSText.sanitized(name) else { return nil }
        if isFrequencyLabel(trimmed, khz: khz) { return nil }
        return trimmed
    }

    static func isFrequencyLabel(_ name: String, khz: Int?) -> Bool {
        let collapsed = name.uppercased()
            .replacingOccurrences(of: " ", with: "")
            .replacingOccurrences(of: "MHZ", with: "")
            .replacingOccurrences(of: ",", with: ".")
        let withoutFM = collapsed.hasPrefix("FM") ? String(collapsed.dropFirst(2)) : collapsed
        guard !withoutFM.isEmpty else { return true }
        guard let khz else {
            return withoutFM.range(of: #"^\d+(\.\d+)?$"#, options: .regularExpression) != nil
        }
        let mhz2 = String(format: "%.2f", Double(khz) / 1000)
        let mhz1 = String(format: "%.1f", Double(khz) / 1000)
        return withoutFM == mhz2 || withoutFM == mhz1
            || withoutFM == String(format: "%.0f", Double(khz) / 1000)
    }
}

extension ScannedStation {
    enum CodingKeys: String, CodingKey {
        case id, name, band, fmFrequencyKhz, dabFreqIndex, dabServiceId
        case dabComponentId, rssiDbuv, hasBroadcastName, fmFrequenciesKhz
    }

    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        id = try container.decode(String.self, forKey: .id)
        name = try container.decode(String.self, forKey: .name)
        band = try container.decode(TunerBand.self, forKey: .band)
        fmFrequencyKhz = try container.decodeIfPresent(Int.self, forKey: .fmFrequencyKhz)
        dabFreqIndex = try container.decodeIfPresent(Int.self, forKey: .dabFreqIndex)
        dabServiceId = try container.decodeIfPresent(Int.self, forKey: .dabServiceId)
        dabComponentId = try container.decodeIfPresent(Int.self, forKey: .dabComponentId)
        rssiDbuv = try container.decodeIfPresent(Int.self, forKey: .rssiDbuv)
        hasBroadcastName = try container.decodeIfPresent(Bool.self, forKey: .hasBroadcastName) ?? false
        var freqs = try container.decodeIfPresent([Int].self, forKey: .fmFrequenciesKhz) ?? []
        if let khz = fmFrequencyKhz { freqs.append(khz) }
        fmFrequenciesKhz = Array(Set(freqs)).sorted()
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(id, forKey: .id)
        try container.encode(name, forKey: .name)
        try container.encode(band, forKey: .band)
        try container.encodeIfPresent(fmFrequencyKhz, forKey: .fmFrequencyKhz)
        try container.encodeIfPresent(dabFreqIndex, forKey: .dabFreqIndex)
        try container.encodeIfPresent(dabServiceId, forKey: .dabServiceId)
        try container.encodeIfPresent(dabComponentId, forKey: .dabComponentId)
        try container.encodeIfPresent(rssiDbuv, forKey: .rssiDbuv)
        try container.encode(hasBroadcastName, forKey: .hasBroadcastName)
        try container.encode(allFMFrequenciesKhz, forKey: .fmFrequenciesKhz)
    }
}

/// Unisce le FM con lo stesso nome RDS in una sola riga con più frequenze.
enum FMStationMerge {
    /// PS RDS incompleto (1–2 caratteri) non è un nome: non accorpare su quello.
    static let minNameLength = 3

    static func merge(_ stations: [ScannedStation]) -> [ScannedStation] {
        var unnamed: [ScannedStation] = []
        var groups: [String: [ScannedStation]] = [:]
        for station in stations {
            if station.band == .dab {
                unnamed.append(station)
                continue
            }
            if let key = groupingKey(station) {
                groups[key, default: []].append(station)
            } else {
                unnamed.append(station)
            }
        }
        var merged = unnamed
        for (key, members) in groups {
            merged.append(combine(key: key, members: members))
        }
        return merged.sorted { lhs, rhs in
            if lhs.band != rhs.band { return lhs.band == .fm }
            if lhs.hasBroadcastName != rhs.hasBroadcastName { return lhs.hasBroadcastName }
            let nameOrder = lhs.name.localizedCaseInsensitiveCompare(rhs.name)
            if nameOrder != .orderedSame { return nameOrder == .orderedAscending }
            return (lhs.fmFrequencyKhz ?? 0) < (rhs.fmFrequencyKhz ?? 0)
        }
    }

    static func groupingKey(_ station: ScannedStation) -> String? {
        guard station.band == .fm, station.hasBroadcastName else { return nil }
        guard let name = StationNameDisplay.meaningful(station.name, khz: station.fmFrequencyKhz) else {
            return nil
        }
        let collapsed = name.uppercased().replacingOccurrences(
            of: #"\s+"#,
            with: " ",
            options: .regularExpression
        )
        guard collapsed.count >= minNameLength else { return nil }
        return collapsed
    }

    private static func combine(key: String, members: [ScannedStation]) -> ScannedStation {
        let preferred = members.max { ($0.rssiDbuv ?? -128) < ($1.rssiDbuv ?? -128) } ?? members[0]
        var freqs = Set<Int>()
        for member in members {
            freqs.formUnion(member.allFMFrequenciesKhz)
        }
        let sorted = freqs.sorted()
        let slug = key.replacingOccurrences(of: #"\s+"#, with: "-", options: .regularExpression)
        return ScannedStation(
            id: "fm-name-\(slug)",
            name: preferred.name,
            band: .fm,
            fmFrequencyKhz: preferred.fmFrequencyKhz ?? sorted.first,
            dabFreqIndex: nil,
            dabServiceId: nil,
            dabComponentId: nil,
            rssiDbuv: preferred.rssiDbuv,
            fmFrequenciesKhz: sorted,
            hasBroadcastName: true
        )
    }
}
