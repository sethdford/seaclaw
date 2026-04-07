import Foundation

/// Adapts OnDeviceProvider to a simple chat interface compatible with the native app's
/// conversation flow. Uses on-device inference when available, with graceful fallback
/// indication when the model is unavailable.
///
/// Uses runtime `#available` checks internally so consumers don't need `@available`.
public final class OnDeviceChatAdapter: @unchecked Sendable {
    private var _provider: Any?

    public struct ChatMessage: Sendable {
        public let role: Role
        public let content: String

        public enum Role: String, Sendable {
            case system
            case user
            case assistant
        }

        public init(role: Role, content: String) {
            self.role = role
            self.content = content
        }
    }

    public struct ChatResponse: Sendable {
        public let content: String
        public let isOnDevice: Bool
    }

    public init() {
        #if canImport(FoundationModels)
        if #available(macOS 26.0, iOS 26.0, *) {
            _provider = OnDeviceProvider()
        }
        #endif
    }

    public var isAvailable: Bool {
        #if canImport(FoundationModels)
        if #available(macOS 26.0, iOS 26.0, *) {
            return (_provider as? OnDeviceProvider)?.isAvailable ?? false
        }
        #endif
        return false
    }

    public func chat(messages: [ChatMessage]) async throws -> ChatResponse {
        #if canImport(FoundationModels)
        if #available(macOS 26.0, iOS 26.0, *) {
            guard let provider = _provider as? OnDeviceProvider else {
                throw CocoaError(.featureUnsupported)
            }
            let systemPrompt = messages.first(where: { $0.role == .system })?.content
            let userMessage = messages.last(where: { $0.role == .user })?.content ?? ""
            let response = try await provider.generate(prompt: userMessage, systemPrompt: systemPrompt)
            return ChatResponse(content: response, isOnDevice: true)
        }
        #endif
        throw CocoaError(.featureUnsupported)
    }

    public func streamChat(
        messages: [ChatMessage],
        onChunk: @Sendable @escaping (String) -> Void
    ) async throws -> ChatResponse {
        #if canImport(FoundationModels)
        if #available(macOS 26.0, iOS 26.0, *) {
            guard let provider = _provider as? OnDeviceProvider else {
                throw CocoaError(.featureUnsupported)
            }
            let systemPrompt = messages.first(where: { $0.role == .system })?.content
            let userMessage = messages.last(where: { $0.role == .user })?.content ?? ""
            let response = try await provider.stream(prompt: userMessage, systemPrompt: systemPrompt, onChunk: onChunk)
            return ChatResponse(content: response, isOnDevice: true)
        }
        #endif
        throw CocoaError(.featureUnsupported)
    }

    public func fitsInContext(_ text: String) -> Bool {
        text.count / 4 < 2048
    }
}
