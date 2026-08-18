import SwiftUI

struct AudioView: View {
    @Environment(AppEnvironment.self) private var environment
    @State private var viewModel: AudioViewModel?

    var body: some View {
        let vm = viewModel ?? AudioViewModel(service: environment.digiRadio)

        ScrollView {
            VStack(alignment: .leading, spacing: IGITheme.spacingL) {
                header(vm)

                if vm.isLoading {
                    ProgressView("Caricamento profilo audio…")
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 40)
                } else {
                    IGIMasterVolumeHero(
                        leftDb: Binding(
                            get: { vm.master.leftDb },
                            set: { vm.master.leftDb = $0; vm.scheduleApply() }
                        ),
                        rightDb: Binding(
                            get: { vm.master.rightDb },
                            set: { vm.master.rightDb = $0; vm.scheduleApply() }
                        ),
                        onCommit: { vm.scheduleApply() }
                    )

                    panelPicker(vm)

                    switch vm.panel {
                    case .mixer:
                        mixerPanel(vm)
                    case .enhance:
                        enhancePanel(vm)
                    }

                    resetButton(vm)
                }
            }
            .padding(IGITheme.spacingM)
        }
        .background(
            LinearGradient(
                colors: [
                    IGITheme.screenBackground,
                    IGITheme.accent.opacity(0.06),
                    IGITheme.screenBackground
                ],
                startPoint: .topLeading,
                endPoint: .bottomTrailing
            )
            .ignoresSafeArea()
        )
        .navigationTitle("Audio")
        .navigationBarTitleDisplayMode(.large)
        .onAppear {
            if viewModel == nil { viewModel = vm }
            Task { await vm.load() }
        }
    }

    @ViewBuilder
    private func header(_ vm: AudioViewModel) -> some View {
        HStack {
            VStack(alignment: .leading, spacing: 4) {
                Text("ADAU1701")
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(IGITheme.accent)
                Text("Mixer · EQ · DSP")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
            Spacer()
            if vm.isSaving {
                HStack(spacing: 6) {
                    ProgressView().controlSize(.small)
                    Text("Salvataggio")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
        }
    }

    @ViewBuilder
    private func panelPicker(_ vm: AudioViewModel) -> some View {
        HStack(spacing: IGITheme.spacingS) {
            ForEach(AudioPanel.allCases) { panel in
                Button {
                    withAnimation(.snappy) { vm.panel = panel }
                } label: {
                    Label(panel.rawValue, systemImage: panel.icon)
                        .font(.subheadline.weight(.semibold))
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 12)
                        .background(
                            vm.panel == panel
                                ? AnyShapeStyle(IGITheme.accent.gradient)
                                : AnyShapeStyle(Color.primary.opacity(0.06)),
                            in: RoundedRectangle(cornerRadius: 12, style: .continuous)
                        )
                        .foregroundStyle(vm.panel == panel ? .white : .primary)
                }
                .buttonStyle(.plain)
            }
        }
    }

    @ViewBuilder
    private func mixerPanel(_ vm: AudioViewModel) -> some View {
        VStack(spacing: IGITheme.spacingM) {
            IGIMixerChannelGroup(
                title: "Radio Si4684",
                subtitle: "FM / DAB — ingresso tuner",
                systemImage: "antenna.radiowaves.left.and.right",
                leftDb: Binding(
                    get: { vm.mixer.si4684LeftDb },
                    set: { vm.mixer.si4684LeftDb = $0 }
                ),
                rightDb: Binding(
                    get: { vm.mixer.si4684RightDb },
                    set: { vm.mixer.si4684RightDb = $0 }
                ),
                onCommit: { vm.scheduleApply() }
            )

            IGIMixerChannelGroup(
                title: "Stream ESP32",
                subtitle: "Web radio / phone push",
                systemImage: "waveform",
                leftDb: Binding(
                    get: { vm.mixer.esp32LeftDb },
                    set: { vm.mixer.esp32LeftDb = $0 }
                ),
                rightDb: Binding(
                    get: { vm.mixer.esp32RightDb },
                    set: { vm.mixer.esp32RightDb = $0 }
                ),
                onCommit: { vm.scheduleApply() }
            )

            IGIMixerChannelGroup(
                title: "Mix bus",
                subtitle: "Uscita mixer ADAU",
                systemImage: "speaker.wave.2.fill",
                leftDb: Binding(
                    get: { vm.mixer.mixLeftDb },
                    set: { vm.mixer.mixLeftDb = $0 }
                ),
                rightDb: Binding(
                    get: { vm.mixer.mixRightDb },
                    set: { vm.mixer.mixRightDb = $0 }
                ),
                onCommit: { vm.scheduleApply() }
            )
        }
    }

    @ViewBuilder
    private func enhancePanel(_ vm: AudioViewModel) -> some View {
        HStack(spacing: IGITheme.spacingL) {
            IGIEnhancementDial(
                title: "Stereo",
                systemImage: "circle.lefthalf.filled",
                level: Binding(
                    get: { vm.stereoLevel },
                    set: { vm.stereoLevel = $0 }
                ),
                onCommit: { Task { await vm.applyEnhancements() } }
            )
            IGIEnhancementDial(
                title: "Bass",
                systemImage: "waveform.path",
                level: Binding(
                    get: { vm.bassLevel },
                    set: { vm.bassLevel = $0 }
                ),
                onCommit: { Task { await vm.applyEnhancements() } }
            )
        }
        .igiAudioCard()
    }

    @ViewBuilder
    private func resetButton(_ vm: AudioViewModel) -> some View {
        Button(role: .destructive) {
            Task { await vm.reset() }
        } label: {
            Label("Ripristina profilo audio", systemImage: "arrow.counterclockwise")
                .frame(maxWidth: .infinity)
                .padding(.vertical, 14)
        }
        .buttonStyle(.bordered)
        .padding(.top, IGITheme.spacingS)
    }
}
