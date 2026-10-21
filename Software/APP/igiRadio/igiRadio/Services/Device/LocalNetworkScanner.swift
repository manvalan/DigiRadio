import Foundation
import Darwin

enum LocalNetworkScanner {
  /// A contiguous IPv4 host range derived from the interface's actual
  /// address + netmask — NOT assumed to be a /24 (see `hosts(in:)`).
  struct Subnet {
    var prefix: String
    var networkAddress: UInt32
    var hostCount: UInt32
  }

  /// Returns the Wi‑Fi IPv4 subnet, sized from the interface's real netmask.
  ///
  /// Networks narrower than /24 (e.g. /22, common on larger home/office
  /// routers) span multiple third-octet blocks; assuming /24 silently
  /// drops hosts outside the phone's own block. Cap the scan at /20 (4094
  /// hosts) so an unusually flat network doesn't turn this into an
  /// unbounded sweep.
  static func currentWiFiSubnet() -> Subnet? {
    var candidates: [(ip: UInt32, mask: UInt32, display: String)] = []
    var ifaddr: UnsafeMutablePointer<ifaddrs>?
    guard getifaddrs(&ifaddr) == 0, let first = ifaddr else { return nil }
    defer { freeifaddrs(ifaddr) }

    var ptr: UnsafeMutablePointer<ifaddrs>? = first
    while let interface = ptr {
      let flags = Int32(interface.pointee.ifa_flags)
      let isUp = (flags & IFF_UP) != 0
      let isLoopback = (flags & IFF_LOOPBACK) != 0
      let name = String(cString: interface.pointee.ifa_name)

      if isUp, !isLoopback, name.hasPrefix("en"),
         let addr = interface.pointee.ifa_addr,
         addr.pointee.sa_family == UInt8(AF_INET),
         let maskAddr = interface.pointee.ifa_netmask,
         maskAddr.pointee.sa_family == UInt8(AF_INET) {
        var host = [CChar](repeating: 0, count: Int(NI_MAXHOST))
        let len = socklen_t(addr.pointee.sa_len)
        if getnameinfo(addr, len, &host, socklen_t(host.count), nil, 0, NI_NUMERICHOST) == 0 {
          let ip = String(cString: host)
          if !ip.hasPrefix("127."), !ip.hasPrefix("169.254."),
             let ipValue = ipv4Value(ip) {
            let maskValue = maskAddr.withMemoryRebound(to: sockaddr_in.self, capacity: 1) {
              UInt32(bigEndian: $0.pointee.sin_addr.s_addr)
            }
            candidates.append((ipValue, maskValue, ip))
          }
        }
      }
      ptr = interface.pointee.ifa_next
    }

    guard let (ip, mask, display) = candidates.first else { return nil }
    // Never scan a wider range than a /20 (4094 hosts), even if the
    // reported mask is flatter than that.
    let cappedMask = mask | 0xFFF0_0000
    let networkAddress = ip & cappedMask
    let hostCount = ~cappedMask
    let parts = display.split(separator: ".").compactMap { Int($0) }
    let prefix = parts.count == 4 ? "\(parts[0]).\(parts[1]).\(parts[2])" : display
    return Subnet(prefix: prefix, networkAddress: networkAddress, hostCount: hostCount)
  }

  /// Every usable host address in `subnet` (network + broadcast excluded).
  static func hosts(in subnet: Subnet) -> [String] {
    guard subnet.hostCount > 1 else { return [] }
    return (1 ..< subnet.hostCount).map { offset in
      "http://\(ipv4String(subnet.networkAddress + offset))"
    }
  }

  static let setupSoftAPHost = "http://192.168.4.1"

  private static func ipv4Value(_ dotted: String) -> UInt32? {
    let parts = dotted.split(separator: ".").compactMap { UInt32($0) }
    guard parts.count == 4, parts.allSatisfy({ $0 <= 255 }) else { return nil }
    return (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3]
  }

  private static func ipv4String(_ value: UInt32) -> String {
    "\((value >> 24) & 0xFF).\((value >> 16) & 0xFF).\((value >> 8) & 0xFF).\(value & 0xFF)"
  }
}
