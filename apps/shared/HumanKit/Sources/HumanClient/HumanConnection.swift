import Foundation
import HumanProtocol

/// WebSocket client for the Human gateway control protocol.
/// Uses URLSessionWebSocketTask with reconnection support.
public final class HumanConnection: @unchecked Sendable {
    public enum ConnectionState: Equatable {
        case disconnected
        case connecting
        case connected
    }

    public private(set) var state: ConnectionState = .disconnected {
        didSet { stateHandler?(state) }
    }

    /// Called when connection state changes.
    public var stateHandler: ((ConnectionState) -> Void)?

    /// Called when an event frame is received.
    public var eventHandler: ((String, [String: AnyCodable]?) -> Void)?

    private var task: URLSessionWebSocketTask?
    private var url: URL
    private let session = URLSession(configuration: .default)
    private let queue = DispatchQueue(label: "com.human.connection")
    private var reconnectWorkItem: DispatchWorkItem?
    private var pendingRequests: [String: CheckedContinuation<ControlResponse, Error>] = [:]
    private var requestTimeouts: [String: DispatchWorkItem] = [:]
    private let pendingLock = NSLock()
    private var reconnectAttempt: UInt = 0
    private var pingWorkItem: DispatchWorkItem?
    /// RPC wait timeout (matches web client default).
    public static var requestTimeoutSeconds: TimeInterval = 10
    private static let maxReconnectDelay: TimeInterval = 30
    private static let baseReconnectDelay: TimeInterval = 3

    public init(url: URL) {
        self.url = url
    }

    public convenience init(urlString: String) {
        let url = URL(string: urlString) ?? URL(string: "wss://localhost:3000/ws")!
        self.init(url: url)
    }

    /// Connect to the gateway WebSocket endpoint.
    public func connect() {
        queue.async { [weak self] in
            self?._connect()
        }
    }

    /// Disconnect and cancel reconnection.
    public func disconnect() {
        queue.async { [weak self] in
            self?.cancelPingSchedule()
            self?.reconnectWorkItem?.cancel()
            self?.reconnectWorkItem = nil
            self?.task?.cancel(with: .goingAway, reason: nil)
            self?.task = nil
            self?.failPendingRequests()
            self?.reconnectAttempt = 0
            self?.state = .disconnected
        }
    }

    /// Send a raw JSON message without waiting for response.
    public func sendMessage(_ json: String) {
        guard state == .connected else { return }
        task?.send(.string(json)) { _ in }
    }

    /// Register a push token with the gateway for push notifications.
    public func registerPushToken(token: String) {
        let id = "push-\(UUID().uuidString)"
        let params: [String: AnyCodable] = [
            "token": AnyCodable(token),
            "provider": AnyCodable("apns")
        ]
        let req = ControlRequest(id: id, method: Methods.pushRegister, params: params)
        guard let data = try? JSONEncoder().encode(req),
              let text = String(data: data, encoding: .utf8) else { return }
        sendMessage(text)
    }

    /// Unregister a push token from the gateway.
    public func unregisterPushToken(token: String) {
        let id = "pushu-\(UUID().uuidString)"
        let params: [String: AnyCodable] = ["token": AnyCodable(token)]
        let req = ControlRequest(id: id, method: Methods.pushUnregister, params: params)
        guard let data = try? JSONEncoder().encode(req),
              let text = String(data: data, encoding: .utf8) else { return }
        sendMessage(text)
    }

    /// Send an RPC request and wait for the response.
    public func request(method: String, params: [String: AnyCodable]? = nil) async throws -> ControlResponse {
        guard state == .connected else {
            throw HumanConnectionError.notConnected
        }
        let id = "req-\(UUID().uuidString)"
        let req = ControlRequest(id: id, method: method, params: params)
        let data = try JSONEncoder().encode(req)
        guard let text = String(data: data, encoding: .utf8) else {
            throw HumanConnectionError.encodingFailed
        }

        return try await withCheckedThrowingContinuation { cont in
            pendingLock.lock()
            pendingRequests[id] = cont
            let timeout = DispatchWorkItem { [weak self] in
                guard let self = self else { return }
                self.pendingLock.lock()
                self.requestTimeouts.removeValue(forKey: id)
                guard let c = self.pendingRequests.removeValue(forKey: id) else {
                    self.pendingLock.unlock()
                    return
                }
                self.pendingLock.unlock()
                c.resume(throwing: HumanConnectionError.timeout)
            }
            requestTimeouts[id] = timeout
            pendingLock.unlock()
            queue.asyncAfter(deadline: .now() + Self.requestTimeoutSeconds, execute: timeout)

            task?.send(.string(text)) { [weak self] error in
                if let error = error {
                    self?.pendingLock.lock()
                    self?.requestTimeouts[id]?.cancel()
                    self?.requestTimeouts.removeValue(forKey: id)
                    _ = self?.pendingRequests.removeValue(forKey: id)
                    self?.pendingLock.unlock()
                    cont.resume(throwing: error)
                }
            }
        }
    }

    // MARK: - Private

    private func _connect() {
        if state == .connecting, task != nil { return }
        state = .connecting
        task?.cancel(with: .goingAway, reason: nil)
        var request = URLRequest(url: url)
        request.setValue("websocket", forHTTPHeaderField: "Upgrade")
        request.setValue("Upgrade", forHTTPHeaderField: "Connection")
        task = session.webSocketTask(with: request)
        task?.resume()
        sendConnect()
        receive()
    }

    private func sendConnect() {
        let connect: [String: Any] = [
            "type": "req",
            "id": "connect-\(UUID().uuidString)",
            "method": "connect",
            "params": [:]
        ]
        guard let data = try? JSONSerialization.data(withJSONObject: connect),
              let text = String(data: data, encoding: .utf8) else { return }
        task?.send(.string(text)) { _ in }
    }

    private func receive() {
        task?.receive { [weak self] result in
            guard let self = self else { return }
            switch result {
            case .success(let message):
                switch message {
                case .string(let text):
                    self.handleMessage(text)
                case .data:
                    break
                @unknown default:
                    break
                }
                self.receive()
            case .failure:
                DispatchQueue.main.async { self.state = .disconnected }
                self.failPendingRequests()
                self.scheduleReconnect()
            }
        }
    }

    private func handleMessage(_ text: String) {
        guard let data = text.data(using: .utf8),
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let type = json["type"] as? String else { return }

        switch type {
        case "hello-ok":
            queue.async { [weak self] in
                self?.reconnectAttempt = 0
                self?.state = .connected
                self?.schedulePing()
            }
        case "res":
            if let payload = json["payload"] as? [String: Any],
               payload["type"] as? String == "hello-ok" {
                queue.async { [weak self] in
                    self?.reconnectAttempt = 0
                    self?.state = .connected
                    self?.schedulePing()
                }
            }
            if let id = json["id"] as? String {
                pendingLock.lock()
                requestTimeouts[id]?.cancel()
                requestTimeouts.removeValue(forKey: id)
                if let cont = pendingRequests.removeValue(forKey: id) {
                    pendingLock.unlock()
                    do {
                        let res = try parseResponse(from: json)
                        cont.resume(returning: res)
                    } catch {
                        cont.resume(throwing: error)
                    }
                } else {
                    pendingLock.unlock()
                }
            }
        case "event":
            if let event = json["event"] as? String {
                let payload = (json["payload"] as? [String: Any]).map { dict in
                    dict.mapValues { AnyCodable($0) }
                }
                eventHandler?(event, payload)
            }
        default:
            break
        }
    }

    private func parseResponse(from json: [String: Any]) throws -> ControlResponse {
        let data = try JSONSerialization.data(withJSONObject: json)
        return try JSONDecoder().decode(ControlResponse.self, from: data)
    }

    private func failPendingRequests() {
        pendingLock.lock()
        for (_, tw) in requestTimeouts { tw.cancel() }
        requestTimeouts.removeAll()
        let pending = pendingRequests
        pendingRequests.removeAll()
        pendingLock.unlock()
        for cont in pending.values {
            cont.resume(throwing: HumanConnectionError.disconnected)
        }
    }

    private func cancelPingSchedule() {
        pingWorkItem?.cancel()
        pingWorkItem = nil
    }

    private func schedulePing() {
        cancelPingSchedule()
        let work = DispatchWorkItem { [weak self] in
            guard let self = self else { return }
            if self.state == .connected {
                self.task?.sendPing { _ in }
            }
            self.schedulePing()
        }
        pingWorkItem = work
        queue.asyncAfter(deadline: .now() + 25, execute: work)
    }

    private func scheduleReconnect() {
        reconnectWorkItem?.cancel()
        reconnectAttempt += 1
        let exp = min(Self.maxReconnectDelay, Self.baseReconnectDelay * pow(2.0, Double(min(reconnectAttempt, 4))))
        let jitter = Double.random(in: 0...0.5)
        let delay = exp + jitter
        let work = DispatchWorkItem { [weak self] in
            self?._connect()
        }
        reconnectWorkItem = work
        queue.asyncAfter(deadline: .now() + delay, execute: work)
    }
}

public enum HumanConnectionError: Error {
    case notConnected
    case encodingFailed
    case disconnected
    case timeout
}
