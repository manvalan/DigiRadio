import SwiftUI

enum AppDestination: String, Hashable, Identifiable, CaseIterable {
    case listening
    case stations
    case dab
    case sound
    case settings
    case configuration
    case fm
    case stream
    case profiles
    case bluetooth
    case connection
    case deviceInfo
    case diagnostics
    case firmware
    case fmScan

    var id: String { rawValue }

    var title: String {
        switch self {
        case .listening: "Ascolto"
        case .stations: "Radio"
        case .dab: "DAB"
        case .sound: "Suono"
        case .settings: "Impostazioni"
        case .configuration: "La mia radio"
        case .fm: "FM"
        case .stream: "Stream"
        case .profiles: "Profili"
        case .bluetooth: "Altoparlante"
        case .connection: "Connessione"
        case .deviceInfo: "Informazioni"
        case .diagnostics: "Diagnostica"
        case .firmware: "Firmware"
        case .fmScan: "Scansione FM"
        }
    }

    var systemImage: String {
        switch self {
        case .listening: "play.square.stack.fill"
        case .stations: "radio.fill"
        case .dab: "antenna.radiowaves.left.and.right"
        case .sound: "slider.horizontal.3"
        case .settings: "gearshape.fill"
        case .configuration: "switch.2"
        case .fm: "dot.radiowaves.left.and.right"
        case .stream: "dot.radiowaves.forward"
        case .profiles: "waveform.path.ecg"
        case .bluetooth: "hifispeaker.fill"
        case .connection: "link"
        case .deviceInfo: "info.circle.fill"
        case .diagnostics: "stethoscope"
        case .firmware: "cpu.fill"
        case .fmScan: "slider.horizontal.2.magnifyingglass"
        }
    }

    var usesReadableWidth: Bool {
        switch self {
        case .listening, .stations, .sound, .settings, .configuration: false
        default: true
        }
    }

    static let sidebarRoots: [AppDestination] = [
        .listening, .stations, .sound, .settings, .configuration,
    ]
}

struct AppDestinationView: View {
    @Environment(AppEnvironment.self) private var environment
    var destination: AppDestination

    var body: some View {
        if destination.usesReadableWidth {
            content.igiReadableWidth()
        } else {
            content
        }
    }

    @ViewBuilder
    private var content: some View {
        switch destination {
        case .listening:
            HomeView()
        case .stations:
            PresetsView()
        case .fm:
            FMRadioView()
        case .dab:
            DABRadioView()
        case .stream:
            StreamingView()
        case .sound:
            AdauStudioView(vm: environment.adauStudio, initialSection: .sound)
        case .profiles:
            AudioProfilesView()
        case .bluetooth:
            PrepareRadioView(entry: .speaker)
        case .connection:
            ConnectionView()
        case .deviceInfo:
            DeviceInfoView()
        case .diagnostics:
            DiagnosticsView()
        case .firmware:
            FirmwareView()
        case .fmScan:
            FMScanSettingsView()
        case .settings:
            SettingsRootView()
        case .configuration:
            ConfigurationRootView()
        }
    }
}
