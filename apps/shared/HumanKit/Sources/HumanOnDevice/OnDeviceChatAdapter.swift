import Foundation

/// Adapts OnDeviceProvider to a simple chat interface compatible with the native app's
/// conversation flow. Uses on-device inference when available, with graceful fallback
/// indication when the model is unavailable.
public final class OnDeviceChatAdapter: @unchecked Sendable {
    private let provider = OnDeviceProvider()

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

    public init() {}

    /// Whether on-device inference is available on this device.
    public var isAvailable: Bool { provider.isAvailable }

    /// Send a message and get a response using on-device inference.
    public func chat(messages: [ChatMessage]) async throws -> ChatResponse {
        let systemPrompt = messages.first(where: { $0.role == .system })?.content
        let userMessage = messages.last(where: { $0.role == .user })?.content ?? ""

        let response = try await provider.generate(
            prompt: userMessage,
            systemPrompt: systemPrompt
        )

        return ChatResponse(content: response, isOnDevice: true)
    }

    /// Send a message and stream the response using on-device inference.
    public func streamChat(
        messages: [ChatMessage],
        onChunk: @Sendable @escaping (String) -> Void
    ) async throws -> ChatResponse {
        let systemPrompt = messages.first(where: { $0.role == .system })?.content
        let userMessage = messages.last(where: { $0.role == .user })?.content ?? ""

        let response = try await provider.stream(
            prompt: userMessage,
            systemPrompt: systemPrompt,
            onChunk: onChunk
        )

        return ChatResponse(content: response, isOnDevice: true)
    }

    /// Estimate whether a message is within the on-device context window.
    /// Uses a rough 4 characters per token heuristic.
    public func fitsInContext(_ text: String) -> Bool {
        let estimatedTokens = text.count / 4
        return estimatedTokens < (OnDeviceProvider.contextWindow / 2)
    }
}
