import SwiftUI
import UniformTypeIdentifiers

struct StreamingView: View {
    @Environment(AppEnvironment.self) private var environment
    @State private var viewModel: StreamingViewModel?
    @State private var showFilePicker = false

    var body: some View {
        let vm = viewModel ?? StreamingViewModel(service: environment.digiRadio)
        Form {
            Section {
                Picker("Sorgente", selection: Binding(
                    get: { vm.source },
                    set: { vm.source = $0 }
                )) {
                    ForEach(StreamSource.allCases) { item in
                        Text(item.rawValue).tag(item)
                    }
                }
                .pickerStyle(.segmented)
                .listRowInsets(EdgeInsets(top: 10, leading: 16, bottom: 10, trailing: 16))
            } footer: {
                Text(vm.source.subtitle)
            }

            Section("Stato") {
                LabeledContent("Riproduzione", value: playbackLabel(vm))
                Text(statusSubtitle(vm))
                    .foregroundStyle(.secondary)
            }

            switch vm.source {
            case .otherApps:
                otherAppsSections(vm)
            case .webRadio:
                webRadioSections(vm)
            case .online:
                urlContentSections(vm)
            }

            if let error = vm.errorMessage {
                Section {
                    Text(error).foregroundStyle(.red)
                }
            }
        }
        .navigationTitle("Stream")
        .navigationBarTitleDisplayMode(.large)
        .fileImporter(
            isPresented: $showFilePicker,
            allowedContentTypes: [.audio, .mp3, .mpeg4Audio, .wav, .aiff],
            allowsMultipleSelection: false
        ) { result in
            if case let .success(urls) = result, let url = urls.first {
                vm.pickFile(url)
            }
        }
        .onAppear {
            if viewModel == nil { viewModel = vm }
            Task {
                await vm.load()
                await consumeIncomingShare(vm)
            }
        }
        .onChange(of: environment.incomingShare) { _, _ in
            Task { await consumeIncomingShare(vm) }
        }
        .onDisappear {
            viewModel?.stopPolling()
        }
    }

    private func playbackLabel(_ vm: StreamingViewModel) -> String {
        let active = vm.source == .otherApps ? vm.phoneStreamActive : vm.enabled
        return active ? "In riproduzione" : "Fermo"
    }

    private func statusSubtitle(_ vm: StreamingViewModel) -> String {
        switch vm.source {
        case .webRadio, .online:
            return vm.enabled ? vm.url : "Nessuno stream web attivo"
        case .otherApps:
            return vm.phoneStreamStatus.isEmpty ? "Nessun audio in invio" : vm.phoneStreamStatus
        }
    }

    @ViewBuilder
    private func otherAppsSections(_ vm: StreamingViewModel) -> some View {
        Section {
            Button {
                Task { await vm.startOtherApps() }
            } label: {
                Label("Ricevi audio dalle altre app", systemImage: "app.badge.checkmark")
            }
            .disabled(!vm.isConnected || vm.phoneStreamActive)

            Button {
                Task { await vm.stopPhone() }
            } label: {
                Label("Stop", systemImage: "stop.fill")
            }
            .disabled(!vm.phoneStreamActive)
        } header: {
            Text("Dal telefono")
        } footer: {
            Text("iOS non dà l’audio delle altre app: per questo chiede «Registra schermo». Serve solo l’audio, il microfono è spento. Poi torna a YouTube Music e tieni il brano in play.")
        }

        Section {
            Button {
                showFilePicker = true
            } label: {
                Label(
                    vm.selectedFileName.isEmpty ? "Oppure scegli un file audio" : vm.selectedFileName,
                    systemImage: "folder"
                )
            }
            Button {
                Task { await vm.playLocalFile() }
            } label: {
                Label("Invia file a DigiRadio", systemImage: "arrow.up.circle")
            }
            .disabled(!vm.isConnected || vm.selectedFileURL == nil || vm.phoneStreamActive)
        } header: {
            Text("File")
        }
    }

    @ViewBuilder
    private func webRadioSections(_ vm: StreamingViewModel) -> some View {
        Section("Webradio") {
            ForEach(StreamPresets.catalog) { preset in
                Button {
                    vm.selectPreset(preset)
                } label: {
                    VStack(alignment: .leading, spacing: 2) {
                        Text(preset.name)
                            .foregroundStyle(.primary)
                        Text(preset.url)
                            .font(.footnote)
                            .foregroundStyle(.secondary)
                            .lineLimit(1)
                    }
                }
            }
        }
        streamURLSection(vm, playTitle: "Play webradio")
    }

    @ViewBuilder
    private func urlContentSections(_ vm: StreamingViewModel) -> some View {
        Section {
            streamURLFields(vm)
        } header: {
            Text("URL")
        } footer: {
            Text("Il DigiRadio scarica e decodifica MP3. L’URL deve iniziare con http:// o https://.")
        }
        streamActions(vm, playTitle: "Play stream")
    }

    @ViewBuilder
    private func streamURLSection(_ vm: StreamingViewModel, playTitle: String) -> some View {
        Section("URL") {
            streamURLFields(vm)
        }
        streamActions(vm, playTitle: playTitle)
    }

    @ViewBuilder
    private func streamURLFields(_ vm: StreamingViewModel) -> some View {
        TextField("https://…", text: Binding(get: { vm.url }, set: { vm.url = $0 }))
            .textInputAutocapitalization(.never)
            .autocorrectionDisabled()
            .keyboardType(.URL)
    }

    @ViewBuilder
    private func streamActions(_ vm: StreamingViewModel, playTitle: String) -> some View {
        Section {
            Button {
                Task { await vm.playWebStream() }
            } label: {
                Label(playTitle, systemImage: "play.fill")
            }
            .disabled(vm.isSaving || !vm.isConnected)

            Button {
                Task { await vm.stopWebStream() }
            } label: {
                Label("Stop", systemImage: "stop.fill")
            }
            .disabled(vm.isSaving)
        }
    }

    @MainActor
    private func consumeIncomingShare(_ vm: StreamingViewModel) async {
        guard let share = environment.incomingShare else { return }
        environment.incomingShare = nil
        switch share {
        case let .webStream(url):
            vm.source = .online
            vm.url = url.absoluteString
            await vm.playWebStream()
        case let .otherApp(url):
            vm.source = .otherApps
            vm.url = url.absoluteString
            environment.adauStudio.syncFromDevice()
        }
    }
}
