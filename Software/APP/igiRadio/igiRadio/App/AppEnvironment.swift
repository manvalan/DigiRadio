import Foundation
import Observation

/// Dependency container for igiRadio.
@Observable
final class AppEnvironment {
    var useMockDevice: Bool
    private(set) var digiRadio: any DigiRadioService

    var state: DigiRadioState {
        digiRadio.state
    }

    init(useMockDevice: Bool = {
        #if DEBUG
        true
        #else
        false
        #endif
    }()) {
        self.useMockDevice = useMockDevice
        self.digiRadio = useMockDevice ? MockDigiRadioService() : RealDigiRadioService()
    }

    func setUseMock(_ enabled: Bool) {
        guard useMockDevice != enabled else { return }
        useMockDevice = enabled
        digiRadio = enabled ? MockDigiRadioService() : RealDigiRadioService()
    }
}
