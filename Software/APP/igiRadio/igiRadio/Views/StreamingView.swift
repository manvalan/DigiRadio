import SwiftUI
import UniformTypeIdentifiers

struct StreamingView: View {
    @Environment(AppEnvironment.self) private var environment
    @State private var viewModel: StreamingViewModel?
    @State private var showFilePicker = false

    var body: some View {
        let vm = viewModel ?? StreamingViewModel(service: environment.digiRadio)

        ScrollView {
            VStack(alignment: .leading, spacing: IGITheme.spacingL) {
                sourcePicker(vm)
                statusHero(vm)

                switch vm.source {
                case .webRadio:
                    webRadioSection(vm)
                case .urlContent:
                    urlContentSection(vm)
                case .localFile:
                    localFileSection(vm)
                }

                if let error = vm.errorMessage {
                    Text(error)
                        .font(.caption)
                        .foregroundStyle(.red)
                        .igiPremiumCard()
                }
            }
            .padding(IGITheme.spacingM)
        }
        .background(IGIHeroBackground())
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
            Task { await vm.load() }
        }
        .onDisappear {
            viewModel?.stopPolling()
        }
    }

    @ViewBuilder
    private func sourcePicker(_ vm: StreamingViewModel) -> some View {
        VStack(alignment: .leading, spacing: IGITheme.spacingS) {
            Text("Sorgente")
                .font(.headline)
            ForEach(StreamSource.allCases) { item in
                Button {
                    withAnimation(.snappy) { vm.source = item }
                } label: {
                    HStack(spacing: IGITheme.spacingM) {
                        Image(systemName: item.icon)
                            .frame(width: 28)
                            .foregroundStyle(vm.source == item ? .white : IGITheme.accent)
                        VStack(alignment: .leading, spacing: 2) {
                            Text(item.rawValue).font(.subheadline.weight(.semibold))
                            Text(item.subtitle).font(.caption2).foregroundStyle(vm.source == item ? .white.opacity(0.85) : .secondary)
                        }
                        Spacer()
                        if vm.source == item {
                            Image(systemName: "checkmark.circle.fill")
                        }
                    }
                    .padding(IGITheme.spacingM)
                    .background(
                        vm.source == item
                            ? AnyShapeStyle(IGITheme.accent.gradient)
                            : AnyShapeStyle(Color.primary.opacity(0.05)),
                        in: RoundedRectangle(cornerRadius: 14, style: .continuous)
                    )
                    .foregroundStyle(vm.source == item ? .white : .primary)
                }
                .buttonStyle(.plain)
            }
        }
        .igiPremiumCard()
    }

    @ViewBuilder
    private func statusHero(_ vm: StreamingViewModel) -> some View {
        let active = vm.source == .localFile ? vm.phoneStreamActive : vm.enabled
        VStack(spacing: IGITheme.spacingM) {
            Image(systemName: active ? "dot.radiowaves.forward" : "pause.circle")
                .font(.system(size: 48))
                .foregroundStyle(active ? IGITheme.accent : .secondary)
                .symbolEffect(.pulse, options: .repeating, value: active)

            Text(active ? "In riproduzione" : "Fermo")
                .font(.title3.weight(.bold))
            Text(statusSubtitle(vm))
                .font(.caption)
                .foregroundStyle(.secondary)
                .multilineTextAlignment(.center)
        }
        .frame(maxWidth: .infinity)
        .igiPremiumCard()
    }

    private func statusSubtitle(_ vm: StreamingViewModel) -> String {
        switch vm.source {
        case .webRadio, .urlContent:
            return vm.enabled ? vm.url : "Nessuno stream web attivo"
        case .localFile:
            return vm.phoneStreamStatus.isEmpty ? "Nessun file in invio" : vm.phoneStreamStatus
        }
    }

    @ViewBuilder
    private func webRadioSection(_ vm: StreamingViewModel) -> some View {
        VStack(alignment: .leading, spacing: IGITheme.spacingM) {
            Text("Stazioni")
                .font(.headline)
            ForEach(StreamPresets.catalog) { preset in
                Button {
                    vm.selectPreset(preset)
                } label: {
                    HStack {
                        VStack(alignment: .leading) {
                            Text(preset.name).font(.headline)
                            Text(preset.url).font(.caption2).foregroundStyle(.secondary).lineLimit(1)
                        }
                        Spacer()
                        Image(systemName: "play.circle.fill").font(.title2)
                    }
                }
                .buttonStyle(.plain)
                if preset.id != StreamPresets.catalog.last?.id {
                    Divider()
                }
            }
        }
        .igiPremiumCard()

        streamURLControls(vm, playLabel: "Play radio")
    }

    @ViewBuilder
    private func urlContentSection(_ vm: StreamingViewModel) -> some View {
        VStack(alignment: .leading, spacing: IGITheme.spacingS) {
            Text("Contenuto via URL")
                .font(.headline)
            Text("Il DigiRadio scarica e decodifica MP3 HTTP sul device. L'URL deve iniziare con http:// (max 200 caratteri).")
                .font(.caption)
                .foregroundStyle(.secondary)
        }
        .igiPremiumCard()

        streamURLControls(vm, playLabel: "Play contenuto")
    }

    @ViewBuilder
    private func streamURLControls(_ vm: StreamingViewModel, playLabel: String) -> some View {
        VStack(alignment: .leading, spacing: IGITheme.spacingM) {
            TextField("http://…", text: Binding(get: { vm.url }, set: { vm.url = $0 }))
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled()
                .keyboardType(.URL)
                .padding()
                .background(Color.primary.opacity(0.05), in: RoundedRectangle(cornerRadius: 12))

            HStack(spacing: IGITheme.spacingM) {
                Button { Task { await vm.playWebStream() } } label: {
                    Label(playLabel, systemImage: "play.fill")
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 14)
                }
                .buttonStyle(.borderedProminent)
                .disabled(vm.isSaving || !vm.isConnected)

                Button { Task { await vm.stopWebStream() } } label: {
                    Label("Stop", systemImage: "stop.fill")
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 14)
                }
                .buttonStyle(.bordered)
                .disabled(vm.isSaving)
            }
        }
        .igiPremiumCard()
    }

    @ViewBuilder
    private func localFileSection(_ vm: StreamingViewModel) -> some View {
        VStack(alignment: .leading, spacing: IGITheme.spacingM) {
            Text("File sul telefono")
                .font(.headline)
            Text("L'iPhone decodifica il file e invia PCM stereo 48 kHz a PUT /api/stream/phone. Ferma prima eventuali stream web sul device.")
                .font(.caption)
                .foregroundStyle(.secondary)

            Button {
                showFilePicker = true
            } label: {
                Label(vm.selectedFileName.isEmpty ? "Scegli file audio" : vm.selectedFileName, systemImage: "folder")
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding()
                    .background(Color.primary.opacity(0.05), in: RoundedRectangle(cornerRadius: 12))
            }
            .buttonStyle(.plain)

            HStack(spacing: IGITheme.spacingM) {
                Button { Task { await vm.playLocalFile() } } label: {
                    Label("Invia a DigiRadio", systemImage: "arrow.up.circle.fill")
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 14)
                }
                .buttonStyle(.borderedProminent)
                .disabled(!vm.isConnected || vm.selectedFileURL == nil || vm.phoneStreamActive)

                Button { Task { await vm.stopLocalFile() } } label: {
                    Label("Stop", systemImage: "stop.fill")
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 14)
                }
                .buttonStyle(.bordered)
                .disabled(!vm.phoneStreamActive)
            }
        }
        .igiPremiumCard()
    }
}
