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
            LazyView(OverviewView())
                .tabItem {
                    Label("Overview", systemImage: "square.grid.2x2")
                }
                .tag(AppTab.overview)
            LazyView(ChatView())
                .tabItem {
                    Label("Chat", systemImage: "bubble.left.and.bubble.right")
                }
                .tag(AppTab.chat)
            LazyView(SessionsView())
                .tabItem {
                    Label("Sessions", systemImage: "clock.arrow.circlepath")
                }
                .tag(AppTab.sessions)
            LazyView(ToolsView())
                .tabItem {
                    Label("Tools", systemImage: "wrench.and.screwdriver")
                }
                .tag(AppTab.tools)
            LazyView(SettingsView())
                .tabItem {
                    Label("Settings", systemImage: "gear")
                }
                .tag(AppTab.settings)
        }
        .tint(accentColor)
#if os(iOS)
        .overlay(alignment: .topLeading) {
            // iPad keyboard shortcuts: Cmd+1..5 for tabs, Cmd+N for new chat
            Group {
                Button { selectedTab = .overview } label: { EmptyView() }
                    .keyboardShortcut("1", modifiers: .command)
                Button { selectedTab = .chat } label: { EmptyView() }
                    .keyboardShortcut("2", modifiers: .command)
                Button { selectedTab = .sessions } label: { EmptyView() }
                    .keyboardShortcut("3", modifiers: .command)
                Button { selectedTab = .tools } label: { EmptyView() }
                    .keyboardShortcut("4", modifiers: .command)
                Button { selectedTab = .settings } label: { EmptyView() }
                    .keyboardShortcut("5", modifiers: .command)
                Button { selectedTab = .chat } label: { EmptyView() }
                    .keyboardShortcut("n", modifiers: .command)
            }
            .frame(width: 0, height: 0)
            .opacity(0)
            .allowsHitTesting(false)
        }
#endif
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
