import Foundation
import HumanOnDevice

/// OpenAI-compatible HTTP router backed by Apple's on-device FoundationModels.
/// Implements /v1/models and /v1/chat/completions — zero external dependencies.
@available(macOS 26.0, iOS 26.0, *)
public final class OnDeviceRouter: HTTPRouter, @unchecked Sendable {
    private let provider = OnDeviceProvider()
    private let requiredToken: String

    public init(requiredToken: String) {
        self.requiredToken = requiredToken
    }

    public func handle(_ request: HTTPRequest) async -> HTTPResponse {
        if request.method == "OPTIONS" {
            return corsPreflightResponse()
        }

        let auth = request.headers["authorization"] ?? ""
        guard auth == "Bearer \(requiredToken)" else {
            return .error("Unauthorized", status: 401)
        }

        let path = request.path.split(separator: "?").first.map(String.init) ?? request.path

        switch (request.method, path) {
        case ("GET", "/v1/models"):
            return modelsResponse()
        case ("POST", "/v1/chat/completions"):
            return await chatCompletions(request)
        case ("GET", "/health"), ("GET", "/"):
            return healthResponse()
        default:
            return .notFound()
        }
    }

    // MARK: - /v1/models

    private func modelsResponse() -> HTTPResponse {
        .json([
            "object": "list",
            "data": [[
                "id": OnDeviceProvider.modelName,
                "object": "model",
                "created": Int(Date().timeIntervalSince1970),
                "owned_by": "apple",
                "context_window": OnDeviceProvider.contextWindow,
            ] as [String: Any]]
        ])
    }

    // MARK: - /v1/chat/completions

    private func chatCompletions(_ request: HTTPRequest) async -> HTTPResponse {
        guard let json = request.json else {
            return .error("Invalid JSON body", status: 400)
        }

        guard provider.isAvailable else {
            return .error("Apple Intelligence is not available on this device", status: 503)
        }

        guard let messages = json["messages"] as? [[String: Any]] else {
            return .error("Missing 'messages' array", status: 400)
        }

        let systemPrompt = messages.first(where: { ($0["role"] as? String) == "system" })?["content"] as? String
        let userMessage = messages.last(where: { ($0["role"] as? String) == "user" })?["content"] as? String ?? ""

        let stream = (json["stream"] as? Bool) ?? false
        let requestId = "chatcmpl-\(UUID().uuidString.prefix(12))"

        if stream {
            return await streamingCompletion(
                prompt: userMessage,
                systemPrompt: systemPrompt,
                requestId: requestId
            )
        } else {
            return await batchCompletion(
                prompt: userMessage,
                systemPrompt: systemPrompt,
                requestId: requestId
            )
        }
    }

    private func batchCompletion(prompt: String, systemPrompt: String?, requestId: String) async -> HTTPResponse {
        do {
            let content = try await provider.generate(prompt: prompt, systemPrompt: systemPrompt)
            let promptTokens = (prompt.count + (systemPrompt?.count ?? 0)) / 4
            let completionTokens = content.count / 4

            return .json([
                "id": requestId,
                "object": "chat.completion",
                "created": Int(Date().timeIntervalSince1970),
                "model": OnDeviceProvider.modelName,
                "choices": [[
                    "index": 0,
                    "message": [
                        "role": "assistant",
                        "content": content,
                    ],
                    "finish_reason": "stop",
                ] as [String: Any]],
                "usage": [
                    "prompt_tokens": promptTokens,
                    "completion_tokens": completionTokens,
                    "total_tokens": promptTokens + completionTokens,
                ] as [String: Any],
            ] as [String: Any])
        } catch {
            return mapError(error)
        }
    }

    private func streamingCompletion(prompt: String, systemPrompt: String?, requestId: String) async -> HTTPResponse {
        let (stream, continuation) = AsyncStream<Data>.makeStream()

        Task {
            do {
                _ = try await provider.stream(prompt: prompt, systemPrompt: systemPrompt) { chunk in
                    let delta: [String: Any] = [
                        "id": requestId,
                        "object": "chat.completion.chunk",
                        "created": Int(Date().timeIntervalSince1970),
                        "model": OnDeviceProvider.modelName,
                        "choices": [[
                            "index": 0,
                            "delta": ["content": chunk],
                            "finish_reason": NSNull(),
                        ] as [String: Any]],
                    ]
                    if let jsonData = try? JSONSerialization.data(withJSONObject: delta),
                       let jsonStr = String(data: jsonData, encoding: .utf8) {
                        let sseChunk = "data: \(jsonStr)\n\n"
                        let chunkLen = String(sseChunk.utf8.count, radix: 16)
                        let framed = Data("\(chunkLen)\r\n\(sseChunk)\r\n".utf8)
                        continuation.yield(framed)
                    }
                }

                // Final chunk with finish_reason
                let finalDelta: [String: Any] = [
                    "id": requestId,
                    "object": "chat.completion.chunk",
                    "created": Int(Date().timeIntervalSince1970),
                    "model": OnDeviceProvider.modelName,
                    "choices": [[
                        "index": 0,
                        "delta": [String: String](),
                        "finish_reason": "stop",
                    ] as [String: Any]],
                ]
                if let jsonData = try? JSONSerialization.data(withJSONObject: finalDelta),
                   let jsonStr = String(data: jsonData, encoding: .utf8) {
                    let sseChunk = "data: \(jsonStr)\n\n"
                    let chunkLen = String(sseChunk.utf8.count, radix: 16)
                    let framed = Data("\(chunkLen)\r\n\(sseChunk)\r\n".utf8)
                    continuation.yield(framed)
                }

                // [DONE] sentinel
                let done = "data: [DONE]\n\n"
                let doneLen = String(done.utf8.count, radix: 16)
                let doneFinal = "\(doneLen)\r\n\(done)\r\n0\r\n\r\n"
                continuation.yield(Data(doneFinal.utf8))
                continuation.finish()
            } catch {
                let escaped = Self.jsonEscapedForSSE(String(describing: error))
                let errMsg = "data: {\"error\":{\"message\":\"\(escaped)\"}}\n\n"
                let errLen = String(errMsg.utf8.count, radix: 16)
                continuation.yield(Data("\(errLen)\r\n\(errMsg)\r\n0\r\n\r\n".utf8))
                continuation.finish()
            }
        }

        return .sseStream(chunks: stream)
    }

    // MARK: - Health

    private func healthResponse() -> HTTPResponse {
        .json([
            "status": provider.isAvailable ? "ok" : "unavailable",
            "model": OnDeviceProvider.modelName,
            "context_window": OnDeviceProvider.contextWindow,
        ] as [String: Any])
    }

    // MARK: - CORS

    private func corsPreflightResponse() -> HTTPResponse {
        HTTPResponse(
            statusCode: 204,
            statusText: "No Content",
            contentType: "text/plain",
            body: Data(),
            streamChunks: nil,
            accessControlAllowMethods: "GET, POST, OPTIONS",
            accessControlAllowHeaders: "Authorization, Content-Type"
        )
    }

    private static func jsonEscapedForSSE(_ s: String) -> String {
        s
            .replacingOccurrences(of: "\\", with: "\\\\")
            .replacingOccurrences(of: "\"", with: "\\\"")
            .replacingOccurrences(of: "\n", with: "\\n")
            .replacingOccurrences(of: "\r", with: "\\r")
    }

    // MARK: - Error Mapping

    private func mapError(_ error: Error) -> HTTPResponse {
        if let onDeviceError = error as? OnDeviceProvider.OnDeviceError {
            switch onDeviceError {
            case .modelUnavailable:
                return .error("Apple Intelligence is not available on this device", status: 503)
            case .guardrailBlocked:
                return .error("Response blocked by Apple Intelligence safety guardrails", status: 400)
            case .contextOverflow:
                return .error("Input exceeds on-device context window (\(OnDeviceProvider.contextWindow) tokens)", status: 400)
            case .generationFailed(let msg):
                return .error("Generation failed: \(msg)", status: 500)
            }
        }
        return .error("Internal error: \(error)", status: 500)
    }
}
