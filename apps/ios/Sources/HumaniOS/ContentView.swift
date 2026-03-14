import SwiftUI
import HumanChatUI

enum AppTab: Int, CaseIterable {
    case overview, chat, sessions, tools, settings
}

struct ContentView: View {
    @Environment(\.colorScheme) private var colorScheme
    @EnvironmentObject var connectionManager: ConnectionManager
    @State private var selectedTab: AppTab = .overview

    private var accentColor: Color {
        colorScheme == .dark ? HUTokens.Dark.accent : HUTokens.Light.accent
    }

    var body: some View {
        TabView(selection: $selectedTab) {
            OverviewView()
                .tabItem {
                    Label("Overview", systemImage: "square.grid.2x2")
                }
                .tag(AppTab.overview)
            ChatView()
                .tabItem {
                    Label("Chat", systemImage: "bubble.left.and.bubble.right")
                }
                .tag(AppTab.chat)
            SessionsView()
                .tabItem {
                    Label("Sessions", systemImage: "clock.arrow.circlepath")
                }
                .tag(AppTab.sessions)
            ToolsView()
                .tabItem {
                    Label("Tools", systemImage: "wrench.and.screwdriver")
                }
                .tag(AppTab.tools)
            SettingsView()
                .tabItem {
                    Label("Settings", systemImage: "gear")
                }
                .tag(AppTab.settings)
        }
        .tint(accentColor)
        .onReceive(NotificationCenter.default.publisher(for: .navigateToTab)) { notification in
            if let tab = notification.userInfo?["tab"] as? AppTab {
                withAnimation(HUTokens.springExpressive) {
                    selectedTab = tab
                }
            }
        }
        .onChange(of: selectedTab) { _, _ in
#if os(iOS)
            HUTokens.Haptic.selection.trigger()
#endif
        }
        .onChange(of: connectionManager.isConnected) { _, isConnected in
            withAnimation(HUTokens.springExpressive) {
#if os(iOS)
                let notification = UINotificationFeedbackGenerator()
                if isConnected {
                    notification.notificationOccurred(.success)
                } else {
                    notification.notificationOccurred(.error)
                }
#endif
            }
        }
    }
}
