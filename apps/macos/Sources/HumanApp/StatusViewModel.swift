import AppKit
import Combine
import HumanChatUI
import HumanProtocol
import SwiftUI

enum MacTab: String, CaseIterable {
    case overview, chat, sessions, tools, settings
}

struct MacSessionRow: Identifiable {
    let id: String
    let title: String
    let preview: String
    let relativeTime: String
    let messageCount: Int
    let isArchived: Bool
}

struct MacToolRow: Identifiable {
    let id: String
    let name: String
    let description: String
    let category: String
}

@MainActor
class StatusViewModel: ObservableObject {
    @Published var isServiceRunning = false
    @Published var gatewayURL: String = "ws://localhost:3000/ws"
    @Published var selectedTab: MacTab = .overview

    @Published var overviewChannelCount = "—"
    @Published var overviewToolCount = "—"
    @Published var overviewModel = "—"
    @Published var overviewUptime = "—"

    @Published var sessionRows: [MacSessionRow] = []
    @Published var toolRows: [MacToolRow] = []
    @Published var serviceStartError: String?

    var isGatewayConnected: Bool { gatewayClient.isConnected }
    var statusColor: Color {
        if isServiceRunning && isGatewayConnected {
            return HUTokens.Dark.accent
        }
        if isServiceRunning || isGatewayConnected {
            return HUTokens.Dark.warning
        }
        return HUTokens.Dark.error
    }

    let gatewayClient: GatewayClient
    private let processManager = ProcessManager()
    private var pollTimer: Timer?
    private var cancellables = Set<AnyCancellable>()
    private let relativeFormatter: RelativeDateTimeFormatter = {
        let f = RelativeDateTimeFormatter()
        f.unitsStyle = .abbreviated
        return f
    }()

    init() {
        gatewayClient = GatewayClient(url: "ws://localhost:3000/ws")
        processManager.onStartFailure = { [weak self] msg in
            Task { @MainActor in
                self?.serviceStartError = msg
                let alert = NSAlert()
                alert.messageText = "Could not start service"
                alert.informativeText = msg
                alert.alertStyle = .warning
                alert.addButton(withTitle: "OK")
                alert.runModal()
            }
        }
        gatewayClient.eventHandler = { [weak self] event, payload in
            Task { @MainActor in
                switch event {
                case "error":
                    let msg = payload?["message"] as? String ?? payload?["error"] as? String ?? "Unknown error"
                    let alert = NSAlert()
                    alert.messageText = "Human Error"
                    alert.informativeText = msg
                    alert.alertStyle = .warning
                    alert.addButton(withTitle: "OK")
                    alert.runModal()
                case "health":
                    self?.objectWillChange.send()
                default:
                    break
                }
            }
        }
        gatewayClient.objectWillChange
            .receive(on: RunLoop.main)
            .sink { [weak self] _ in self?.objectWillChange.send() }
            .store(in: &cancellables)
        startPolling()
    }

    /// Try to open the gateway connection (idempotent).
    func ensureGatewayConnection() {
        gatewayClient.connectIfNeeded()
    }

    /// Connect when Chat (or another live tab) is shown.
    func connectIfNeeded() {
        gatewayClient.connectIfNeeded()
    }

    func refreshOverviewFromGateway() {
        ensureGatewayConnection()
        gatewayClient.request(method: "health", params: [:]) { [weak self] result in
            guard let self = self else { return }
            switch result {
            case let .success(dict):
                Task { @MainActor in
                    self.applyHealthPayload(dict)
                }
            case .failure:
                break
            }
        }
        gatewayClient.request(method: Methods.capabilities, params: [:]) { [weak self] result in
            guard let self = self else { return }
            switch result {
            case let .success(dict):
                Task { @MainActor in
                    if let tools = dict["tools"] as? Int {
                        self.overviewToolCount = "\(tools)"
                    } else if let tools = dict["tools"] as? Double {
                        self.overviewToolCount = "\(Int(tools))"
                    }
                    if self.overviewChannelCount == "—" {
                        if let ch = dict["channels"] as? Int {
                            self.overviewChannelCount = "\(ch)"
                        } else if let ch = dict["channels"] as? Double {
                            self.overviewChannelCount = "\(Int(ch))"
                        }
                    }
                }
            case .failure:
                break
            }
        }
    }

    func fetchSessionsList() {
        ensureGatewayConnection()
        gatewayClient.request(method: Methods.sessionsList, params: [:]) { [weak self] result in
            guard let self = self else { return }
            switch result {
            case let .success(dict):
                let arr = dict["sessions"] as? [[String: Any]] ?? []
                Task { @MainActor in
                    self.sessionRows = arr.compactMap { self.parseSessionRow($0) }
                }
            case .failure:
                Task { @MainActor in
                    self.sessionRows = []
                }
            }
        }
    }

    func fetchToolsCatalog() {
        ensureGatewayConnection()
        gatewayClient.request(method: Methods.toolsCatalog, params: [:]) { [weak self] result in
            guard let self = self else { return }
            switch result {
            case let .success(dict):
                let arr = dict["tools"] as? [[String: Any]] ?? []
                Task { @MainActor in
                    self.toolRows = arr.compactMap { self.parseToolRow($0) }
                }
            case .failure:
                Task { @MainActor in
                    self.toolRows = []
                }
            }
        }
    }

    func checkForUpdates() async {
        ensureGatewayConnection()
        await withCheckedContinuation { (cont: CheckedContinuation<Void, Never>) in
            gatewayClient.request(method: Methods.updateCheck, params: [:]) { result in
                Task { @MainActor in
                    let alert = NSAlert()
                    switch result {
                    case let .success(dict):
                        let available = dict["available"] as? Bool ?? false
                        alert.messageText = available ? "Update available" : "You’re up to date"
                        if let v = dict["current_version"] as? String {
                            alert.informativeText = "Current version: \(v)"
                        }
                    case let .failure(err):
                        alert.messageText = "Update check failed"
                        alert.informativeText = err.localizedDescription
                        alert.alertStyle = .warning
                    }
                    alert.addButton(withTitle: "OK")
                    alert.runModal()
                    cont.resume()
                }
            }
        }
    }

    deinit {
        pollTimer?.invalidate()
        gatewayClient.disconnect()
    }

    private func startPolling() {
        pollTimer = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { [weak self] _ in
            Task { @MainActor in
                self?.isServiceRunning = self?.processManager.isRunning ?? false
            }
        }
        pollTimer?.tolerance = 0.2
        RunLoop.main.add(pollTimer!, forMode: .common)
    }

    func startService() {
        serviceStartError = nil
        processManager.start()
    }

    func stopService() {
        processManager.stop()
    }

    private func applyHealthPayload(_ dict: [String: Any]) {
        if let ch = dict["channel_count"] as? Int {
            overviewChannelCount = "\(ch)"
        } else if let ch = dict["channel_count"] as? Double {
            overviewChannelCount = "\(Int(ch))"
        }
        if let t = dict["tool_count"] as? Int {
            overviewToolCount = "\(t)"
        } else if let t = dict["tool_count"] as? Double {
            overviewToolCount = "\(Int(t))"
        }
        if let m = dict["default_model"] as? String, !m.isEmpty {
            overviewModel = m
        } else if let p = dict["default_provider"] as? String, !p.isEmpty {
            overviewModel = p
        }
        if let u = dict["uptime_seconds"] as? UInt64 {
            overviewUptime = formatUptime(u)
        } else if let u = dict["uptime_seconds"] as? Int {
            overviewUptime = formatUptime(UInt64(max(0, u)))
        } else if let u = dict["uptime_seconds"] as? Double {
            overviewUptime = formatUptime(UInt64(max(0, u)))
        }
    }

    private func formatUptime(_ seconds: UInt64) -> String {
        let d = seconds / 86400
        let h = (seconds % 86400) / 3600
        let m = (seconds % 3600) / 60
        if d > 0 { return "\(d)d \(h)h" }
        if h > 0 { return "\(h)h \(m)m" }
        return "\(m)m"
    }

    private func parseSessionRow(_ dict: [String: Any]) -> MacSessionRow? {
        guard let key = dict["key"] as? String else { return nil }
        let label = dict["label"] as? String ?? dict["title"] as? String ?? ""
        let title = label.isEmpty ? key : label
        let lastMessage = dict["last_message"] as? String ?? ""
        let ts = (dict["last_active"] as? NSNumber)?.doubleValue
            ?? (dict["updated_at"] as? NSNumber)?.doubleValue
            ?? (dict["created_at"] as? NSNumber)?.doubleValue
            ?? 0
        let date = Date(timeIntervalSince1970: ts / 1000)
        let messageCount = (dict["messages_count"] as? NSNumber)?.intValue
            ?? (dict["turn_count"] as? NSNumber)?.intValue
            ?? 0
        let preview = lastMessage.isEmpty ? "No messages yet" : lastMessage
        let rel = relativeFormatter.localizedString(for: date, relativeTo: Date())
        let status = (dict["status"] as? String ?? "").lowercased()
        let archived = status == "archived"
        return MacSessionRow(
            id: key,
            title: title,
            preview: preview,
            relativeTime: rel,
            messageCount: messageCount,
            isArchived: archived
        )
    }

    private func parseToolRow(_ dict: [String: Any]) -> MacToolRow? {
        guard let name = dict["name"] as? String, !name.isEmpty else { return nil }
        let description = dict["description"] as? String ?? ""
        let category = inferToolCategory(name)
        return MacToolRow(id: name, name: name, description: description, category: category)
    }

    private func inferToolCategory(_ name: String) -> String {
        let lower = name.lowercased()
        if lower.contains("shell") || lower.contains("eval") || lower.contains("time") { return "System" }
        if lower.contains("file") || lower.contains("read") || lower.contains("write") || lower.contains("list") {
            return "Files"
        }
        if lower.contains("fetch") || lower.contains("curl") || lower.contains("http") { return "Network" }
        if lower.contains("grep") || lower.contains("search") || lower.contains("codebase") { return "Search" }
        return "Other"
    }
}
