import SwiftUI

struct AudioView: View {
    @Environment(AppEnvironment.self) private var environment
    @State private var stereoLevel: Double = 0
    @State private var bassLevel: Double = 0
    @State private var masterVolume: Double = 0
    @State private var eqBands: [EQBandState] = []

    var body: some View {
        Form {
            Section("Volume master") {
                IGIVolumeSlider(value: $masterVolume) { value in
                    Task { try? await environment.digiRadio.setVolume(Int(value)) }
                }
            }

            Section("Enhancements") {
                VStack(alignment: .leading) {
                    Text("Stereo enhance")
                    Slider(value: $stereoLevel, in: 0 ... 100, step: 1) { editing in
                        if !editing {
                            Task { try? await environment.digiRadio.setStereoEnhance(level: Int(stereoLevel)) }
                        }
                    }
                }
                VStack(alignment: .leading) {
                    Text("Bass enhance")
                    Slider(value: $bassLevel, in: 0 ... 100, step: 1) { editing in
                        if !editing {
                            Task { try? await environment.digiRadio.setBassEnhance(level: Int(bassLevel)) }
                        }
                    }
                }
            }

            Section("Equalizzatore (6 bande)") {
                ForEach($eqBands) { $band in
                    VStack(alignment: .leading) {
                        Text("\(Int(band.centerHz)) Hz")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                        Slider(value: $band.gainDb, in: -12 ... 12, step: 0.5) { editing in
                            if !editing { Task { await applyEQ() } }
                        }
                    }
                }
            }

            Section {
                Button("Reset profilo audio", role: .destructive) {
                    Task {
                        try? await environment.digiRadio.resetAudio()
                        syncFromState()
                    }
                }
            }
        }
        .navigationTitle("Audio")
        .onAppear {
            Task {
                try? await environment.digiRadio.refreshAudioProfile()
                syncFromState()
            }
        }
        .onChange(of: environment.state.audio.enhancements.stereoLevel) { _, _ in syncFromState() }
    }

    private func syncFromState() {
        let audio = environment.state.audio
        stereoLevel = Double(audio.enhancements.stereoLevel)
        bassLevel = Double(audio.enhancements.bassLevel)
        masterVolume = Double(environment.state.tuner.volume)
        eqBands = audio.eq
    }

    private func applyEQ() async {
        let profile = AudioProfileDTO(
            mixer: environment.state.audio.mixer,
            master: environment.state.audio.master,
            eq: eqBands,
            enhancements: EnhancementsState(stereoLevel: Int(stereoLevel), bassLevel: Int(bassLevel))
        )
        try? await environment.digiRadio.applyAudioProfile(profile)
    }
}
