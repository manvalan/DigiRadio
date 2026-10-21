import Foundation

/// Firmware `GainDb` accepts −96…+12 dB (see `GainDb.hpp`).
enum GainDbClamp {
    static let minDb: Double = -96
    static let maxDb: Double = 12

    static func clamp(_ db: Double) -> Double {
        min(maxDb, max(minDb, db))
    }
}

enum VolumeMapping {
    /// UI 0…100 ↔ master −96…+12 dB.
    /// La radio è già calda (~0 dBFS): 0 dB (unity) deve stare a metà fader,
    /// non in fondo. Silenzio compresso all’inizio, boost +12 nella seconda metà.
    private struct Anchor {
        let level: Double
        let db: Double
    }

    private static let anchors: [Anchor] = [
        Anchor(level: 0, db: GainDbClamp.minDb),
        Anchor(level: 12, db: -36),
        Anchor(level: 50, db: 0),
        Anchor(level: 100, db: GainDbClamp.maxDb),
    ]

    static func masterDb(forVolumeLevel level: Double) -> Double {
        GainDbClamp.clamp(lerp(max(0, min(100, level)), from: { $0.level }, to: { $0.db }))
    }

    static func volumeLevel(forMasterDb db: Double) -> Double {
        lerp(GainDbClamp.clamp(db), from: { $0.db }, to: { $0.level })
    }

    private static func lerp(
        _ x: Double,
        from: (Anchor) -> Double,
        to: (Anchor) -> Double
    ) -> Double {
        let first = anchors[0]
        let last = anchors[anchors.count - 1]
        if x <= from(first) { return to(first) }
        if x >= from(last) { return to(last) }
        for i in 1 ..< anchors.count {
            let a = anchors[i - 1]
            let b = anchors[i]
            let ax = from(a)
            let bx = from(b)
            if x <= bx {
                let span = bx - ax
                let u = span == 0 ? 0 : (x - ax) / span
                return to(a) + u * (to(b) - to(a))
            }
        }
        return to(last)
    }

    static func masterDb(forVolumePercent volume: Int) -> Double {
        masterDb(forVolumeLevel: Double(volume))
    }

    static func volumePercent(forMasterDb db: Double) -> Int {
        Int(round(volumeLevel(forMasterDb: db)))
    }

    static func formattedDb(_ db: Double) -> String {
        db >= 0 ? String(format: "+%.1f dB", db) : String(format: "%.1f dB", db)
    }
}

extension AudioProfileDTO {
    func clampedForDevice() -> AudioProfileDTO {
        var copy = self
        copy.master.leftDb = GainDbClamp.clamp(copy.master.leftDb)
        copy.master.rightDb = GainDbClamp.clamp(copy.master.rightDb)
        copy.eq = copy.eq.enumerated().map { index, band in
            EQBandState(
                index: index,
                gainDb: GainDbClamp.clamp(band.gainDb),
                centerHz: band.centerHz,
                q: band.q,
                isLocked: band.isLocked
            )
        }
        copy.enhancements.stereoLevel = max(0, min(100, copy.enhancements.stereoLevel))
        copy.enhancements.bassLevel = max(0, min(100, copy.enhancements.bassLevel))
        return copy
    }

    func mergingPreset(_ preset: AudioProfileTemplate) -> AudioProfileDTO {
        var merged = mergingPresetEQ(preset)
        merged.enhancements = preset.enhancements
        return merged.clampedForDevice()
    }

    func mergingPresetEQ(_ preset: AudioProfileTemplate) -> AudioProfileDTO {
        var merged = clampedForDevice()
        let targetBands = preset.eq.count == AudioProfileTemplate.bandCenters.count
            ? preset.eq
            : AudioProfileTemplate.eqBands(gains: preset.eq.map(\.gainDb))
        if merged.eq.count == targetBands.count {
            merged.eq = zip(targetBands, merged.eq).enumerated().map { index, pair in
                EQBandState(
                    index: index,
                    gainDb: pair.0.gainDb,
                    centerHz: pair.1.centerHz,
                    q: pair.1.q,
                    isLocked: pair.1.isLocked
                )
            }
        } else {
            merged.eq = targetBands
        }
        return merged.clampedForDevice()
    }
}
