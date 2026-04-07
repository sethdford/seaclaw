import Foundation

/// Route HTTP requests to handlers.
@available(macOS 14.0, iOS 17.0, *)
public protocol HTTPRouter: Sendable {
    func handle(_ request: HTTPRequest) async -> HTTPResponse
}
