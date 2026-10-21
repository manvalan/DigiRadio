import SwiftUI

struct RootView: View {
    @Environment(\.horizontalSizeClass) private var horizontalSizeClass
    @Environment(AppEnvironment.self) private var environment

    var body: some View {
        @Bindable var environment = environment
        Group {
            if horizontalSizeClass == .regular {
                IPadRootView()
            } else {
                IPhoneRootView()
            }
        }
        .preferredColorScheme(.light)
        .tint(IGITheme.accent)
        .igiThemedScreen()
        .sheet(isPresented: $environment.presentStreaming) {
            NavigationStack {
                StreamingView()
                    .toolbar {
                        ToolbarItem(placement: .confirmationAction) {
                            Button("Fine") { environment.presentStreaming = false }
                        }
                    }
            }
        }
    }
}

private struct IPhoneRootView: View {
    var body: some View {
        TabView {
            NavigationStack {
                HomeView()
            }
            .tabItem { Label("Ascolto", systemImage: "play.square.stack.fill") }

            NavigationStack {
                PresetsView()
            }
            .tabItem { Label("Radio", systemImage: "radio.fill") }

            NavigationStack {
                AppDestinationView(destination: .sound)
            }
            .tabItem { Label("Suono", systemImage: "slider.horizontal.3") }

            NavigationStack {
                SettingsRootView()
            }
            .tabItem { Label("Impostazioni", systemImage: "gearshape.fill") }

            NavigationStack {
                ConfigurationRootView()
            }
            .tabItem { Label("La mia radio", systemImage: "hifispeaker.and.homepod.fill") }
        }
    }
}

private struct IPadRootView: View {
    @State private var selection: AppDestination = .listening

    var body: some View {
        VStack(spacing: 0) {
            IPadTopMenu(selection: $selection)
            NavigationStack {
                AppDestinationView(destination: selection)
                    .id(selection)
            }
        }
        .background(IGITheme.canvas)
    }
}

private struct IPadTopMenu: View {
    @Binding var selection: AppDestination

    var body: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: 8) {
                ForEach(AppDestination.sidebarRoots) { destination in
                    Button {
                        selection = destination
                    } label: {
                        Label(destination.title, systemImage: destination.systemImage)
                            .font(.subheadline.weight(selection == destination ? .semibold : .medium))
                            .labelStyle(.titleAndIcon)
                            .padding(.horizontal, 14)
                            .padding(.vertical, 9)
                            .foregroundStyle(selection == destination ? IGITheme.ink : IGITheme.inkMuted)
                            .background(
                                selection == destination ? IGITheme.card : Color.clear,
                                in: Capsule()
                            )
                            .overlay(
                                Capsule()
                                    .strokeBorder(
                                        selection == destination
                                            ? IGITheme.ink.opacity(0.10)
                                            : Color.clear,
                                        lineWidth: 1
                                    )
                            )
                    }
                    .buttonStyle(.plain)
                    .accessibilityAddTraits(selection == destination ? .isSelected : [])
                }
            }
            .padding(.horizontal, 20)
            .padding(.vertical, 10)
        }
        .background(IGITheme.canvas)
        .overlay(alignment: .bottom) {
            Rectangle()
                .fill(IGITheme.ink.opacity(0.08))
                .frame(height: 1)
        }
    }
}
