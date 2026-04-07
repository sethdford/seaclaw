import Foundation
import HumanClient
import HumanOnDevice
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

public struct MemoryEntrySummary: Identifiable {
    public let id: String
    public let key: String
    public let content: String
    public let category: String
    public let source: String
    public let timestamp: String
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
    /// Gateway process uptime when `health` returns `uptime_seconds`.
    @Published public private(set) var uptimeSeconds: UInt64?
    @Published public private(set) var channelCount: Int?
    @Published public private(set) var toolCount: Int?
    @Published public private(set) var hulaProgramCount: Int = 0
    @Published public private(set) var memoryEntries: [MemoryEntrySummary] = []

    private var connection: HumanConnection?
    private var eventHandlerStorage: ((String, [String: AnyCodable]?) -> Void)?
    private let queue = DispatchQueue(label: "com.human.connectionManager")

    /// On-device Apple Intelligence adapter for offline/fallback inference.
    public let onDevice = OnDeviceChatAdapter()

    /// Whether on-device Apple Intelligence inference is available on this device.
    public var onDeviceAvailable: Bool { onDevice.isAvailable }

    public init() {
        self.gatewayURL = UserDefaults.standard.string(forKey: "Human.gatewayURL")
            ?? "wss://localhost:3000/ws"
    }

    public func connect() {
        queue.async { [weak self] in
            guard let self = self else { return }
            // Disconnect existing connection to prevent orphaned WebSockets
            self.connection?.disconnect()
            self.connection = nil
            let conn = HumanConnection(urlString: self.gatewayURL)
            conn.stateHandler = { [weak self] state in
                DispatchQueue.main.async {
                    let connected = (state == .connected)
                    self?.isConnected = connected
                    if connected {
                        self?.fetchHealthStatus()
                        self?.fetchRecentActivity()
                    } else {
                        self?.latencyMs = nil
                        self?.modelName = nil
                        self?.lastMessageTimestamp = nil
                    }
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
                self?.uptimeSeconds = nil
                self?.channelCount = nil
                self?.toolCount = nil
                self?.memoryEntries = []
            }
        }
    }

    public func reconnect() {
        disconnect()
        connect()
    }

    public func request(method: String, params: [String: AnyCodable]? = nil) async throws -> ControlResponse {
        try await withCheckedThrowingContinuation { continuation in
            queue.async { [weak self] in
                guard let self = self, let conn = self.connection else {
                    continuation.resume(throwing: HumanConnectionError.notConnected)
                    return
                }
                Task {
                    do {
                        let result = try await conn.request(method: method, params: params)
                        continuation.resume(returning: result)
                    } catch {
                        continuation.resume(throwing: error)
                    }
                }
            }
        }
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

    // MARK: - On-Device Fallback

    /// Send a message using on-device Apple Intelligence when the gateway is disconnected.
    /// Returns nil if on-device inference is not available.
    public func chatOnDevice(message: String, systemPrompt: String? = nil) async -> String? {
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

    /// `archived: true` moves the session to archived; `false` restores to active (gateway `sessions.patch` + `status`).
    public func updateSessionArchive(key: String, archived: Bool) {
        Task {
            do {
                let status = archived ? "archived" : "active"
                _ = try await request(
                    method: Methods.sessionsPatch,
                    params: ["key": AnyCodable(key), "status": AnyCodable(status)]
                )
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

    public func fetchHulaAnalytics() {
        Task {
            do {
                let resp = try await request(method: "hula.traces.analytics", params: nil)
                guard resp.ok, let payload = resp.payload else { return }
                if let summary = payload["summary"]?.value as? [String: Any],
                   let count = summary["file_count"] as? Int {
                    await MainActor.run { hulaProgramCount = count }
                }
            } catch {}
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

    /// Loads memory entries from `memory.list` (shape: `{ "entries": [ { key, content, category, source, timestamp } ] }`).
    public func fetchMemoryList(completion: ((Result<Void, Error>) -> Void)? = nil) {
        Task {
            do {
                let resp = try await request(method: Methods.memoryList, params: nil)
                guard resp.ok else {
                    await MainActor.run {
                        memoryEntries = []
                        completion?(.failure(MemoryListError.requestFailed))
                    }
                    return
                }
                guard let payload = resp.payload else {
                    await MainActor.run {
                        memoryEntries = []
                        completion?(.success(()))
                    }
                    return
                }
                let arr = payload["entries"]?.value as? [[String: Any]] ?? []
                let parsed = arr.compactMap { parseMemoryEntry($0) }
                await MainActor.run {
                    memoryEntries = parsed
                    completion?(.success(()))
                }
            } catch {
                await MainActor.run {
                    memoryEntries = []
                    completion?(.failure(error))
                }
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
                await MainActor.run {
                    healthStatus = status
                    applyHealthMetrics(payload)
                }
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
            fetchHulaAnalytics()
            if sessions.isEmpty { fetchSessions() }
            if tools.isEmpty { fetchTools() }
        case .sessions:
            if sessions.isEmpty { fetchSessions() }
        case .tools:
            if tools.isEmpty { fetchTools() }
        case .memory:
            if memoryEntries.isEmpty { fetchMemoryList() }
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

    private func parseMemoryEntry(_ dict: [String: Any]) -> MemoryEntrySummary? {
        guard let key = dict["key"] as? String, !key.isEmpty else { return nil }
        let content = dict["content"] as? String ?? ""
        let category = dict["category"] as? String ?? ""
        let source = dict["source"] as? String ?? ""
        let timestamp = dict["timestamp"] as? String ?? ""
        return MemoryEntrySummary(
            id: key,
            key: key,
            content: content,
            category: category,
            source: source,
            timestamp: timestamp
        )
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

    private func applyHealthMetrics(_ payload: [String: AnyCodable]) {
        if let u = payload["uptime_seconds"]?.value as? UInt64 {
            uptimeSeconds = u
        } else if let u = payload["uptime_seconds"]?.value as? Int {
            uptimeSeconds = UInt64(max(0, u))
        } else if let u = payload["uptime_seconds"]?.value as? Double {
            uptimeSeconds = UInt64(max(0, u))
        } else if let u = payload["uptime_seconds"]?.value as? NSNumber {
            uptimeSeconds = u.uint64Value
        }
        if let c = payload["channel_count"]?.value as? Int {
            channelCount = c
        } else if let c = payload["channel_count"]?.value as? Double {
            channelCount = Int(c)
        } else if let c = payload["channel_count"]?.value as? NSNumber {
            channelCount = c.intValue
        }
        if let t = payload["tool_count"]?.value as? Int {
            toolCount = t
        } else if let t = payload["tool_count"]?.value as? Double {
            toolCount = Int(t)
        } else if let t = payload["tool_count"]?.value as? NSNumber {
            toolCount = t.intValue
        }
        if let ms = payload["latency_ms"]?.value as? Int {
            latencyMs = ms
        } else if let ms = payload["latency_ms"]?.value as? Double {
            latencyMs = Int(ms)
        } else if let ms = payload["latency_ms"]?.value as? NSNumber {
            latencyMs = ms.intValue
        }
        if let m = payload["model"]?.value as? String, !m.isEmpty {
            modelName = m
        } else if let m = payload["default_model"]?.value as? String, !m.isEmpty {
            modelName = m
        } else if let m = payload["default_provider"]?.value as? String, !m.isEmpty {
            modelName = m
        }
    }
}

private enum MemoryListError: LocalizedError {
    case requestFailed

    var errorDescription: String? {
        switch self {
        case .requestFailed:
            return "Could not load memory from the gateway."
        }
    }
}
