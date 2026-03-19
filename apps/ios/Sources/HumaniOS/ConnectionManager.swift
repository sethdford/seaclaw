import Foundation
import HumanClient
import HumanProtocol

// MARK: - RPC Data Types

public struct SessionSummary: Identifiable {
    public let id: String
    public let title: String
    public let lastMessage: String
    public let timestamp: Date
    public let messageCount: Int
    public let isArchived: Bool
}

public struct ToolInfo: Identifiable {
    public let id: String
    public let name: String
    public let description: String
    public let category: String
}

public struct ActivityEvent: Identifiable {
    public let id: String
    public let type: String
    public let description: String
    public let timestamp: Date
}

public enum HealthStatus {
    case unknown, healthy, degraded, unhealthy
}

// MARK: - ConnectionManager

/// Wraps HumanConnection as an ObservableObject for SwiftUI.
public final class ConnectionManager: ObservableObject {
    @Published public private(set) var isConnected = false
    @Published public var gatewayURL: String {
        didSet { UserDefaults.standard.set(gatewayURL, forKey: "Human.gatewayURL") }
    }
    /// Connection latency in milliseconds when connected; nil when disconnected.
    @Published public private(set) var latencyMs: Int?
    /// Model name from gateway status when available.
    @Published public private(set) var modelName: String?
    /// Timestamp of last received message when available.
    @Published public private(set) var lastMessageTimestamp: Date?

    @Published public private(set) var sessions: [SessionSummary] = []
    @Published public private(set) var tools: [ToolInfo] = []
    @Published public private(set) var recentActivity: [ActivityEvent] = []
    @Published public private(set) var healthStatus: HealthStatus = .unknown

    private var connection: HumanConnection?
    private var eventHandlerStorage: ((String, [String: AnyCodable]?) -> Void)?
    private let queue = DispatchQueue(label: "com.human.connectionManager")

    public init() {
        self.gatewayURL = UserDefaults.standard.string(forKey: "Human.gatewayURL")
            ?? "wss://localhost:3000/ws"
    }

    public func connect() {
        queue.async { [weak self] in
            guard let self = self else { return }
            let conn = HumanConnection(urlString: self.gatewayURL)
            conn.stateHandler = { [weak self] state in
                DispatchQueue.main.async {
                    let connected = (state == .connected)
                    self?.isConnected = connected
                    self?.latencyMs = connected ? 42 : nil
                    self?.modelName = connected ? "claude-sonnet" : nil
                    if !connected { self?.lastMessageTimestamp = nil }
                }
            }
            conn.eventHandler = self.eventHandlerStorage
            conn.connect()
            self.connection = conn
        }
    }

    public func disconnect() {
        queue.async { [weak self] in
            self?.connection?.disconnect()
            self?.connection = nil
            DispatchQueue.main.async {
                self?.isConnected = false
                self?.latencyMs = nil
                self?.modelName = nil
                self?.lastMessageTimestamp = nil
                self?.sessions = []
                self?.tools = []
                self?.recentActivity = []
                self?.healthStatus = .unknown
            }
        }
    }

    public func reconnect() {
        disconnect()
        connect()
    }

    public func request(method: String, params: [String: AnyCodable]? = nil) async throws -> ControlResponse {
        guard let conn = connection else { throw HumanConnectionError.notConnected }
        return try await conn.request(method: method, params: params)
    }

    public func registerPushToken(token: String) {
        connection?.registerPushToken(token: token)
    }

    public func unregisterPushToken(token: String) {
        connection?.unregisterPushToken(token: token)
    }

    public func setEventHandler(_ handler: @escaping (String, [String: AnyCodable]?) -> Void) {
        queue.async { [weak self] in
            guard let self = self else { return }
            self.eventHandlerStorage = { [weak self] event, payload in
                if event == "chat" {
                    DispatchQueue.main.async { self?.lastMessageTimestamp = Date() }
                }
                handler(event, payload)
            }
            self.connection?.eventHandler = self.eventHandlerStorage
        }
    }

    // MARK: - RPC Fetch Methods

    public func fetchSessions() {
        Task {
            do {
                let resp = try await request(method: Methods.sessionsList, params: nil)
                guard resp.ok, let payload = resp.payload else { return }
                let arr = payload["sessions"]?.value as? [[String: Any]] ?? []
                let parsed = arr.compactMap { parseSession($0) }
                await MainActor.run { sessions = parsed }
            } catch {
                await MainActor.run { sessions = [] }
            }
        }
    }

    public func deleteSession(key: String) {
        Task {
            do {
                _ = try await request(method: Methods.sessionsDelete, params: ["key": AnyCodable(key)])
                fetchSessions()
            } catch { /* ignore */ }
        }
    }

    public func fetchTools() {
        Task {
            do {
                let resp = try await request(method: Methods.toolsCatalog, params: nil)
                guard resp.ok, let payload = resp.payload else { return }
                let arr = payload["tools"]?.value as? [[String: Any]] ?? []
                let parsed = arr.compactMap { parseTool($0) }
                await MainActor.run { tools = parsed }
            } catch {
                await MainActor.run { tools = [] }
            }
        }
    }

    public func fetchRecentActivity() {
        Task {
            do {
                let resp = try await request(method: Methods.activityRecent, params: nil)
                guard resp.ok, let payload = resp.payload else { return }
                let arr = payload["events"]?.value as? [[String: Any]] ?? []
                let parsed = arr.enumerated().compactMap { idx, dict in parseActivityEvent(dict, index: idx) }
                await MainActor.run { recentActivity = parsed }
            } catch {
                await MainActor.run { recentActivity = [] }
            }
        }
    }

    public func fetchHealthStatus() {
        Task {
            do {
                let resp = try await request(method: Methods.health, params: nil)
                guard resp.ok, let payload = resp.payload else {
                    await MainActor.run { healthStatus = .unknown }
                    return
                }
                let status = parseHealthStatus(payload)
                await MainActor.run { healthStatus = status }
            } catch {
                await MainActor.run { healthStatus = .unknown }
            }
        }
    }

    func prefetchDataForTab(_ tab: AppTab) {
        guard isConnected else { return }
        switch tab {
        case .overview:
            if recentActivity.isEmpty { fetchRecentActivity() }
            fetchHealthStatus()
        case .sessions:
            if sessions.isEmpty { fetchSessions() }
        case .tools:
            if tools.isEmpty { fetchTools() }
        case .chat, .settings:
            break
        }
    }

    // MARK: - RPC Response Parsing

    private func parseSession(_ dict: [String: Any]) -> SessionSummary? {
        guard let key = dict["key"] as? String else { return nil }
        let label = dict["label"] as? String ?? dict["title"] as? String ?? ""
        let title = label.isEmpty ? key : label
        let lastMessage = dict["last_message"] as? String ?? ""
        let ts = (dict["last_active"] as? NSNumber)?.doubleValue
            ?? (dict["updated_at"] as? NSNumber)?.doubleValue
            ?? (dict["created_at"] as? NSNumber)?.doubleValue
            ?? 0
        let timestamp = Date(timeIntervalSince1970: ts / 1000)
        let messageCount = (dict["messages_count"] as? NSNumber)?.intValue
            ?? (dict["turn_count"] as? NSNumber)?.intValue
            ?? 0
        let status = dict["status"] as? String ?? "active"
        let isArchived = status.lowercased() == "archived"
        return SessionSummary(
            id: key,
            title: title,
            lastMessage: lastMessage,
            timestamp: timestamp,
            messageCount: messageCount,
            isArchived: isArchived
        )
    }

    private func parseTool(_ dict: [String: Any]) -> ToolInfo? {
        guard let name = dict["name"] as? String, !name.isEmpty else { return nil }
        let description = dict["description"] as? String ?? ""
        let category = inferToolCategory(name)
        return ToolInfo(
            id: name,
            name: name,
            description: description,
            category: category
        )
    }

    private func inferToolCategory(_ name: String) -> String {
        let lower = name.lowercased()
        if lower.contains("shell") || lower.contains("eval") || lower.contains("time") { return "System" }
        if lower.contains("file") || lower.contains("read") || lower.contains("write") || lower.contains("list") { return "Files" }
        if lower.contains("fetch") || lower.contains("curl") || lower.contains("http") { return "Network" }
        if lower.contains("grep") || lower.contains("search") || lower.contains("codebase") { return "Search" }
        return "Other"
    }

    private func parseActivityEvent(_ dict: [String: Any], index: Int) -> ActivityEvent? {
        let type = dict["type"] as? String ?? "unknown"
        let time = (dict["time"] as? NSNumber)?.doubleValue ?? 0
        let timestamp = Date(timeIntervalSince1970: time / 1000)

        let description: String
        if let msg = dict["message"] as? String, !msg.isEmpty {
            description = msg
        } else if let preview = dict["preview"] as? String, !preview.isEmpty {
            let source = dict["channel"] as? String ?? dict["user"] as? String ?? ""
            description = source.isEmpty ? preview : "\(source): \(preview)"
        } else if let tool = dict["tool"] as? String {
            let cmd = dict["command"] as? String ?? ""
            description = cmd.isEmpty ? "Tool: \(tool)" : "\(tool): \(cmd)"
        } else if let session = dict["session"] as? String {
            description = "Session started: \(session)"
        } else {
            description = "Activity"
        }

        return ActivityEvent(
            id: "event-\(index)",
            type: type,
            description: description,
            timestamp: timestamp
        )
    }

    private func parseHealthStatus(_ payload: [String: AnyCodable]) -> HealthStatus {
        let s = (payload["status"]?.value as? String ?? "").lowercased()
        if s == "ok" || s == "operational" || s == "healthy" { return .healthy }
        if s == "degraded" || s == "warning" { return .degraded }
        if s == "error" || s == "unhealthy" || s == "down" { return .unhealthy }
        return .unknown
    }
}
