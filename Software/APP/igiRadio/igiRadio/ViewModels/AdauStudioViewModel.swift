import Foundation
import Observation
import SwiftUI

enum AdauStudioSection: String, CaseIterable, Identifiable {
    case listening = "Ascolto"
    case sound = "Suono"
    case diagnostics = "Diagnostica"

    var id: String { rawValue }

    var icon: String {
        switch self {
        case .listening: "speaker.wave.2.fill"
        case .sound: "waveform.path.ecg"
        case .diagnostics: "wrench.and.screwdriver"
        }
    }
}

@Observable
final class AdauStudioViewModel {
    private var service: any DigiRadioService
    private var applyTask: Task<Void, Never>?
    private var levelsPollTask: Task<Void, Never>?
    private var sourceBeforeBeep: AudioActiveSource = .radio
    private var latestPresetToApply: AudioProfileTemplate?
    private var isFlushingPresets = false
    private var userDivergedFromPreset = false
    private var pendingApply = false

    var section: AdauStudioSection = .listening
    var activeSource: AudioActiveSource = .radio
    var master = MasterVolumeState()
    var eqBands: [EQBandState] = []
    var selectedEQPresetID: String?
    var stereoLevel: Double = 0
    var bassLevel: Double = 0
    var voiceClean = false
    var dspParams: [DspParamEntry] = []
    var dspSearchText = ""
    var selectedDspParam: DspParamEntry?
    var dspParamValue = ""
    var masterLevel: Double = 50
    var outputLevels: AudioLevelsDTO?
    var outputLevelsAvailable = false
    var isLoading = false
    var isSaving = false
    var sourceChangeToken = UUID()
    var errorMessage: String?
    var statusMessage: String?

    init(service: any DigiRadioService) {
        self.service = service
        voiceClean = UserDefaults.standard.bool(forKey: Self.voiceClarifierDefaultsKey)
    }

    func replaceService(_ service: any DigiRadioService) {
        stopOutputLevelsPolling()
        self.service = service
    }

    var isConnected: Bool { service.state.connection.isConnected }

    var masterDbLabel: String {
        VolumeMapping.formattedDb((master.leftDb + master.rightDb) / 2)
    }

    var filteredDspParams: [DspParamEntry] {
        let query = dspSearchText.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !query.isEmpty else { return dspParams }
        return dspParams.filter {
            $0.name.localizedCaseInsensitiveContains(query)
                || String($0.address).contains(query)
        }
    }

    func load() async {
        isLoading = true
        errorMessage = nil
        defer { isLoading = false }
        do {
            try await service.refreshAudioProfile()
            syncFromDevice()
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }

    func syncFromDevice() {
        let audio = service.state.audio
        activeSource = audio.activeSource
        master = audio.master
        eqBands = audio.eq
        stereoLevel = Double(audio.enhancements.stereoLevel)
        bassLevel = Double(audio.enhancements.bassLevel)
        masterLevel = VolumeMapping.volumeLevel(forMasterDb: (master.leftDb + master.rightDb) / 2)
        if eqBands.isEmpty {
            eqBands = AudioProfileTemplate.eqBands(gains: Array(repeating: 0, count: 6))
        }
        if selectedEQPresetID == nil, !userDivergedFromPreset {
            selectedEQPresetID = matchingEQPreset()?.id
        }
    }

    func pushMasterLevelToChannels() {
        let db = VolumeMapping.masterDb(forVolumeLevel: masterLevel)
        master.leftDb = db
        master.rightDb = db
    }

    func commitMasterVolume() {
        pushMasterLevelToChannels()
        scheduleApply()
    }

    func startOutputLevelsPolling() {
        stopOutputLevelsPolling()
        levelsPollTask = Task { [weak self] in
            while let self, !Task.isCancelled {
                await self.pollOutputLevels()
                try? await Task.sleep(for: .milliseconds(350))
            }
        }
    }

    func stopOutputLevelsPolling() {
        levelsPollTask?.cancel()
        levelsPollTask = nil
        outputLevelsAvailable = false
        outputLevels = nil
    }

    @MainActor
    private func pollOutputLevels() async {
        guard isConnected, !service.isBackgroundRefreshPaused else {
            outputLevelsAvailable = false
            outputLevels = nil
            return
        }
        do {
            let levels = try await service.fetchAudioLevels()
            outputLevels = levels
            outputLevelsAvailable = true
        } catch {
            outputLevelsAvailable = false
            outputLevels = nil
        }
    }

    func applyActiveSource(_ source: AudioActiveSource) async {
        activeSource = source
        sourceChangeToken = UUID()
        await applyNow()
    }

    func scheduleApply() {
        applyTask?.cancel()
        applyTask = Task {
            try? await Task.sleep(for: .milliseconds(450))
            guard !Task.isCancelled else { return }
            await applyNow()
        }
    }

    func applyNow() async {
        if isSaving {
            pendingApply = true
            return
        }
        isSaving = true
        errorMessage = nil
        statusMessage = nil
        defer { isSaving = false }
        repeat {
            pendingApply = false
            let profile = currentProfileDTO()
            do {
                try await service.applyAudioProfile(profile)
                syncFromDevice()
                statusMessage = "Impostazioni applicate."
            } catch {
                errorMessage = DigiRadioFriendlyError.message(for: error)
            }
        } while pendingApply
    }

    func applyEnhancements() async {
        await applyNow()
    }

    func reset() async {
        isSaving = true
        errorMessage = nil
        statusMessage = nil
        defer { isSaving = false }
        do {
            if voiceClean {
                eqBands = AudioEnhancementsDesign.removingVoice(eqBands)
            }
            voiceClean = false
            persistVoiceClarifier()
            try await service.resetAudio()
            userDivergedFromPreset = false
            selectedEQPresetID = nil
            syncFromDevice()
            statusMessage = "Profilo ripristinato."
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }

    func loadDspParams() async {
        guard isConnected else {
            errorMessage = "Connetti a DigiRadio."
            return
        }
        do {
            let response = try await service.fetchDspParams()
            dspParams = response.params.sorted { $0.address < $1.address }
            statusMessage = "\(dspParams.count) parametri DSP caricati."
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }

    func writeDspParam() async {
        guard let param = selectedDspParam else { return }
        guard let value = Double(dspParamValue.replacingOccurrences(of: ",", with: ".")) else {
            errorMessage = "Valore non valido."
            return
        }
        isSaving = true
        errorMessage = nil
        defer { isSaving = false }
        do {
            try await service.setDspParam(name: param.name, value: value)
            statusMessage = "\(param.name) = \(value) (live, non persistente)"
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }

    func testBeep(enabled: Bool) async {
        guard isConnected else {
            errorMessage = "Connetti a DigiRadio."
            return
        }
        isSaving = true
        errorMessage = nil
        defer { isSaving = false }
        do {
            if enabled {
                sourceBeforeBeep = activeSource == .beep ? .radio : activeSource
                activeSource = .beep
                try await service.applyAudioProfile(currentProfileDTO())
                try await service.playAudioBeep(enabled: true)
                statusMessage = "Tono di test attivo (sorgente: beep)."
            } else {
                try await service.playAudioBeep(enabled: false)
                activeSource = sourceBeforeBeep
                try await service.applyAudioProfile(currentProfileDTO())
                syncFromDevice()
                statusMessage = "Tono di test disattivato."
            }
        } catch {
            errorMessage = DigiRadioFriendlyError.message(for: error)
        }
    }

    private func currentProfileDTO() -> AudioProfileDTO {
        AudioProfileDTO(
            activeSource: activeSource,
            master: master,
            eq: eqBands,
            enhancements: EnhancementsState(
                stereoLevel: Int(stereoLevel),
                bassLevel: Int(bassLevel),
                voiceClean: voiceClean
            )
        ).clampedForDevice()
    }
}

extension AdauStudioViewModel {
    func matchingEQPreset() -> AudioProfileTemplate? {
        AudioProfileStore.allProfiles().first {
            AudioProfileLibrary.matchActive(
                $0,
                eq: eqBands,
                enhancements: EnhancementsState(
                    stereoLevel: Int(stereoLevel),
                    bassLevel: Int(bassLevel)
                )
            )
        }
    }

    var selectedEQPresetName: String? {
        guard let id = selectedEQPresetID else { return nil }
        return AudioProfileStore.allProfiles().first { $0.id == id }?.name
    }

    /// L'utente ha toccato EQ / SuperPhat / Bass: non è più "quel" preset.
    func notePresetEdited() {
        userDivergedFromPreset = true
        selectedEQPresetID = nil
    }

    func applyEQPreset(_ preset: AudioProfileTemplate) async {
        errorMessage = nil
        applyPresetLocally(preset)
        latestPresetToApply = preset
        await flushPresetApplies()
    }

    /// Aggiorna EQ, SuperPhat e Bass Boost in UI (barre e nodi Home) prima della rete.
    private func applyPresetLocally(_ preset: AudioProfileTemplate) {
        userDivergedFromPreset = false
        selectedEQPresetID = preset.id
        withAnimation(.snappy(duration: 0.28)) {
            eqBands = preset.resolvedEQ(usingDevice: eqBands.isEmpty ? preset.eq : eqBands)
            stereoLevel = Double(preset.enhancements.stereoLevel)
            bassLevel = Double(preset.enhancements.bassLevel)
        }
    }

    private func flushPresetApplies() async {
        guard !isFlushingPresets else { return }
        isFlushingPresets = true
        defer { isFlushingPresets = false }
        while let preset = latestPresetToApply {
            latestPresetToApply = nil
            isSaving = true
            defer { isSaving = false }
            do {
                try await service.applyAudioPreset(preset)
                applyPresetLocally(preset)
                statusMessage = "Preset \(preset.name) applicato."
            } catch {
                errorMessage = DigiRadioFriendlyError.message(for: error)
            }
        }
    }

    func setMasterVolumeFromHome(_ level: Double) async {
        masterLevel = max(0, min(100, level.rounded()))
        pushMasterLevelToChannels()
        await applyNow()
    }

    func cycleBass() async {
        notePresetEdited()
        bassLevel = Self.nextEffectStep(bassLevel)
        await applyEnhancements()
    }

    func cycleStereo() async {
        notePresetEdited()
        stereoLevel = Self.nextEffectStep(stereoLevel)
        await applyEnhancements()
    }

    func toggleVoiceClean() async {
        notePresetEdited()
        voiceClean.toggle()
        persistVoiceClarifier()
        withAnimation(.snappy) {
            eqBands = voiceClean
                ? AudioEnhancementsDesign.applyingVoice(eqBands)
                : AudioEnhancementsDesign.removingVoice(eqBands)
        }
        await applyNow()
    }

    func persistVoiceClarifier() {
        UserDefaults.standard.set(voiceClean, forKey: Self.voiceClarifierDefaultsKey)
    }

    private static let voiceClarifierDefaultsKey = "igiRadio.voiceClarifier.enabled"

    private static func nextEffectStep(_ current: Double) -> Double {
        let steps: [Double] = [0, 40, 70, 100]
        return steps.first(where: { $0 > current + 1 }) ?? 0
    }
}
