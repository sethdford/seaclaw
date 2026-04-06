import Combine
import Foundation
import HumanClient
import HumanOnDevice
import HumanProtocol

/// Bridges `HumanConnection` to the macOS app’s completion-based API and `ObservableObject` updates.
final class GatewayClient: ObservableObject {
    @Published private(set) var isConnected = false

    /// Primary observer (e.g. status / alerts).
    var eventHandler: ((String, [String: Any]?) -> Void)?

    /// Optional second observer for chat streaming (does not replace `eventHandler`).
    var chatEventHandler: ((String, [String: Any]?) -> Void)?

    private let connection: HumanConnection
    private let chatEvents = PassthroughSubject<(String, [String: Any]?), Never>()

    /// On-device Apple Intelligence adapter for offline/fallback inference.
    let onDevice = OnDeviceChatAdapter()

    /// Whether on-device Apple Intelligence inference is available on this device.
    var onDeviceAvailable: Bool { onDevice.isAvailable }

    /// Stream of gateway `event` frames for SwiftUI `onReceive`.
    var chatEventsPublisher: AnyPublisher<(String, [String: Any]?), Never> {
        chatEvents.eraseToAnyPublisher()
    }

    init(url: String = "ws://localhost:3000/ws") {
        connection = HumanConnection(urlString: url)
        connection.stateHandler = { [weak self] state in
            DispatchQueue.main.async {
                self?.isConnected = (state == .connected)
            }
        }
        connection.eventHandler = { [weak self] event, payload in
            let plain: [String: Any]? = payload.map { $0.mapValues { $0.value } }
            self?.eventHandler?(event, plain)
            self?.chatEventHandler?(event, plain)
            self?.chatEvents.send((event, plain))
        }
    }

    /// Connect only when needed (e.g. when Chat tab is selected).
    func connectIfNeeded() {
        guard !isConnected else { return }
        connection.connect()
    }

    func connect() {
        connection.connect()
    }

    func disconnect() {
        connection.disconnect()
    }

    /// Send a message using on-device Apple Intelligence when the gateway is disconnected.
    func chatOnDevice(message: String, systemPrompt: String? = nil) async -> String? {
        guard onDeviceAvailable else { return nil }
        guard onDevice.fitsInContext(message) else { return nil }

        var messages: [OnDeviceChatAdapter.ChatMessage] = []
        if let systemPrompt {
            messages.append(.init(role: .system, content: systemPrompt))
        }
        messages.append(.init(role: .user, content: message))

        do {
            let response = try await onDevice.chat(messages: messages)
            return response.content
        } catch {
            return nil
        }
    }

    func request(method: String, params: [String: Any] = [:],
                 completion: @escaping (Result<[String: Any], Error>) -> Void) {
        let codableParams: [String: AnyCodable]? = params.isEmpty ? nil : params.mapValues { AnyCodable($0) }
        Task {
            do {
                let resp = try await connection.request(method: method, params: codableParams)
                await MainActor.run {
                    if resp.ok {
                        let dict = resp.payload?.mapValues { $0.value } as? [String: Any] ?? [:]
                        completion(.success(dict))
                    } else {
                        let err = (resp.payload?["error"]?.value as? String) ?? "Request failed"
                        completion(.failure(NSError(domain: "GatewayClient", code: -2,
                                                    userInfo: [NSLocalizedDescriptionKey: err])))
                    }
                }
            } catch {
                await MainActor.run {
                    completion(.failure(error))
                }
            }
        }
    }
}
