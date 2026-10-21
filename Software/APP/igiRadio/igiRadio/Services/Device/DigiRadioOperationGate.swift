import Foundation

/// Serializes slow device operations (audio profile safeload, enhancements) to avoid overlapping HTTP calls.
actor DigiRadioOperationGate {
    func run<T>(_ operation: () async throws -> T) async rethrows -> T {
        try await operation()
    }
}
