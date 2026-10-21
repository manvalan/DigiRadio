import Foundation

enum DigiRadioHealthProbe {
  private static let fastSession: URLSession = {
    let config = URLSessionConfiguration.ephemeral
    config.timeoutIntervalForRequest = 1.5
    config.timeoutIntervalForResource = 2
    config.waitsForConnectivity = false
    return URLSession(configuration: config)
  }()

  struct Result: Equatable {
    var host: String
    var firmware: String
    var serialNumber: String
  }

  static func probe(host: String) async -> Result? {
    let client = HTTPDigiRadioClient(session: fastSession)
    do {
      try client.setHost(host)
      let health = try await client.fetchHealth()
      guard health.status == "ok", !health.fw.isEmpty else { return nil }
      return Result(host: HostNormalizer.httpURL(from: host), firmware: health.fw, serialNumber: health.serialNumber)
    } catch {
      return nil
    }
  }
}
