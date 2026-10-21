import SwiftUI

struct AudioProfilesView: View {
    @Environment(AppEnvironment.self) private var environment
    @State private var viewModel: AudioProfilesViewModel?

    var body: some View {
        Group {
            if let viewModel {
                AudioProfilesContent(viewModel: viewModel)
            } else {
                ProgressView()
            }
        }
        .onAppear {
            if viewModel == nil {
                viewModel = AudioProfilesViewModel(service: environment.digiRadio)
            }
        }
        .task {
            await viewModel?.load()
            environment.bassBoost.syncFromDevice()
        }
    }
}

private struct AudioProfilesContent: View {
    @Environment(AppEnvironment.self) private var environment
    @Bindable var viewModel: AudioProfilesViewModel
    @State private var editorProfile: AudioProfileTemplate?
    @State private var editorIsNew = false
    @State private var showNewProfileSheet = false
    @State private var newProfileName = ""

    var body: some View {
        List {
            if !viewModel.isConnected {
                Section {
                    Text("Non connesso — i preset si applicano solo dopo la connessione al DigiRadio.")
                        .foregroundStyle(.secondary)
                }
            }

            Section {
                LabeledContent("Profilo attivo", value: activeProfileName)
                if viewModel.isApplying || viewModel.isLoading {
                    HStack {
                        ProgressView()
                        Text("Aggiornamento…")
                            .foregroundStyle(.secondary)
                    }
                }
            }

            profileListSection(title: "Predefiniti", profiles: viewModel.builtIn, allowDelete: false, onDelete: nil)
            profileListSection(
                title: "I tuoi profili",
                profiles: viewModel.userProfiles,
                allowDelete: true,
                onDelete: { viewModel.deleteUserProfile($0) }
            )

            Section {
                Button {
                    newProfileName = "Il mio profilo"
                    showNewProfileSheet = true
                } label: {
                    Label("Nuovo profilo", systemImage: "plus")
                }
                Button("Ripristina audio di fabbrica", role: .destructive) {
                    Task { await viewModel.restoreFactoryAudio() }
                }
            }
        }
        .navigationTitle("Profili")
        .navigationBarTitleDisplayMode(.large)
        .navigationDestination(item: $editorProfile) { profile in
            AudioProfileEditorView(
                profile: profile,
                isNewProfile: editorIsNew,
                onSave: { saved in
                    viewModel.saveUserProfile(saved)
                    viewModel.reloadUserProfiles()
                    viewModel.activeProfileID = saved.id
                }
            )
        }
        .alert("Nuovo profilo", isPresented: $showNewProfileSheet) {
            TextField("Nome", text: $newProfileName)
            Button("Annulla", role: .cancel) {}
            Button("Crea") {
                let profile = viewModel.profileFromDevice(name: newProfileName.isEmpty ? "Il mio profilo" : newProfileName)
                editorIsNew = true
                editorProfile = profile
            }
        } message: {
            Text("Parte dal profilo attualmente sul DigiRadio.")
        }
        .alert("Profilo audio", isPresented: errorBinding) {
            Button("OK", role: .cancel) { viewModel.errorMessage = nil }
        } message: {
            Text(viewModel.errorMessage ?? "")
        }
    }

    private var activeProfileName: String {
        viewModel.builtIn.first(where: { $0.id == viewModel.activeProfileID })?.name
            ?? viewModel.userProfiles.first(where: { $0.id == viewModel.activeProfileID })?.name
            ?? "Personalizzato"
    }

    private var errorBinding: Binding<Bool> {
        Binding(
            get: { viewModel.errorMessage != nil },
            set: { if !$0 { viewModel.errorMessage = nil } }
        )
    }

    @ViewBuilder
    private func profileListSection(
        title: String,
        profiles: [AudioProfileTemplate],
        allowDelete: Bool,
        onDelete: ((AudioProfileTemplate) -> Void)?
    ) -> some View {
        Section(title) {
            if profiles.isEmpty {
                Text("Nessun profilo.")
                    .foregroundStyle(.secondary)
            } else {
                ForEach(profiles) { profile in
                    Button {
                        Task {
                            await viewModel.apply(profile)
                            environment.bassBoost.syncFromDevice()
                        }
                    } label: {
                        HStack {
                            VStack(alignment: .leading, spacing: 2) {
                                Text(profile.name)
                                Text(profile.subtitle)
                                    .font(.footnote)
                                    .foregroundStyle(.secondary)
                            }
                            Spacer()
                            if viewModel.activeProfileID == profile.id {
                                Image(systemName: "checkmark")
                                    .foregroundStyle(Color.accentColor)
                            }
                        }
                    }
                    .foregroundStyle(.primary)
                    .swipeActions(edge: .trailing, allowsFullSwipe: false) {
                        Button {
                            editorIsNew = false
                            editorProfile = profile
                        } label: {
                            Label("Modifica", systemImage: "pencil")
                        }
                        .tint(.orange)
                        if allowDelete {
                            Button(role: .destructive) {
                                onDelete?(profile)
                            } label: {
                                Label("Elimina", systemImage: "trash")
                            }
                        }
                    }
                    .contextMenu {
                        Button {
                            editorIsNew = false
                            editorProfile = profile
                        } label: {
                            Label("Modifica", systemImage: "pencil")
                        }
                        if allowDelete {
                            Button(role: .destructive) {
                                onDelete?(profile)
                            } label: {
                                Label("Elimina", systemImage: "trash")
                            }
                        }
                    }
                }
            }
        }
    }
}
