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

    private var autoFindTask: Task<Void, Never>?
    private var reconnectLoop: Task<Void, Never>?

    /// Connects to the radio without user action: the saved host first
    /// (quick health probe), then a silent network search (mDNS resolved
    /// to IP, LAN sweep, SoftAP) — the radio's DHCP address can change.
    @MainActor
    func autoConnectIfNeeded() async {
        guard !useMockDevice, !digiRadio.state.connection.isConnected else { return }
        if let task = autoFindTask {
            await task.value
            return
        }
        let task = Task { @MainActor in await self.findAndConnect() }
        autoFindTask = task
        await task.value
        autoFindTask = nil
    }

    /// While the app is in the foreground, retry every 30 s when the radio
    /// is not connected (it may have been off or changed address).
    @MainActor
    func startAutoReconnect() {
        guard reconnectLoop == nil else { return }
        reconnectLoop = Task { @MainActor [weak self] in
            while !Task.isCancelled {
                guard let self else { return }
                if self.digiRadio.state.connection.isConnected {
                    // Notice a radio that went away (off, new IP) so the
                    // next round searches for it again.
                    _ = await self.digiRadio.verifyReachability()
                }
                await self.autoConnectIfNeeded()
                try? await Task.sleep(for: .seconds(30))
            }
        }
    }

    @MainActor
    func stopAutoReconnect() {
        reconnectLoop?.cancel()
        reconnectLoop = nil
    }

    @MainActor
    private func findAndConnect() async {
        if let saved = ConnectionPreferences.lastHost {
            for candidate in DeviceIdentity.connectionCandidates(from: saved) {
                if await DigiRadioHealthProbe.probe(host: candidate) != nil,
                   (try? await digiRadio.connect(host: candidate)) != nil {
                    return
                }
            }
        }
        guard let found = await DigiRadioDiscoveryService().firstReachable() else { return }
        try? await digiRadio.connect(host: found.host)
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
