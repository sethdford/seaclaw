import SwiftUI
import HumanChatUI

struct DashboardView: View {
    @EnvironmentObject var status: StatusViewModel
    @Environment(\.colorScheme) private var colorScheme

    private var tokens: (bgSurface: Color, surfaceContainer: Color, text: Color, textMuted: Color, accent: Color, success: Color, error: Color) {
        if colorScheme == .dark {
            return (HUTokens.Dark.bgSurface, HUTokens.Dark.surfaceContainer, HUTokens.Dark.text, HUTokens.Dark.textMuted, HUTokens.Dark.accent, HUTokens.Dark.success, HUTokens.Dark.error)
        } else {
            return (HUTokens.Light.bgSurface, HUTokens.Light.surfaceContainer, HUTokens.Light.text, HUTokens.Light.textMuted, HUTokens.Light.accent, HUTokens.Light.success, HUTokens.Light.error)
        }
    }

    var body: some View {
        NavigationSplitView {
            sidebar
        } detail: {
            detailContent
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                .background(tokens.bgSurface)
        }
        .navigationSplitViewStyle(.balanced)
    }

    @ViewBuilder
    private var sidebar: some View {
        List(selection: $status.selectedTab) {
            Label("Overview", systemImage: "square.grid.2x2")
                .tag(MacTab.overview)
                .accessibilityLabel("Overview tab")
            Label("Chat", systemImage: "bubble.left.and.bubble.right")
                .tag(MacTab.chat)
                .accessibilityLabel("Chat tab")
            Label("Sessions", systemImage: "clock.arrow.circlepath")
                .tag(MacTab.sessions)
                .accessibilityLabel("Sessions tab")
            Label("Tools", systemImage: "wrench.and.screwdriver")
                .tag(MacTab.tools)
                .accessibilityLabel("Tools tab")

            Section("System") {
                Label("Settings", systemImage: "gearshape")
                    .tag(MacTab.settings)
                    .accessibilityLabel("Settings tab")
            }
        }
        .listStyle(.sidebar)
        .frame(minWidth: 180)
        .toolbar {
            ToolbarItem(placement: .automatic) {
                HStack(spacing: HUTokens.spaceXs) {
                    Circle()
                        .fill(status.isServiceRunning ? tokens.success : tokens.error)
                        .frame(width: 8, height: 8)
                    Text(status.isServiceRunning ? "Running" : "Stopped")
                        .font(.custom("Avenir-Book", size: HUTokens.textXs))
                        .foregroundStyle(tokens.textMuted)
                }
                .accessibilityElement(children: .combine)
                .accessibilityLabel("Service \(status.isServiceRunning ? "running" : "stopped")")
            }
        }
    }

    @ViewBuilder
    private var detailContent: some View {
        switch status.selectedTab {
        case .overview:
            MacOverviewPane(tokens: tokens, status: status)
        case .chat:
            MacChatPane(tokens: tokens)
        case .sessions:
            MacSessionsPane(tokens: tokens)
        case .tools:
            MacToolsPane(tokens: tokens)
        case .settings:
            SettingsView()
        }
    }
}

struct MacOverviewPane: View {
    let tokens: (bgSurface: Color, surfaceContainer: Color, text: Color, textMuted: Color, accent: Color, success: Color, error: Color)
    @ObservedObject var status: StatusViewModel
    @State private var appeared = false

    private let stats: [(String, String, String)] = [
        ("Providers", "9", "cpu"),
        ("Channels", "34", "bubble.left.and.bubble.right"),
        ("Tools", "67", "wrench.and.screwdriver"),
        ("Memory", "5.7 MB", "memorychip"),
    ]

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: HUTokens.spaceLg) {
                Text("Overview")
                    .font(.custom("Avenir-Heavy", size: HUTokens.text2xl))
                    .foregroundStyle(tokens.text)

                HStack(spacing: HUTokens.spaceMd) {
                    Circle()
                        .fill(status.isGatewayConnected ? tokens.success : tokens.error)
                        .frame(width: 10, height: 10)
                    Text(status.isGatewayConnected ? "Gateway Connected" : "Gateway Disconnected")
                        .font(.custom("Avenir-Medium", size: HUTokens.textBase))
                        .foregroundStyle(tokens.text)
                    Spacer()
                    Text(status.gatewayURL)
                        .font(.custom("Avenir-Book", size: HUTokens.textSm))
                        .foregroundStyle(tokens.textMuted)
                }
                .padding(HUTokens.spaceMd)
                .background(tokens.surfaceContainer)
                .clipShape(RoundedRectangle(cornerRadius: HUTokens.radiusLg, style: .continuous))
                .accessibilityElement(children: .combine)
                .accessibilityLabel("Gateway \(status.isGatewayConnected ? "connected" : "disconnected") at \(status.gatewayURL)")

                LazyVGrid(columns: Array(repeating: GridItem(.flexible(), spacing: HUTokens.spaceMd), count: 4), spacing: HUTokens.spaceMd) {
                    ForEach(Array(stats.enumerated()), id: \.offset) { index, stat in
                        VStack(alignment: .leading, spacing: HUTokens.spaceSm) {
                            Image(systemName: stat.2)
                                .font(.custom("Avenir-Medium", size: HUTokens.textLg))
                                .foregroundStyle(tokens.accent)
                            Text(stat.1)
                                .font(.custom("Avenir-Heavy", size: HUTokens.textXl))
                                .foregroundStyle(tokens.text)
                            Text(stat.0)
                                .font(.custom("Avenir-Book", size: HUTokens.textXs))
                                .foregroundStyle(tokens.textMuted)
                        }
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(HUTokens.spaceMd)
                        .background(tokens.surfaceContainer)
                        .clipShape(RoundedRectangle(cornerRadius: HUTokens.radiusLg, style: .continuous))
                        .accessibilityElement(children: .combine)
                        .accessibilityLabel("\(stat.0): \(stat.1)")
                        .opacity(appeared ? 1 : 0)
                        .scaleEffect(appeared ? 1 : 0.9)
                        .animation(HUTokens.springExpressive.delay(Double(index) * 0.05), value: appeared)
                    }
                }

                Text("Service Status")
                    .font(.custom("Avenir-Heavy", size: HUTokens.textLg))
                    .foregroundStyle(tokens.text)

                HStack(spacing: HUTokens.spaceLg) {
                    VStack(alignment: .leading, spacing: HUTokens.spaceXs) {
                        Text("Binary")
                            .font(.custom("Avenir-Book", size: HUTokens.textSm))
                            .foregroundStyle(tokens.textMuted)
                        Text("~1696 KB")
                            .font(.custom("Avenir-Heavy", size: HUTokens.textBase))
                            .foregroundStyle(tokens.text)
                    }
                    VStack(alignment: .leading, spacing: HUTokens.spaceXs) {
                        Text("Startup")
                            .font(.custom("Avenir-Book", size: HUTokens.textSm))
                            .foregroundStyle(tokens.textMuted)
                        Text("<30 ms")
                            .font(.custom("Avenir-Heavy", size: HUTokens.textBase))
                            .foregroundStyle(tokens.text)
                    }
                    VStack(alignment: .leading, spacing: HUTokens.spaceXs) {
                        Text("Peak RSS")
                            .font(.custom("Avenir-Book", size: HUTokens.textSm))
                            .foregroundStyle(tokens.textMuted)
                        Text("5.7 MB")
                            .font(.custom("Avenir-Heavy", size: HUTokens.textBase))
                            .foregroundStyle(tokens.text)
                    }
                    Spacer()
                }
                .padding(HUTokens.spaceMd)
                .background(tokens.surfaceContainer)
                .clipShape(RoundedRectangle(cornerRadius: HUTokens.radiusLg, style: .continuous))
            }
            .padding(HUTokens.spaceLg)
        }
        .onAppear {
            withAnimation(HUTokens.springExpressive) { appeared = true }
        }
    }
}

struct MacChatPane: View {
    let tokens: (bgSurface: Color, surfaceContainer: Color, text: Color, textMuted: Color, accent: Color, success: Color, error: Color)

    var body: some View {
        VStack {
            Spacer()
            Text("Chat")
                .font(.custom("Avenir-Heavy", size: HUTokens.textLg))
                .foregroundStyle(tokens.textMuted)
            Text("Connect to gateway to start chatting")
                .font(.custom("Avenir-Book", size: HUTokens.textSm))
                .foregroundStyle(tokens.textMuted)
            Spacer()
        }
        .frame(maxWidth: .infinity)
        .accessibilityLabel("Chat view, connect to gateway to start")
    }
}

struct MacSessionsPane: View {
    let tokens: (bgSurface: Color, surfaceContainer: Color, text: Color, textMuted: Color, accent: Color, success: Color, error: Color)

    private let sessions: [(String, String, String)] = [
        ("CLI conversation", "2 min ago", "12 messages"),
        ("Telegram support", "1 hour ago", "8 messages"),
        ("Discord sync", "3 hours ago", "24 messages"),
    ]

    var body: some View {
        VStack(alignment: .leading, spacing: HUTokens.spaceMd) {
            Text("Sessions")
                .font(.custom("Avenir-Heavy", size: HUTokens.text2xl))
                .foregroundStyle(tokens.text)
                .padding(.horizontal, HUTokens.spaceLg)
                .padding(.top, HUTokens.spaceLg)

            List {
                ForEach(Array(sessions.enumerated()), id: \.offset) { _, session in
                    HStack {
                        Image(systemName: "bubble.left.and.bubble.right")
                            .foregroundStyle(tokens.accent)
                        VStack(alignment: .leading, spacing: HUTokens.spaceXs) {
                            Text(session.0)
                                .font(.custom("Avenir-Heavy", size: HUTokens.textBase))
                                .foregroundStyle(tokens.text)
                            Text(session.1)
                                .font(.custom("Avenir-Book", size: HUTokens.textXs))
                                .foregroundStyle(tokens.textMuted)
                        }
                        Spacer()
                        Text(session.2)
                            .font(.custom("Avenir-Book", size: HUTokens.textXs))
                            .foregroundStyle(tokens.textMuted)
                    }
                    .accessibilityElement(children: .combine)
                    .accessibilityLabel("\(session.0), \(session.1), \(session.2)")
                }
            }
        }
    }
}

struct MacToolsPane: View {
    let tokens: (bgSurface: Color, surfaceContainer: Color, text: Color, textMuted: Color, accent: Color, success: Color, error: Color)

    private let tools: [(String, String, String)] = [
        ("Shell", "Execute commands", "terminal"),
        ("Read File", "Read file contents", "doc.text"),
        ("Write File", "Write to a file", "doc.badge.plus"),
        ("Fetch", "HTTP request", "network"),
        ("Grep", "Search patterns", "magnifyingglass"),
        ("Browser", "Web navigation", "globe"),
        ("Memory", "Store and recall", "brain"),
        ("Eval", "Evaluate expressions", "function"),
    ]

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: HUTokens.spaceMd) {
                Text("Tools")
                    .font(.custom("Avenir-Heavy", size: HUTokens.text2xl))
                    .foregroundStyle(tokens.text)

                LazyVGrid(columns: Array(repeating: GridItem(.flexible(), spacing: HUTokens.spaceMd), count: 3), spacing: HUTokens.spaceMd) {
                    ForEach(Array(tools.enumerated()), id: \.offset) { _, tool in
                        VStack(alignment: .leading, spacing: HUTokens.spaceSm) {
                            Image(systemName: tool.2)
                                .font(.custom("Avenir-Medium", size: HUTokens.textLg))
                                .foregroundStyle(tokens.accent)
                            Text(tool.0)
                                .font(.custom("Avenir-Heavy", size: HUTokens.textBase))
                                .foregroundStyle(tokens.text)
                            Text(tool.1)
                                .font(.custom("Avenir-Book", size: HUTokens.textXs))
                                .foregroundStyle(tokens.textMuted)
                        }
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(HUTokens.spaceMd)
                        .background(tokens.surfaceContainer)
                        .clipShape(RoundedRectangle(cornerRadius: HUTokens.radiusLg, style: .continuous))
                        .accessibilityElement(children: .combine)
                        .accessibilityLabel("\(tool.0): \(tool.1)")
                    }
                }
            }
            .padding(HUTokens.spaceLg)
        }
    }
}
