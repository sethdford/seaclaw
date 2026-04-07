import Foundation
import Security

/// High-level API: start an OpenAI-compatible on-device inference server.
/// Uses Apple's FoundationModels framework via Network.framework. Zero dependencies.
///
/// Usage:
///   let server = OnDeviceServer(port: 11435)
///   try server.start()
///   // Send `Authorization: Bearer <bearerToken>` on every request (token printed to stdout on start).
///   // Server is now listening on http://127.0.0.1:11435/v1
///   server.stop()
@available(macOS 26.0, iOS 26.0, *)
public final class OnDeviceServer: Sendable {
    private let httpServer: HTTPServer
    public let port: UInt16
    /// Random secret for `Authorization: Bearer …` on all HTTP endpoints (except CORS preflight).
    public let bearerToken: String

    /// Default port (11435) avoids conflict with Ollama's 11434.
    public init(port: UInt16 = 11435) {
        self.port = port
        var bytes = [UInt8](repeating: 0, count: 32)
        let randStatus = SecRandomCopyBytes(kSecRandomDefault, bytes.count, &bytes)
        precondition(randStatus == errSecSuccess)
        let token = Data(bytes).base64EncodedString()
        self.bearerToken = token
        let router = OnDeviceRouter(requiredToken: token)
        self.httpServer = HTTPServer(port: port, router: router)
    }

    /// Start listening. Non-blocking — returns immediately.
    public func start() throws {
        print("[human-ondevice] Authorization: Bearer \(bearerToken)")
        try httpServer.start()
    }

    /// Stop the server and release the port.
    public func stop() {
        httpServer.stop()
    }

    /// Base URL for the OpenAI-compatible API.
    public var baseURL: String {
        "http://127.0.0.1:\(port)/v1"
    }
}
