import Foundation

class GatewayClient: ObservableObject {
    @Published var isConnected = false
    var eventHandler: ((String, [String: Any]?) -> Void)?
    private var task: URLSessionWebSocketTask?
    private var url: URL
    private var reconnectWorkItem: DispatchWorkItem?
    private let queue = DispatchQueue(label: "com.seaclaw.gateway")

    init(url: String = "ws://localhost:8080/ws") {
        self.url = URL(string: url) ?? URL(string: "ws://localhost:8080/ws")!
    }

    func connect() {
        queue.async { [weak self] in
            guard let self = self else { return }
            self.task?.cancel(with: .goingAway, reason: nil)
            var request = URLRequest(url: self.url)
            request.setValue("websocket", forHTTPHeaderField: "Upgrade")
            request.setValue("Upgrade", forHTTPHeaderField: "Connection")
            let session = URLSession(configuration: .default)
            self.task = session.webSocketTask(with: request)
            self.task?.resume()
            self.sendConnect()
            self.receive()
        }
    }

    func disconnect() {
        queue.async { [weak self] in
            self?.reconnectWorkItem?.cancel()
            self?.reconnectWorkItem = nil
            self?.task?.cancel(with: .goingAway, reason: nil)
            self?.task = nil
            DispatchQueue.main.async { [weak self] in
                self?.isConnected = false
            }
        }
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
                DispatchQueue.main.async { [weak self] in
                    self?.isConnected = false
                }
                self.scheduleReconnect()
            }
        }
    }

    private func handleMessage(_ text: String) {
        guard let data = text.data(using: .utf8),
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let type = json["type"] as? String else { return }

        switch type {
        case "res":
            if let payload = json["payload"] as? [String: Any],
               payload["type"] as? String == "hello-ok" {
                DispatchQueue.main.async { [weak self] in
                    self?.isConnected = true
                }
            }
        case "event":
            if let event = json["event"] as? String {
                let payload = json["payload"] as? [String: Any]
                self.eventHandler?(event, payload)
            }
        default:
            break
        }
    }

    private func sendConnect() {
        let connect: [String: Any] = [
            "type": "req",
            "id": "connect-\(UUID().uuidString)",
            "method": "connect",
            "params": [:]
        ]
        guard let data = try? JSONSerialization.data(withJSONObject: connect) else { return }
        task?.send(.data(data)) { _ in }
    }

    private func scheduleReconnect() {
        reconnectWorkItem?.cancel()
        let work = DispatchWorkItem { [weak self] in
            self?.connect()
        }
        reconnectWorkItem = work
        queue.asyncAfter(deadline: .now() + 3, execute: work)
    }
}
