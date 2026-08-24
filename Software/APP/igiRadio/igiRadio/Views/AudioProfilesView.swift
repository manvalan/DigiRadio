import SwiftUI

struct AudioProfilesView: View {
    @Environment(AppEnvironment.self) private var environment
    @State private var viewModel: AudioProfilesViewModel?
    @State private var editorProfile: AudioProfileTemplate?
    @State private var editorIsNew = false
    @State private var showNewProfileSheet = false
    @State private var newProfileName = ""

    var body: some View {
        let vm = viewModel ?? AudioProfilesViewModel(service: environment.digiRadio)

        ScrollView {
            VStack(alignment: .leading, spacing: IGITheme.spacingL) {
                activeBanner(vm)
                profileSection(title: "Predefiniti", profiles: vm.builtIn, vm: vm, allowDelete: false, onDelete: nil)
                profileSection(title: "I tuoi profili", profiles: vm.userProfiles, vm: vm, allowDelete: true, onDelete: { vm.deleteUserProfile($0) })

                if vm.userProfiles.isEmpty {
                    Text("Crea un profilo personalizzato partendo da un preset o dal suono attuale del device.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                        .igiPremiumCard()
                }

                HStack(spacing: IGITheme.spacingM) {
                    Button {
                        newProfileName = "Il mio profilo"
                        showNewProfileSheet = true
                    } label: {
                        Label("Nuovo profilo", systemImage: "plus.circle.fill")
                            .frame(maxWidth: .infinity)
                            .padding(.vertical, 14)
                    }
                    .buttonStyle(.borderedProminent)

                    NavigationLink {
                        AudioView()
                    } label: {
                        Label("Mixer", systemImage: "slider.vertical.3")
                            .frame(maxWidth: .infinity)
                            .padding(.vertical, 14)
                    }
                    .buttonStyle(.bordered)
                }
            }
            .padding(IGITheme.spacingM)
        }
        .background(IGIHeroBackground())
        .navigationTitle("Profilo audio")
        .navigationBarTitleDisplayMode(.large)
        .navigationDestination(item: $editorProfile) { profile in
            AudioProfileEditorView(
                profile: profile,
                isNewProfile: editorIsNew,
                onSave: { saved in
                    vm.saveUserProfile(saved)
                    vm.reloadUserProfiles()
                    vm.activeProfileID = saved.id
                }
            )
        }
        .alert("Nuovo profilo", isPresented: $showNewProfileSheet) {
            TextField("Nome", text: $newProfileName)
            Button("Annulla", role: .cancel) {}
            Button("Crea") {
                let profile = vm.profileFromDevice(name: newProfileName.isEmpty ? "Il mio profilo" : newProfileName)
                editorIsNew = true
                editorProfile = profile
            }
        } message: {
            Text("Parte dal profilo attualmente sul DigiRadio.")
        }
        .onAppear {
            if viewModel == nil { viewModel = vm }
            Task { await vm.load() }
        }
    }

    @ViewBuilder
    private func activeBanner(_ vm: AudioProfilesViewModel) -> some View {
        HStack(spacing: IGITheme.spacingM) {
            Image(systemName: "checkmark.seal.fill")
                .font(.title2)
                .foregroundStyle(IGITheme.accent)
            VStack(alignment: .leading, spacing: 2) {
                Text("Profilo attivo")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                Text(vm.builtIn.first(where: { $0.id == vm.activeProfileID })?.name
                     ?? vm.userProfiles.first(where: { $0.id == vm.activeProfileID })?.name
                     ?? "Personalizzato / device")
                    .font(.headline)
            }
            Spacer()
            if vm.isApplying { ProgressView() }
        }
        .igiPremiumCard()
    }

    @ViewBuilder
    private func profileSection(
        title: String,
        profiles: [AudioProfileTemplate],
        vm: AudioProfilesViewModel,
        allowDelete: Bool,
        onDelete: ((AudioProfileTemplate) -> Void)?
    ) -> some View {
        VStack(alignment: .leading, spacing: IGITheme.spacingM) {
            Text(title)
                .font(.title3.weight(.bold))

            LazyVGrid(columns: [GridItem(.adaptive(minimum: 150), spacing: IGITheme.spacingS)], spacing: IGITheme.spacingS) {
                ForEach(profiles) { profile in
                    AudioProfileCard(
                        profile: profile,
                        isActive: vm.activeProfileID == profile.id,
                        allowDelete: allowDelete
                    ) {
                        Task { await vm.apply(profile) }
                    } onEdit: {
                        editorIsNew = false
                        editorProfile = profile
                    } onDelete: {
                        onDelete?(profile)
                    }
                }
            }
        }
        .igiPremiumCard()
    }
}

private struct AudioProfileCard: View {
    var profile: AudioProfileTemplate
    var isActive: Bool
    var allowDelete: Bool
    var onApply: () -> Void
    var onEdit: () -> Void
    var onDelete: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: IGITheme.spacingS) {
            HStack {
                Image(systemName: profile.systemImage)
                    .foregroundStyle(isActive ? .white : IGITheme.accent)
                Spacer()
                if isActive {
                    Image(systemName: "checkmark.circle.fill")
                        .foregroundStyle(.white)
                }
            }
            Text(profile.name)
                .font(.headline)
                .foregroundStyle(isActive ? .white : .primary)
            Text(profile.subtitle)
                .font(.caption2)
                .foregroundStyle(isActive ? .white.opacity(0.85) : .secondary)
                .lineLimit(2)

            HStack(spacing: IGITheme.spacingS) {
                Button("Applica", action: onApply)
                    .font(.caption.weight(.semibold))
                    .buttonStyle(.borderedProminent)
                    .tint(isActive ? .white : IGITheme.accent)

                Button("Modifica", action: onEdit)
                    .font(.caption.weight(.semibold))
                    .buttonStyle(.bordered)
                    .tint(isActive ? .white : .primary)
            }
            .padding(.top, 4)
        }
        .padding(IGITheme.spacingM)
        .background(
            isActive
                ? AnyShapeStyle(IGITheme.accent.gradient)
                : AnyShapeStyle(Color.primary.opacity(0.05)),
            in: RoundedRectangle(cornerRadius: 16, style: .continuous)
        )
        .contextMenu {
            if allowDelete {
                Button(role: .destructive, action: onDelete) {
                    Label("Elimina", systemImage: "trash")
                }
            }
        }
    }
}
