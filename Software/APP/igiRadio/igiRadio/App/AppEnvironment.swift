import Foundation
import Observation

/// Dependency container for igiRadio.
@Observable
final class AppEnvironment {
    var useMockDevice: Bool
    private(set) var digiRadio: any DigiRadioService
    let bassBoost: BassBoostViewModel
    let stationScan: StationScanViewModel
    let adauStudio: AdauStudioViewModel
    var incomingShare: IncomingShare?
    var presentStreaming = false
    var shareBanner: String?
    var phoneCaptureActive = false

    var state: DigiRadioState {
        digiRadio.state
    }

    init(useMockDevice: Bool = {
        #if DEBUG
        UserDefaults.standard.object(forKey: "igiRadio.useMock") as? Bool ?? false
        #else
        false
        #endif
    }()) {
        self.useMockDevice = useMockDevice
        if useMockDevice {
            let mock = MockDigiRadioService()
            self.digiRadio = mock
            self.bassBoost = BassBoostViewModel(service: mock)
            self.stationScan = StationScanViewModel(service: mock)
            self.adauStudio = AdauStudioViewModel(service: mock)
        } else {
            let real = RealDigiRadioService()
            self.digiRadio = real
            self.bassBoost = BassBoostViewModel(service: real)
            self.stationScan = StationScanViewModel(service: real)
            self.adauStudio = AdauStudioViewModel(service: real)
        }
    }

    func setUseMock(_ enabled: Bool) {
        guard useMockDevice != enabled else { return }
        useMockDevice = enabled
        UserDefaults.standard.set(enabled, forKey: "igiRadio.useMock")
        if enabled {
            let mock = MockDigiRadioService()
            digiRadio = mock
            bassBoost.replaceService(mock)
            stationScan.replaceService(mock)
            adauStudio.replaceService(mock)
        } else {
            let real = RealDigiRadioService()
            digiRadio = real
            bassBoost.replaceService(real)
            stationScan.replaceService(real)
            adauStudio.replaceService(real)
        }
    }

    func autoConnectIfNeeded() async {
        guard !useMockDevice else { return }
        if let saved = ConnectionPreferences.lastHost {
            ConnectionPreferences.saveLastHost(saved)
        }
        guard !digiRadio.state.connection.isConnected else { return }
        guard let host = ConnectionPreferences.lastHost else { return }
        for candidate in DeviceIdentity.connectionCandidates(from: host) {
            do {
                try await digiRadio.connect(host: candidate)
                return
            } catch {
                continue
            }
        }
    }

    @MainActor
    func handleIncomingShare(_ share: IncomingShare) async {
        incomingShare = share
        await autoConnectIfNeeded()
        do {
            switch share {
            case let .otherApp(url):
                presentStreaming = false
                try await digiRadio.prepareForPhoneAudio()
                let host = url.host ?? "app"
                shareBanner = String(localized: "Radio su Stream (\(host)). Per sentire: webradio, file o «Ricevi audio».")
            case let .webStream(url):
                presentStreaming = true
                try await digiRadio.applyAudioSourceRouting(forStreaming: true)
                shareBanner = "Stream · \(url.host ?? "URL")"
            }
            adauStudio.syncFromDevice()
        } catch {
            shareBanner = String(localized: "Condivisione ricevuta, radio non raggiunta.")
        }
    }

    @MainActor
    func startPhoneCapture() async {
        guard digiRadio.state.connection.isConnected else {
            shareBanner = String(localized: "Radio non connessa.")
            return
        }
        let chunks = AppAudioCapture.shared.start()
        await PhonePCMStreamService.shared.start(
            connectionHost: digiRadio.state.connection.host,
            chunks: chunks
        )
        phoneCaptureActive = true
    }

    @MainActor
    func stopPhoneCapture() async {
        AppAudioCapture.shared.stop()
        await PhonePCMStreamService.shared.stop()
        phoneCaptureActive = false
        try? await digiRadio.endPhoneAudio()
        adauStudio.syncFromDevice()
    }
}
