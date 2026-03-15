import SwiftUI
import HumanChatUI

/// Wraps content in a lazy container so it is built only when first displayed.
private struct LazyDetailView<Content: View>: View {
    let build: () -> Content
    init(_ build: @autoclosure @escaping () -> Content) { self.build = build }
    var body: some View { build() }
}

struct DashboardView: View {
    @EnvironmentObject var status: StatusViewModel
    @Environment(\.colorScheme) private var colorScheme

    private var tokens: (bgSurface: Color, surfaceContainer: Color, surfaceContainerHigh: Color, text: Color, textMuted: Color, accent: Color, success: Color, error: Color) {
        if colorScheme == .dark {
            return (HUTokens.Dark.bgSurface, HUTokens.Dark.surfaceContainer, HUTokens.Dark.surfaceContainerHigh, HUTokens.Dark.text, HUTokens.Dark.textMuted, HUTokens.Dark.accent, HUTokens.Dark.success, HUTokens.Dark.error)
        } else {
            return (HUTokens.Light.bgSurface, HUTokens.Light.surfaceContainer, HUTokens.Light.surfaceContainerHigh, HUTokens.Light.text, HUTokens.Light.textMuted, HUTokens.Light.accent, HUTokens.Light.success, HUTokens.Light.error)
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
        .onChange(of: status.selectedTab) { _, tab in
            if tab == .chat { status.connectIfNeeded() }
        }
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
        .focusSection()
        .toolbar {
            ToolbarItem(placement: .automatic) {
                HStack(spacing: HUTokens.spaceXs) {
                    Circle()
                        .fill(status.isServiceRunning ? tokens.success : tokens.error)
                        .frame(width: HUTokens.spaceSm, height: HUTokens.spaceSm)
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
            LazyDetailView(MacOverviewPane(tokens: tokens, status: status))
                .focusSection()
        case .chat:
            LazyDetailView(MacChatPane(tokens: tokens))
                .focusSection()
        case .sessions:
            LazyDetailView(MacSessionsPane(tokens: tokens))
                .focusSection()
        case .tools:
            LazyDetailView(MacToolsPane(tokens: tokens))
                .focusSection()
        case .settings:
            LazyDetailView(SettingsView())
                .focusSection()
        }
    }
}

struct MacOverviewPane: View {
    let tokens: (bgSurface: Color, surfaceContainer: Color, surfaceContainerHigh: Color, text: Color, textMuted: Color, accent: Color, success: Color, error: Color)
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
                    .font(.custom("Avenir-Heavy", size: 24))
                    .kerning(-0.5)
                    .foregroundStyle(tokens.text)

                HStack(spacing: HUTokens.spaceMd) {
                    Circle()
                        .fill(status.isGatewayConnected ? tokens.success : tokens.error)
                        .frame(width: HUTokens.spaceMd, height: HUTokens.spaceMd)
                    Text(status.isGatewayConnected ? "Gateway Connected" : "Gateway Disconnected")
                        .font(.custom("Avenir-Medium", size: HUTokens.textBase))
                        .foregroundStyle(tokens.text)
                    Spacer()
                    Text(status.gatewayURL)
                        .font(.custom("Avenir-Book", size: HUTokens.textSm))
                        .foregroundStyle(tokens.textMuted)
                }
                .padding(HUTokens.spaceMd)
                .background(tokens.surfaceContainerHigh)
                .clipShape(RoundedRectangle(cornerRadius: HUTokens.radiusLg, style: .continuous))
                .accessibilityElement(children: .contain)
                .accessibilityLabel("Gateway \(status.isGatewayConnected ? "connected" : "disconnected") at \(status.gatewayURL)")

                LazyVGrid(columns: Array(repeating: GridItem(.flexible(), spacing: HUTokens.spaceMd), count: 4), spacing: HUTokens.spaceMd) {
                    ForEach(Array(stats.enumerated()), id: \.offset) { index, stat in
                        VStack(alignment: .leading, spacing: HUTokens.spaceSm) {
                            Image(systemName: stat.2)
                                .font(.custom("Avenir-Medium", size: HUTokens.textLg))
                                .foregroundStyle(tokens.accent)
                            Text(stat.1)
                                .font(.custom("Avenir-Heavy", size: 28))
                                .kerning(-0.5)
                                .foregroundStyle(tokens.text)
                            Text(stat.0)
                                .font(.custom("Avenir-Book", size: HUTokens.textXs))
                                .foregroundStyle(tokens.textMuted)
                        }
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(HUTokens.spaceMd)
                        .background(tokens.surfaceContainerHigh)
                        .clipShape(RoundedRectangle(cornerRadius: HUTokens.radiusLg, style: .continuous))
                        .accessibilityElement(children: .contain)
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
                        Text("Uptime")
                            .font(.custom("Avenir-Book", size: HUTokens.textSm))
                            .foregroundStyle(tokens.textMuted)
                        Text("99.8%")
                            .font(.custom("Avenir-Heavy", size: HUTokens.textBase))
                            .foregroundStyle(tokens.text)
                    }
                    VStack(alignment: .leading, spacing: HUTokens.spaceXs) {
                        Text("Model")
                            .font(.custom("Avenir-Book", size: HUTokens.textSm))
                            .foregroundStyle(tokens.textMuted)
                        Text(status.isGatewayConnected ? "claude-sonnet" : "—")
                            .font(.custom("Avenir-Heavy", size: HUTokens.textBase))
                            .foregroundStyle(tokens.text)
                    }
                    VStack(alignment: .leading, spacing: HUTokens.spaceXs) {
                        Text("Memory entries")
                            .font(.custom("Avenir-Book", size: HUTokens.textSm))
                            .foregroundStyle(tokens.textMuted)
                        Text("1,247")
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
                .background(tokens.surfaceContainerHigh)
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
    let tokens: (bgSurface: Color, surfaceContainer: Color, surfaceContainerHigh: Color, text: Color, textMuted: Color, accent: Color, success: Color, error: Color)

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
    let tokens: (bgSurface: Color, surfaceContainer: Color, surfaceContainerHigh: Color, text: Color, textMuted: Color, accent: Color, success: Color, error: Color)

    private let sessions: [(String, String, Int, String)] = [
        ("CLI conversation", "2 min ago", 12, "I'll check the forecast for you."),
        ("Telegram support", "1 hour ago", 8, "Here's my suggested refactor..."),
        ("Discord sync", "3 hours ago", 24, "Based on your preferences..."),
    ]

    var body: some View {
        VStack(alignment: .leading, spacing: HUTokens.spaceMd) {
            Text("Sessions")
                .font(.custom("Avenir-Heavy", size: HUTokens.textXl))
                .foregroundStyle(tokens.text)
                .padding(.horizontal, HUTokens.spaceLg)
                .padding(.top, HUTokens.spaceLg)

            List {
                ForEach(Array(sessions.enumerated()), id: \.offset) { _, session in
                    HStack {
                        Image(systemName: "bubble.left.and.bubble.right")
                            .foregroundStyle(tokens.accent)
                            .accessibilityHidden(true)
                        VStack(alignment: .leading, spacing: HUTokens.spaceXs) {
                            HStack(spacing: HUTokens.spaceXs) {
                                Text(session.0)
                                    .font(.custom("Avenir-Heavy", size: HUTokens.textBase))
                                    .foregroundStyle(tokens.text)
                                Text("\(session.2)")
                                    .font(.custom("Avenir-Medium", size: HUTokens.textXs, relativeTo: .caption))
                                    .foregroundStyle(tokens.accent)
                                    .padding(.horizontal, HUTokens.spaceXs)
                                    .padding(.vertical, 2)
                                    .background(tokens.accent.opacity(HUTokens.opacityOverlayLight))
                                    .clipShape(Capsule())
                            }
                            Text(String(session.3.prefix(40)) + (session.3.count > 40 ? "…" : ""))
                                .font(.custom("Avenir-Book", size: HUTokens.textXs))
                                .foregroundStyle(tokens.textMuted)
                                .lineLimit(1)
                            Text(session.1)
                                .font(.custom("Avenir-Book", size: HUTokens.textXs))
                                .foregroundStyle(tokens.textMuted)
                        }
                        Spacer()
                        Text("\(session.2) msgs")
                            .font(.custom("Avenir-Book", size: HUTokens.textXs))
                            .foregroundStyle(tokens.textMuted)
                    }
                    .accessibilityElement(children: .combine)
                    .accessibilityLabel("\(session.0), \(session.2) messages, \(session.1), preview: \(session.3)")
                }
            }
        }
    }
}

struct MacToolsPane: View {
    let tokens: (bgSurface: Color, surfaceContainer: Color, surfaceContainerHigh: Color, text: Color, textMuted: Color, accent: Color, success: Color, error: Color)

    private let tools: [(String, String, String, String)] = [
        ("Shell", "Execute commands", "terminal", "2m ago"),
        ("Read File", "Read file contents", "doc.text", "124 uses"),
        ("Write File", "Write to a file", "doc.badge.plus", "5m ago"),
        ("Fetch", "HTTP request", "network", "89 uses"),
        ("Grep", "Search patterns", "magnifyingglass", "1h ago"),
        ("Browser", "Web navigation", "globe", "—"),
        ("Memory", "Store and recall", "brain", "32 uses"),
        ("Eval", "Evaluate expressions", "function", "—"),
    ]

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: HUTokens.spaceMd) {
                Text("Tools")
                    .font(.custom("Avenir-Heavy", size: HUTokens.textXl))
                    .foregroundStyle(tokens.text)

                LazyVGrid(columns: Array(repeating: GridItem(.flexible(), spacing: HUTokens.spaceMd), count: 3), spacing: HUTokens.spaceMd) {
                    ForEach(Array(tools.enumerated()), id: \.offset) { _, tool in
                        VStack(alignment: .leading, spacing: HUTokens.spaceSm) {
                            Image(systemName: tool.2)
                                .font(.custom("Avenir-Medium", size: HUTokens.textLg))
                                .foregroundStyle(tokens.accent)
                                .accessibilityHidden(true)
                            Text(tool.0)
                                .font(.custom("Avenir-Heavy", size: HUTokens.textBase))
                                .foregroundStyle(tokens.text)
                            Text(tool.1)
                                .font(.custom("Avenir-Book", size: HUTokens.textXs))
                                .foregroundStyle(tokens.textMuted)
                            Text(tool.3)
                                .font(.custom("Avenir-Book", size: HUTokens.textXs))
                                .foregroundStyle(tokens.textMuted)
                        }
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(HUTokens.spaceMd)
                        .background(tokens.surfaceContainerHigh)
                        .clipShape(RoundedRectangle(cornerRadius: HUTokens.radiusLg, style: .continuous))
                        .accessibilityElement(children: .contain)
                        .accessibilityLabel("\(tool.0): \(tool.1), last used \(tool.3)")
                    }
                }
            }
            .padding(HUTokens.spaceLg)
        }
    }
}

private struct SparklineView: View {
    let data: [CGFloat]
    let color: Color

    var body: some View {
        GeometryReader { geo in
            let w = geo.size.width / CGFloat(max(1, data.count - 1))
            let maxVal = data.max() ?? 1
            let minVal = data.min() ?? 0
            let range = max(maxVal - minVal, 0.001)
            Path { path in
                for (i, v) in data.enumerated() {
                    let x = CGFloat(i) * w
                    let y = geo.size.height * (1 - (v - minVal) / range)
                    if i == 0 {
                        path.move(to: CGPoint(x: x, y: y))
                    } else {
                        path.addLine(to: CGPoint(x: x, y: y))
                    }
                }
            }
            .stroke(color, style: StrokeStyle(lineWidth: 1.5, lineCap: .round, lineJoin: .round))
        }
    }
}
