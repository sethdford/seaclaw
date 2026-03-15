import SwiftUI
import HumanChatUI

/// Motion 9 spring for all interactive animations.
private let springMotion9 = Animation.spring(response: 0.35, dampingFraction: 0.86)

@main
struct HumanApp: App {
    @StateObject private var status = StatusViewModel()

    var body: some Scene {
        WindowGroup("h-uman Dashboard") {
            DashboardView()
                .environmentObject(status)
                .frame(minWidth: 600, minHeight: 400)
        }
        .defaultSize(width: 900, height: 600)
        .commands {
            CommandGroup(replacing: .newItem) {
                Button("New Chat") {
                    status.selectedTab = .chat
                    status.connectIfNeeded()
                }
                .keyboardShortcut("n", modifiers: .command)
                .accessibilityLabel("New chat")
            }
            CommandGroup(after: .appInfo) {
                Button("Check for Updates...") {
                    // Placeholder: app update check
                }
                .accessibilityLabel("Check for updates")
            }
            CommandMenu("Chat") {
                Button("New Chat") {
                    status.selectedTab = .chat
                    status.connectIfNeeded()
                }
                .keyboardShortcut("n", modifiers: .command)
                .accessibilityLabel("New chat")
                Button("Clear History") {
                    // Placeholder: clear chat history
                }
                .keyboardShortcut("k", modifiers: [.command, .shift])
                .accessibilityLabel("Clear chat history")
            }
            CommandMenu("View") {
                Button("Overview") { status.selectedTab = .overview }
                    .keyboardShortcut("1", modifiers: .command)
                    .accessibilityLabel("Navigate to Overview tab")
                Button("Chat") { status.selectedTab = .chat }
                    .keyboardShortcut("2", modifiers: .command)
                    .accessibilityLabel("Navigate to Chat tab")
                Button("Sessions") { status.selectedTab = .sessions }
                    .keyboardShortcut("3", modifiers: .command)
                    .accessibilityLabel("Navigate to Sessions tab")
                Button("Tools") { status.selectedTab = .tools }
                    .keyboardShortcut("4", modifiers: .command)
                    .accessibilityLabel("Navigate to Tools tab")
                Button("Settings") { status.selectedTab = .settings }
                    .keyboardShortcut("5", modifiers: .command)
                    .accessibilityLabel("Navigate to Settings tab")
            }
            CommandMenu("Service") {
                Button("Start Service") {
                    withAnimation(springMotion9) {
                        status.startService()
                    }
                }
                .keyboardShortcut("r", modifiers: [.command])
                .disabled(status.isServiceRunning)
                .accessibilityLabel("Start h-uman background service")

                Button("Stop Service") {
                    withAnimation(springMotion9) {
                        status.stopService()
                    }
                }
                .keyboardShortcut("r", modifiers: [.command, .shift])
                .disabled(!status.isServiceRunning)
                .accessibilityLabel("Stop h-uman background service")

                Divider()

                Button("Open in Browser") {
                    if let url = URL(string: "http://localhost:3000") {
                        NSWorkspace.shared.open(url)
                    }
                }
                .keyboardShortcut("o", modifiers: [.command, .shift])
                .accessibilityLabel("Open h-uman dashboard in browser")
            }
        }

        Settings {
            SettingsView()
                .environmentObject(status)
        }

        MenuBarExtra {
            VStack(alignment: .leading, spacing: 0) {
                Button("Open Dashboard") {
                    if let url = URL(string: "http://localhost:3000") {
                        NSWorkspace.shared.open(url)
                    }
                }
                .keyboardShortcut("d")
                .accessibilityLabel("Open h-uman dashboard in browser")
                Button("Start Service") {
                    withAnimation(springMotion9) {
                        status.startService()
                    }
                }
                .keyboardShortcut("s")
                .disabled(status.isServiceRunning)
                .accessibilityLabel("Start h-uman background service")
                Button("Stop Service") {
                    withAnimation(springMotion9) {
                        status.stopService()
                    }
                }
                .disabled(!status.isServiceRunning)
                .accessibilityLabel("Stop h-uman background service")
                Divider()
                Button("Settings...") {
                    NSApplication.shared.sendAction(Selector(("showSettingsWindow:")), to: nil, from: nil)
                }
                .keyboardShortcut(",")
                .accessibilityLabel("Open h-uman settings")
                Divider()
                Button("Quit") {
                    status.stopService()
                    NSApplication.shared.terminate(nil)
                }
                .keyboardShortcut("q")
                .accessibilityLabel("Quit h-uman")
            }
            .font(.custom("Avenir-Book", size: HUTokens.textBase))
            .frame(minWidth: 200)
        } label: {
            Image(systemName: "antenna.radiowaves.left.and.right")
                .symbolRenderingMode(.hierarchical)
                .foregroundStyle(status.statusColor)
                .animation(springMotion9, value: "\(status.isServiceRunning)-\(status.isGatewayConnected)")
                .accessibilityLabel("h-uman menu: \(status.isServiceRunning ? "service running" : "service stopped")")
        }
    }
}
