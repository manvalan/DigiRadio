import CoreBluetooth
import Foundation
import Observation
import OSLog

/// Discovers DigiRadio BLE provisioning endpoints (ESP-IDF wifi_provisioning / scheme_ble).
/// Full credential exchange uses Espressif protocomm Security1 (PoP = device serial).
/// This service performs discovery only; credential provisioning UI is documented in BLE_PROTOCOL.md.
@Observable
final class BLEProvisioningService: NSObject {
    enum Phase: Equatable {
        case idle
        case scanning
        case found(name: String, identifier: UUID)
        case unsupported
        case error(String)
    }

    private(set) var phase: Phase = .idle
    private(set) var discoveredDevices: [(name: String, id: UUID, rssi: Int)] = []

    private var central: CBCentralManager?
    private let logger = Logger(subsystem: "com.digiradio.igiRadio", category: "BLE")

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: .main)
    }

    func startScan() {
        guard let central else { return }
        discoveredDevices = []
        phase = .scanning
        guard central.state == .poweredOn else {
            phase = .error("Bluetooth non disponibile")
            return
        }
        central.scanForPeripherals(withServices: nil, options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
        logger.info("BLE scan started")
    }

    func stopScan() {
        central?.stopScan()
        if case .scanning = phase {
            phase = discoveredDevices.isEmpty ? .idle : .found(name: discoveredDevices[0].name, identifier: discoveredDevices[0].id)
        }
    }
}

extension BLEProvisioningService: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state != .poweredOn, case .scanning = phase {
            phase = .error("Bluetooth spento o non autorizzato")
            central.stopScan()
        }
    }

    func centralManager(
        _ central: CBCentralManager,
        didDiscover peripheral: CBPeripheral,
        advertisementData: [String: Any],
        rssi RSSI: NSNumber
    ) {
        let name = peripheral.name ?? advertisementData[CBAdvertisementDataLocalNameKey] as? String ?? ""
        guard name.hasPrefix("DigiRadio") else { return }
        let entry = (name: name, id: peripheral.identifier, rssi: RSSI.intValue)
        if !discoveredDevices.contains(where: { $0.id == entry.id }) {
            discoveredDevices.append(entry)
            logger.debug("Found \(name, privacy: .public) RSSI \(RSSI.intValue)")
        }
        phase = .found(name: name, identifier: peripheral.identifier)
    }
}
