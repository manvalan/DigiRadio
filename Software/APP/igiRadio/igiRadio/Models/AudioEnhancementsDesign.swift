import Foundation

/// Replica firmware `EnhancementsDesign.cpp` — overlay stereo/bass sulle bande EQ.
enum AudioEnhancementsDesign {
    static func effectiveEQ(
        base: [EQBandState],
        enhancements: EnhancementsState
    ) -> [EQBandState] {
        guard !base.isEmpty else { return base }
        var bands = base

        if enhancements.bassLevel > 0 {
            let t = Double(enhancements.bassLevel) / 100.0
            applyGain(&bands, index: 1, gainDb: 9.0 * t, centerHz: 100, q: 0.9)
            applyGain(&bands, index: 2, gainDb: 3.0 * t, centerHz: 400, q: 1.0)
        }

        if enhancements.stereoLevel > 0 {
            let t = Double(enhancements.stereoLevel) / 100.0
            applyGain(&bands, index: 3, gainDb: -1.5 * t, centerHz: 1000, q: 1.0)
            applyGain(&bands, index: 4, gainDb: 2.0 * t, centerHz: 3000, q: 1.0)
            applyGain(&bands, index: 5, gainDb: 4.0 * t, centerHz: 8000, q: 1.0)
        }

        return bands.map { band in
            EQBandState(
                index: band.index,
                gainDb: GainDbClamp.clamp(band.gainDb),
                centerHz: band.centerHz,
                q: band.q
            )
        }
    }

    private static func applyGain(
        _ bands: inout [EQBandState],
        index: Int,
        gainDb: Double,
        centerHz: Double,
        q: Double
    ) {
        guard bands.indices.contains(index) else { return }
        let current = bands[index]
        bands[index] = EQBandState(
            index: index,
            gainDb: GainDbClamp.clamp(current.gainDb + gainDb),
            centerHz: centerHz,
            q: q
        )
    }
}

extension AudioEnhancementsDesign {
    /// Presence: taglia fango ~400 Hz, alza la voce a 1–3 kHz. Reversibile.
    static let voiceDeltas: [(index: Int, gainDb: Double)] = [
        (2, -2.0),
        (3, 1.5),
        (4, 3.5),
    ]

    static func applyingVoice(_ bands: [EQBandState]) -> [EQBandState] {
        applyVoice(bands, sign: 1)
    }

    static func removingVoice(_ bands: [EQBandState]) -> [EQBandState] {
        applyVoice(bands, sign: -1)
    }

    private static func applyVoice(_ bands: [EQBandState], sign: Double) -> [EQBandState] {
        var copy = bands
        for delta in voiceDeltas {
            guard copy.indices.contains(delta.index) else { continue }
            let band = copy[delta.index]
            copy[delta.index] = EQBandState(
                index: band.index,
                gainDb: GainDbClamp.clamp(band.gainDb + delta.gainDb * sign),
                centerHz: band.centerHz,
                q: band.q,
                isLocked: band.isLocked
            )
        }
        return copy
    }
}
