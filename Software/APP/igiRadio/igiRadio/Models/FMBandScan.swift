import Foundation

/// Parametri scansione FM banda III europea (87.5–107.9 MHz, passo 100 kHz).
/// Allineati a `TunerService.cpp` nel firmware.
enum FMBandScan {
    static let minKhz = 87_500
    static let maxKhz = 107_900
    static let stepKhz = 100
    static let minRssiDbuv = 5
    /// Chip VALID SNR (AN649 0x3204) is 3 dB; the full-band firmware scan
    /// does not require SNR once `locked` is set — only RSSI + lock.
    static let minSnrDb = 3
    /// Sotto questa soglia, in ascolto, si prova un'altra frequenza dello stesso nome.
    static let weakRssiDbuv = 12
    static let afImprovementDb = 6
    static let afSettleMs: UInt64 = 250
    static let afListenSettleMs: UInt64 = 8_000
    static let afPollMs: UInt64 = 4_000
    static let afCooldownMs: UInt64 = 20_000
    static let tuneSettleMs: UInt64 = 120
    static let namePollMs: UInt64 = 250
    static let namePollAttempts = 6
    /// Un passo per canale europeo (87.5–107.9 / 100 kHz), se lo seek HW
    /// ricade sul software step da 100 kHz.
    static let maxSeekSteps = 205

    static var frequencies: [Int] {
        Array(stride(from: minKhz, through: maxKhz, by: stepKhz))
    }

    static func isUsableHit(
        fm: FMTunerState,
        locked: Bool,
        minRssiDbuv: Int = FMScanPreferences.minRssiDbuv,
        minSnrDb: Int = FMScanPreferences.minSnrDb
    ) -> Bool {
        guard locked else { return false }
        guard (fm.rssiDbuv ?? -128) >= minRssiDbuv else { return false }
        if minSnrDb > 0 {
            guard (fm.snrDb ?? -128) >= minSnrDb else { return false }
        }
        return true
    }
}

/// Scelta della frequenza alternativa (AF) lato iPhone, solo con firmware
/// precedenti alla 0.9.2: dalla 0.9.2 la radio segue le AF da sola (/api/tuner/af).
enum AlternateFrequencyHunt {
    struct Snapshot: Equatable {
        var khz: Int
        var rssiDbuv: Int
        var locked: Bool
    }

    static func isWeak(_ snapshot: Snapshot) -> Bool {
        !snapshot.locked || snapshot.rssiDbuv < FMBandScan.weakRssiDbuv
    }

    static func shouldSwitch(from current: Snapshot, to candidate: Snapshot) -> Bool {
        let candidateUsable = candidate.locked && candidate.rssiDbuv >= FMBandScan.minRssiDbuv
        guard candidateUsable else { return false }
        let currentUsable = current.locked && current.rssiDbuv >= FMBandScan.minRssiDbuv
        if candidate.rssiDbuv >= current.rssiDbuv + FMBandScan.afImprovementDb {
            return true
        }
        return !currentUsable
    }

    static func best(of samples: [Snapshot]) -> Snapshot? {
        samples.max { lhs, rhs in
            if lhs.locked != rhs.locked { return !lhs.locked }
            return lhs.rssiDbuv < rhs.rssiDbuv
        }
    }
}
