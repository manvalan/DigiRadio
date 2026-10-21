import Foundation
import Network
import Observation
import OSLog

/// Finds DigiRadio via BLE name mapping, mDNS browse, and LAN /api/health scan.
@Observable
final class DigiRadioDiscoveryService {
  struct DiscoveredDevice: Identifiable, Equatable {
    var id: String { host }
    var name: String
    var host: String
    var isReachable: Bool
    var firmware: String?
  }

  private(set) var devices: [DiscoveredDevice] = []
  private(set) var isSearching = false
  private(set) var statusMessage: String?

  private let ble = BLEProvisioningService()
  private let logger = Logger(subsystem: "com.digiradio.igiRadio", category: "Discovery")
  private var bonjourBrowser: NWBrowser?
  private var lanScanTask: Task<Void, Never>?

  func start() {
    guard !isSearching else { return }
    isSearching = true
    devices = []
    statusMessage = String(localized: "Avvio ricerca…")
    ble.startScan()
    startBonjourBrowse()
    probe(host: LocalNetworkScanner.setupSoftAPHost, label: "igiRadio Setup (SoftAP)")
    lanScanTask = Task { await scanLAN() }
    logger.info("Discovery started")
  }

  func stop() {
    isSearching = false
    statusMessage = nil
    ble.stopScan()
    bonjourBrowser?.cancel()
    bonjourBrowser = nil
    lanScanTask?.cancel()
    lanScanTask = nil
  }

  func refreshFromBLE() {
    for entry in ble.discoveredDevices {
      guard let host = Self.httpHost(fromBLEName: entry.name) else { continue }
      upsertDevice(DiscoveredDevice(name: entry.name, host: host, isReachable: false, firmware: nil))
      probe(host: host, label: entry.name)
    }
  }

  func addManualHost(_ raw: String) {
    let normalized = HostNormalizer.httpURL(from: raw)
    guard !normalized.isEmpty, let host = URL(string: normalized)?.host else { return }
    upsertDevice(DiscoveredDevice(name: host, host: normalized, isReachable: false, firmware: nil))
    probe(host: normalized, label: host)
  }

  static func httpHost(fromBLEName name: String) -> String? {
    DeviceIdentity.httpHost(fromAdvertisedName: name)
  }

  private func upsertDevice(_ candidate: DiscoveredDevice) {
    if let index = devices.firstIndex(where: { $0.host == candidate.host }) {
      if candidate.isReachable { devices[index] = candidate }
      return
    }
    devices.append(candidate)
  }

  private func markReachable(host: String, name: String, firmware: String) {
    let normalized = HostNormalizer.httpURL(from: host)
    let device = DiscoveredDevice(name: name, host: normalized, isReachable: true, firmware: firmware)
    if let index = devices.firstIndex(where: { $0.host == normalized }) {
      devices[index] = device
    } else {
      devices.append(device)
    }
  }

  private func startBonjourBrowse() {
    let browser = NWBrowser(for: .bonjour(type: "_http._tcp", domain: nil), using: .tcp)
    browser.browseResultsChangedHandler = { [weak self] results, _ in
      guard let self else { return }
      for result in results {
        guard case let .service(name, _, _, _) = result.endpoint else { continue }
        let lowered = name.lowercased()
        guard DeviceIdentity.matchesBonjourServiceName(name) else { continue }
        let host = "http://\(lowered.hasSuffix(".local") ? lowered : "\(lowered).local")"
        self.upsertDevice(DiscoveredDevice(name: name, host: host, isReachable: false, firmware: nil))
        self.probe(host: host, label: name)
      }
    }
    browser.start(queue: .main)
    bonjourBrowser = browser
  }

  private func probe(host: String, label: String) {
    Task {
      if let result = await DigiRadioHealthProbe.probe(host: host) {
        await MainActor.run {
          let display = label.isEmpty ? HostNormalizer.displayHost(from: result.host) : label
          markReachable(host: result.host, name: display, firmware: result.firmware)
        }
      }
    }
  }

  private func scanLAN() async {
  guard let subnet = LocalNetworkScanner.currentWiFiSubnet() else {
      await MainActor.run {
        statusMessage = String(localized: "Wi‑Fi non rilevato — inserisci l'IP manualmente.")
      }
      return
    }

    let hosts = LocalNetworkScanner.hosts(in: subnet)
    await MainActor.run {
      statusMessage = String(localized: "Scansione \(subnet.prefix).x (0/\(hosts.count))…")
    }

    var found = 0
    let chunkSize = 24
    for chunkStart in stride(from: 0, to: hosts.count, by: chunkSize) {
      if Task.isCancelled { break }
      let chunk = Array(hosts[chunkStart ..< min(chunkStart + chunkSize, hosts.count)])

      await withTaskGroup(of: DigiRadioHealthProbe.Result?.self) { group in
        for host in chunk {
          group.addTask { await DigiRadioHealthProbe.probe(host: host) }
        }
        for await result in group {
          guard let result else { continue }
          found += 1
          await MainActor.run {
            let ip = HostNormalizer.displayHost(from: result.host)
            let name = result.serialNumber.isEmpty ? "igiRadio @ \(ip)" : "igiRadio \(result.serialNumber)"
            markReachable(host: result.host, name: name, firmware: result.firmware)
          }
        }
      }

      await MainActor.run {
        let scanned = min(chunkStart + chunkSize, hosts.count)
        statusMessage = String(localized: "Scansione \(subnet.prefix).x (\(scanned)/\(hosts.count)) — trovati \(found)")
      }
    }

    await MainActor.run {
      if found == 0 {
        statusMessage = String(localized: "Nessun igiRadio su \(subnet.prefix).x. Prova IP manuale o rete SoftAP 192.168.4.1")
      } else {
        statusMessage = String(localized: "Trovati \(found) dispositivo/i. Tocca per connettere.")
      }
    }
  }
}
