import Foundation

/// Metadati descrittivi per le celle Parameter RAM ADAU1701 (SigmaStudio).
enum AdauParamGroup: String, CaseIterable, Identifiable, Sendable {
    case diagnostic = "Diagnostica"
    case mixer = "Mixer"
    case eq = "Equalizzatore"
    case master = "Master"
    case limiter = "Limiter"

    var id: String { rawValue }

    var systemImage: String {
        switch self {
        case .diagnostic: "waveform.badge.magnifyingglass"
        case .mixer: "slider.vertical.3"
        case .eq: "waveform.path.ecg"
        case .master: "speaker.wave.3.fill"
        case .limiter: "chart.line.flattrend.xyaxis"
        }
    }
}

struct AdauParamInfo: Identifiable, Sendable {
    let name: String
    let address: Int
    let group: AdauParamGroup
    let summary: String

    var id: String { name }
}

enum AdauParamCatalog {
    static let entries: [AdauParamInfo] = [
        .init(name: "BEEP1_ENABLE", address: 0, group: .diagnostic, summary: "Abilita generatore toni"),
        .init(name: "BEEP1_KICK", address: 1, group: .diagnostic, summary: "Trigger tono diagnostico"),
        .init(name: "BEEP1_BEEP_FREQ", address: 2, group: .diagnostic, summary: "Frequenza tono (fissa)"),
        .init(name: "SI4674", address: 3, group: .mixer, summary: "Gain pre-mixer Si4684 L"),
        .init(name: "SI4674_1", address: 4, group: .mixer, summary: "Gain pre-mixer Si4684 R"),
        .init(name: "ESP32", address: 5, group: .mixer, summary: "Gain pre-mixer stream L"),
        .init(name: "ESP32_1", address: 6, group: .mixer, summary: "Gain pre-mixer stream R"),
        .init(name: "SINGLE1", address: 7, group: .diagnostic, summary: "Gain tono beep"),
        .init(name: "SSPLITTER1", address: 8, group: .diagnostic, summary: "Splitter beep mono→stereo"),
        .init(name: "STMIXER1_ST0_VOLUME", address: 9, group: .mixer, summary: "Leg mixer radio (Si4684)"),
        .init(name: "STMIXER1_ST1_VOLUME", address: 10, group: .mixer, summary: "Leg mixer stream (ESP32)"),
        .init(name: "STMIXER1_ST2_VOLUME", address: 11, group: .mixer, summary: "Leg mixer beep"),
        .init(name: "MULTIPLE1", address: 42, group: .master, summary: "Volume master L"),
        .init(name: "MULTIPLE1_1", address: 43, group: .master, summary: "Volume master R"),
    ]

    static func info(for name: String) -> AdauParamInfo? {
        entries.first { $0.name == name }
    }

    static func group(forAddress address: Int) -> AdauParamGroup {
        if (12 ... 41).contains(address) { return .eq }
        if (44 ... 73).contains(address) { return .limiter }
        return entries.first { $0.address == address }?.group ?? .mixer
    }

    static func summary(for name: String) -> String {
        if let info = info(for: name) { return info.summary }
        if name.contains("PARAMEQ1") { return String(localized: "Coefficiente EQ") }
        if name.contains("LIMITER") { return String(localized: "Parametro limiter") }
        return String(localized: "Parametro SigmaStudio")
    }
}
