import SwiftUI

struct RootView: View {
    @Environment(AppEnvironment.self) private var environment
    @Environment(\.horizontalSizeClass) private var horizontalSizeClass

    var body: some View {
        Group {
            if horizontalSizeClass == .regular {
                IPadRootView()
            } else {
                IPhoneRootView()
            }
        }
        .tint(IGITheme.accent)
    }
}

private struct IPhoneRootView: View {
    @Environment(AppEnvironment.self) private var environment

    var body: some View {
        TabView {
            NavigationStack { HomeView() }
                .tabItem { Label("Home", systemImage: "house.fill") }

            NavigationStack { FMRadioView() }
                .tabItem { Label("FM", systemImage: "dot.radiowaves.left.and.right") }

            NavigationStack { DABRadioView() }
                .tabItem { Label("DAB", systemImage: "antenna.radiowaves.left.and.right") }

            NavigationStack { PresetsView() }
                .tabItem { Label("Preset", systemImage: "star.fill") }

            NavigationStack { AudioView() }
                .tabItem { Label("Audio", systemImage: "waveform") }

            NavigationStack { SettingsRootView() }
                .tabItem { Label("Impostazioni", systemImage: "gearshape.fill") }
        }
    }
}

private struct IPadRootView: View {
    @State private var selection: SidebarItem? = .home

    var body: some View {
        NavigationSplitView {
            List(SidebarItem.allCases, selection: $selection) { item in
                NavigationLink(value: item) {
                    Label(item.title, systemImage: item.systemImage)
                }
            }
            .navigationTitle("igiRadio")
        } detail: {
            switch selection ?? .home {
            case .home: HomeView()
            case .fm: FMRadioView()
            case .dab: DABRadioView()
            case .bluetooth: BluetoothView()
            case .audio: AudioView()
            case .presets: PresetsView()
            case .settings: SettingsRootView()
            }
        }
    }
}

enum SidebarItem: String, CaseIterable, Identifiable {
    case home, fm, dab, bluetooth, audio, presets, settings

    var id: String { rawValue }

    var title: String {
        switch self {
        case .home: "Home"
        case .fm: "FM"
        case .dab: "DAB"
        case .bluetooth: "Bluetooth"
        case .audio: "Audio"
        case .presets: "Preset"
        case .settings: "Impostazioni"
        }
    }

    var systemImage: String {
        switch self {
        case .home: "house.fill"
        case .fm: "dot.radiowaves.left.and.right"
        case .dab: "antenna.radiowaves.left.and.right"
        case .bluetooth: "dot.radiowaves.left.and.right"
        case .audio: "waveform"
        case .presets: "star.fill"
        case .settings: "gearshape.fill"
        }
    }
}
