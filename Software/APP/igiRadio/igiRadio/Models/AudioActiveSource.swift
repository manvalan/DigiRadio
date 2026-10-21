import Foundation

/// Sorgente sul mux ADAU1701.
/// Il raw value `bluetooth` è il nome firmware dell'ingresso I2S/stream:
/// su questa scheda il BT1035 è solo uscita A2DP, non una sorgente.
enum AudioActiveSource: String, Codable, Sendable, CaseIterable, Identifiable {
    case radio
    case bluetooth
    case beep

    var id: String { rawValue }

    /// Sorgenti esposte nell'UI utente (no tono di test).
    static let userFacing: [AudioActiveSource] = [.radio, .bluetooth]

    var title: String {
        switch self {
        case .radio: "Radio"
        case .bluetooth: "Stream"
        case .beep: "Tono test"
        }
    }

    var subtitle: String {
        switch self {
        case .radio: "FM / DAB dal tuner"
        case .bluetooth: "Stream web o file dal telefono"
        case .beep: "Generatore interno (diagnostica)"
        }
    }

    var systemImage: String {
        switch self {
        case .radio: "antenna.radiowaves.left.and.right"
        case .bluetooth: "dot.radiowaves.forward"
        case .beep: "waveform.badge.magnifyingglass"
        }
    }
}
