import Foundation

/// High-level API: start an OpenAI-compatible on-device inference server.
/// Uses Apple's FoundationModels framework via Network.framework. Zero dependencies.
///
/// Usage:
///   let server = OnDeviceServer(port: 11435)
///   try server.start()
///   // Server is now listening on http://127.0.0.1:11435/v1
///   server.stop()
@available(macOS 14.0, iOS 17.0, *)
public final class OnDeviceServer: Sendable {
    private let httpServer: HTTPServer
    public let port: UInt16

    /// Default port (11435) avoids conflict with Ollama's 11434.
    public init(port: UInt16 = 11435) {
        self.port = port
        let router = OnDeviceRouter()
        self.httpServer = HTTPServer(port: port, router: router)
    }

    /// Start listening. Non-blocking — returns immediately.
    public func start() throws {
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
