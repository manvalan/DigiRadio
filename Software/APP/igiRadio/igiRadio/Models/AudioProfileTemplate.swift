import Foundation

/// Audio profile: EQ + enhancements (stored on device via PUT /api/audio/profile).
struct AudioProfileTemplate: Identifiable, Codable, Equatable, Hashable {
    var id: String
    var name: String
    var subtitle: String
    var systemImage: String
    var isBuiltIn: Bool
    var eq: [EQBandState]
    var enhancements: EnhancementsState

    /// ADAU1701 bands per firmware `EqProfile.cpp` / SigmaStudio (20 Hz HP + 5 peaking).
    static let bandCenters: [Double] = [20, 100, 400, 1000, 3000, 8000]
    static let bandQ: [Double] = [1.414, 1.0, 1.0, 1.0, 1.0, 1.0]

    static func eqBands(gains: [Double]) -> [EQBandState] {
        gains.enumerated().map { index, gain in
            EQBandState(
                index: index,
                gainDb: gain,
                centerHz: bandCenters[index],
                q: bandQ[index]
            )
        }
    }

    /// Preset gains applied on top of the device's current band layout (centre Hz / Q).
    func resolvedEQ(usingDevice deviceEQ: [EQBandState]) -> [EQBandState] {
        guard deviceEQ.count == eq.count else { return eq }
        return zip(eq, deviceEQ).enumerated().map { index, pair in
            EQBandState(
                index: index,
                gainDb: pair.0.gainDb,
                centerHz: pair.1.centerHz,
                q: pair.1.q,
                isLocked: pair.1.isLocked
            )
        }
    }

    func profileDTO(
        activeSource: AudioActiveSource,
        master: MasterVolumeState,
        deviceEQ: [EQBandState] = []
    ) -> AudioProfileDTO {
        let resolved = deviceEQ.isEmpty ? eq : resolvedEQ(usingDevice: deviceEQ)
        return AudioProfileDTO(
            activeSource: activeSource,
            master: master,
            eq: resolved,
            enhancements: enhancements
        )
    }

    func duplicatedAsUser(named name: String) -> AudioProfileTemplate {
        AudioProfileTemplate(
            id: UUID().uuidString,
            name: name,
            subtitle: "Profilo personalizzato",
            systemImage: "slider.horizontal.3",
            isBuiltIn: false,
            eq: eq,
            enhancements: enhancements
        )
    }
}

enum AudioProfileLibrary {
    static let builtIn: [AudioProfileTemplate] = [
        template(id: "builtin.flat", name: "Piatto", subtitle: "Risposta neutra", icon: "minus", gains: [0, 0, 0, 0, 0, 0], stereo: 0, bass: 0),
        template(id: "builtin.vocal", name: "Voci", subtitle: "Parlato e podcast", icon: "person.wave.2", gains: [-1, 0, 2, 4, 2, 0], stereo: 15, bass: 5),
        template(id: "builtin.bass", name: "Bassi", subtitle: "Più corpo e profondità", icon: "waveform.path", gains: [0, 5, 2, 0, -1, -2], stereo: 10, bass: 35),
        template(id: "builtin.treble", name: "Alti", subtitle: "Dettaglio e aria", icon: "sparkles", gains: [-1, -1, 0, 2, 4, 5], stereo: 25, bass: 0),
        template(id: "builtin.rock", name: "Rock", subtitle: "Energia V-shape", icon: "guitars", gains: [3, 4, -1, 1, 4, 4], stereo: 30, bass: 25),
        template(id: "builtin.jazz", name: "Jazz", subtitle: "Caldo e naturale", icon: "music.quarternote.3", gains: [2, 2, 1, 2, 1, 0], stereo: 20, bass: 15),
        template(id: "builtin.classical", name: "Classica", subtitle: "Equilibrio dinamico", icon: "hifispeaker.2", gains: [1, 1, 0, 0, 2, 3], stereo: 15, bass: 5),
        template(id: "builtin.lounge", name: "Lounge", subtitle: "Ascolto rilassato", icon: "moon.stars", gains: [1, 1, 0, -1, -2, -3], stereo: 10, bass: 20),
        template(id: "builtin.fm", name: "Radio FM", subtitle: "Voce in evidenza", icon: "radio", gains: [0, 0, 2, 3, 2, 1], stereo: 20, bass: 10)
    ]

    private static func template(
        id: String, name: String, subtitle: String, icon: String,
        gains: [Double], stereo: Int, bass: Int
    ) -> AudioProfileTemplate {
        AudioProfileTemplate(
            id: id,
            name: name,
            subtitle: subtitle,
            systemImage: icon,
            isBuiltIn: true,
            eq: AudioProfileTemplate.eqBands(gains: gains),
            enhancements: EnhancementsState(stereoLevel: stereo, bassLevel: bass)
        )
    }

    static func matchActive(_ profile: AudioProfileTemplate, eq: [EQBandState], enhancements: EnhancementsState) -> Bool {
        guard eq.count == profile.eq.count else { return false }
        let eqMatch = zip(eq, profile.eq).allSatisfy { abs($0.gainDb - $1.gainDb) < 0.6 }
        return eqMatch
            && profile.enhancements.stereoLevel == enhancements.stereoLevel
            && profile.enhancements.bassLevel == enhancements.bassLevel
            && profile.enhancements.voiceClean == enhancements.voiceClean
    }
}
