import SwiftUI
import HumanChatUI

struct OverviewView: View {
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @EnvironmentObject var connectionManager: ConnectionManager
    @Environment(\.colorScheme) private var colorScheme
    @State private var appeared = false

    fileprivate typealias TokenSet = (bgSurface: Color, surfaceContainer: Color, surfaceContainerHigh: Color, text: Color, textMuted: Color, accent: Color, success: Color, error: Color)

    private var tokens: TokenSet {
        let dark: TokenSet = (
            HUTokens.Dark.bgSurface,
            HUTokens.Dark.surfaceContainer,
            HUTokens.Dark.surfaceContainerHigh,
            HUTokens.Dark.text,
            HUTokens.Dark.textMuted,
            HUTokens.Dark.accent,
            HUTokens.Dark.success,
            HUTokens.Dark.error
        )
        let light: TokenSet = (
            HUTokens.Light.bgSurface,
            HUTokens.Light.surfaceContainer,
            HUTokens.Light.surfaceContainerHigh,
            HUTokens.Light.text,
            HUTokens.Light.textMuted,
            HUTokens.Light.accent,
            HUTokens.Light.success,
            HUTokens.Light.error
        )
        return colorScheme == .dark ? dark : light
    }

    private var overviewStatusColor: Color {
        if !connectionManager.isConnected { return tokens.error }
        switch connectionManager.healthStatus {
        case .healthy, .unknown: return tokens.success
        case .degraded: return tokens.accent
        case .unhealthy: return tokens.error
        }
    }

    private var overviewStatusText: String {
        if !connectionManager.isConnected { return "Disconnected" }
        switch connectionManager.healthStatus {
        case .healthy: return "Connected"
        case .degraded: return "Degraded"
        case .unhealthy: return "Unhealthy"
        case .unknown: return "Connected"
        }
    }

    private func formatTimeAgo(_ date: Date) -> String {
        let formatter = RelativeDateTimeFormatter()
        formatter.unitsStyle = .abbreviated
        return formatter.localizedString(for: date, relativeTo: Date())
    }

    private func formatUptime(_ seconds: UInt64) -> String {
        let d = seconds / 86400
        let h = (seconds % 86400) / 3600
        let m = (seconds % 3600) / 60
        if d > 0 { return "\(d)d \(h)h" }
        if h > 0 { return "\(h)h \(m)m" }
        return "\(m)m"
    }

    private var totalSessionTurns: Int {
        connectionManager.sessions.reduce(0) { $0 + $1.messageCount }
    }

    private var messagesStat: String {
        guard connectionManager.isConnected else { return "—" }
        let n = totalSessionTurns
        return n > 0 ? "\(n)" : "0"
    }

    private var channelsStat: String {
        guard connectionManager.isConnected else { return "—" }
        if let c = connectionManager.channelCount { return "\(c)" }
        return "—"
    }

    private var toolsStat: String {
        guard connectionManager.isConnected else { return "—" }
        if !connectionManager.tools.isEmpty { return "\(connectionManager.tools.count)" }
        if let t = connectionManager.toolCount { return "\(t)" }
        return "—"
    }

    private var uptimeStat: String {
        guard connectionManager.isConnected else { return "—" }
        if let u = connectionManager.uptimeSeconds { return formatUptime(u) }
        return "—"
    }

    private var modelStat: String {
        guard connectionManager.isConnected else { return "—" }
        return connectionManager.modelName ?? "—"
    }

    private var hulaProgramsStat: String {
        guard connectionManager.isConnected else { return "—" }
        return "\(connectionManager.hulaProgramCount)"
    }

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: HUTokens.spaceLg) {
                    HStack(spacing: HUTokens.spaceSm) {
                        Circle()
                            .fill(overviewStatusColor)
                            .frame(width: 8, height: 8)
                        Text(overviewStatusText)
                            .font(.custom("Avenir-Medium", size: HUTokens.textSm, relativeTo: .subheadline))
                            .foregroundStyle(tokens.textMuted)
                        Spacer()
                        if !connectionManager.isConnected {
                            Button {
#if os(iOS)
                                HUTokens.Haptic.medium.trigger()
#endif
                                if reduceMotion {
                                    connectionManager.reconnect()
                                } else {
                                    withAnimation(HUTokens.springExpressive) {
                                        connectionManager.reconnect()
                                    }
                                }
                            } label: {
                                Text("Retry")
                                    .font(.custom("Avenir-Medium", size: HUTokens.textSm, relativeTo: .subheadline))
                                    .foregroundStyle(tokens.accent)
                            }
                            .accessibilityLabel("Retry gateway connection")
                        }
                    }
                    .padding(.horizontal)
                    .accessibilityElement(children: .combine)
                    .accessibilityLabel("Gateway \(connectionManager.isConnected ? "connected" : "disconnected")")

                    // Stat cards grid (or skeletons when disconnected)
                    LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: HUTokens.spaceMd) {
                        if connectionManager.isConnected {
                        StatCard(
                            title: "Turns",
                            value: messagesStat,
                            trend: "sessions",
                            trendUp: true,
                            tokens: tokens,
                            appeared: appeared,
                            delay: 0,
                            reduceMotion: reduceMotion,
                            icon: .chat
                        )
                        StatCard(
                            title: "Channels",
                            value: channelsStat,
                            trend: "configured",
                            trendUp: true,
                            tokens: tokens,
                            appeared: appeared,
                            delay: 0.05,
                            reduceMotion: reduceMotion,
                            icon: .grid
                        )
                        StatCard(
                            title: "Tools",
                            value: toolsStat,
                            trend: "catalog",
                            trendUp: true,
                            tokens: tokens,
                            appeared: appeared,
                            delay: 0.1,
                            reduceMotion: reduceMotion,
                            icon: .terminal
                        )
                        StatCard(
                            title: "Model",
                            value: modelStat,
                            trend: uptimeStat != "—" ? "up \(uptimeStat)" : "from gateway",
                            trendUp: true,
                            tokens: tokens,
                            appeared: appeared,
                            delay: 0.15,
                            reduceMotion: reduceMotion,
                            icon: .clock
                        )
                        StatCard(
                            title: "HuLa",
                            value: hulaProgramsStat,
                            trend: "programs",
                            trendUp: true,
                            tokens: tokens,
                            appeared: appeared,
                            delay: 0.2,
                            reduceMotion: reduceMotion,
                            icon: .terminal
                        )
                        StatCard(
                            title: "SOTA",
                            value: connectionManager.isConnected ? "Active" : "—",
                            trend: "features",
                            trendUp: true,
                            tokens: tokens,
                            appeared: appeared,
                            delay: 0.25,
                            reduceMotion: reduceMotion,
                            icon: .grid
                        )
                        StatCard(
                            title: "Security CoT",
                            value: connectionManager.isConnected ? "Auditing" : "—",
                            trend: "chain of thought",
                            trendUp: true,
                            tokens: tokens,
                            appeared: appeared,
                            delay: 0.3,
                            reduceMotion: reduceMotion,
                            icon: .gear
                        )
                        StatCard(
                            title: "Emotion Voice",
                            value: connectionManager.isConnected ? "Active" : "—",
                            trend: "pipeline",
                            trendUp: true,
                            tokens: tokens,
                            appeared: appeared,
                            delay: 0.35,
                            reduceMotion: reduceMotion,
                            icon: .chat
                        )
                        } else {
                            ForEach(0..<8, id: \.self) { _ in
                                StatCardSkeleton()
                            }
                        }
                    }
                    .padding(.horizontal)

                    // Recent activity (or skeleton when disconnected)
                    VStack(alignment: .leading, spacing: HUTokens.spaceSm) {
                        Text("Recent Activity")
                            .font(.custom("Avenir-Heavy", size: HUTokens.textLg, relativeTo: .body))
                            .foregroundStyle(tokens.text)
                            .padding(.horizontal)

                        VStack(spacing: 0) {
                            if !connectionManager.isConnected {
                                ForEach(0..<5, id: \.self) { i in
                                    VStack(spacing: 0) {
                                        ActivityRowSkeleton()
                                        if i < 4 {
                                            Divider()
                                                .background(tokens.textMuted.opacity(HUTokens.opacityOverlayMedium))
                                                .padding(.leading, HUTokens.space2xl)
                                        }
                                    }
                                }
                            } else if connectionManager.recentActivity.isEmpty {
                                Text("No recent activity yet.")
                                    .font(.custom("Avenir-Book", size: HUTokens.textSm, relativeTo: .subheadline))
                                    .foregroundStyle(tokens.textMuted)
                                    .frame(maxWidth: .infinity, alignment: .leading)
                                    .padding(.horizontal, HUTokens.spaceMd)
                                    .padding(.vertical, HUTokens.spaceSm)
                                    .accessibilityLabel("No recent activity")
                            } else {
                                ForEach(Array(connectionManager.recentActivity.enumerated()), id: \.element.id) { index, activity in
                                    ActivityRow(
                                        title: activity.description,
                                        source: activity.type.capitalized,
                                        timeAgo: formatTimeAgo(activity.timestamp),
                                        accent: tokens.accent,
                                        text: tokens.text,
                                        textMuted: tokens.textMuted
                                    )
                                    .opacity(appeared ? 1 : 0)
                                    .offset(y: appeared ? 0 : HUTokens.spaceSm)
                                    .animation(reduceMotion ? nil : HUTokens.springExpressive.delay(min(Double(index) * 0.05, 0.3)), value: appeared)

                                    if index < connectionManager.recentActivity.count - 1 {
                                        Divider()
                                            .background(tokens.textMuted.opacity(HUTokens.opacityOverlayMedium))
                                            .padding(.leading, HUTokens.space2xl)
                                    }
                                }
                            }
                        }
                        .background(.regularMaterial)
                        .clipShape(RoundedRectangle(cornerRadius: HUTokens.radiusLg, style: .continuous))
                    }
                }
                .padding(.vertical)
            }
            .refreshable {
                connectionManager.reconnect()
#if os(iOS)
                let impact = UIImpactFeedbackGenerator(style: .light)
                impact.impactOccurred()
#endif
            }
            .background(tokens.bgSurface)
            .navigationTitle("Overview")
            .onAppear {
                if reduceMotion {
                    appeared = true
                } else {
                    withAnimation(HUTokens.springExpressive) {
                        appeared = true
                    }
                }
            }
        }
    }
}

private struct StatCardSkeleton: View {
    var body: some View {
        VStack(alignment: .leading, spacing: HUTokens.spaceXs) {
            RoundedRectangle(cornerRadius: HUTokens.radiusSm)
                .fill(.secondary)
                .frame(width: 60, height: HUTokens.textXs)
            RoundedRectangle(cornerRadius: HUTokens.radiusSm)
                .fill(.secondary)
                .frame(width: 80, height: HUTokens.textXl)
            RoundedRectangle(cornerRadius: HUTokens.radiusSm)
                .fill(.secondary)
                .frame(width: 40, height: HUTokens.textXs)
        }
        .padding(HUTokens.spaceMd)
        .frame(maxWidth: .infinity, alignment: .leading)
        .redacted(reason: .placeholder)
    }
}

private struct ActivityRowSkeleton: View {
    var body: some View {
        HStack(spacing: HUTokens.spaceMd) {
            Circle()
                .fill(.secondary)
                .frame(width: 32, height: 32)
            VStack(alignment: .leading, spacing: HUTokens.spaceXs) {
                RoundedRectangle(cornerRadius: HUTokens.radiusSm)
                    .fill(.secondary)
                    .frame(width: 140, height: HUTokens.textSm)
                RoundedRectangle(cornerRadius: HUTokens.radiusSm)
                    .fill(.secondary)
                    .frame(width: 50, height: HUTokens.textXs)
            }
            Spacer()
            RoundedRectangle(cornerRadius: HUTokens.radiusSm)
                .fill(.secondary)
                .frame(width: 36, height: HUTokens.textXs)
        }
        .padding(.horizontal, HUTokens.spaceMd)
        .padding(.vertical, HUTokens.spaceXs)
        .redacted(reason: .placeholder)
    }
}

private struct StatCard: View {
    let title: String
    let value: String
    let trend: String
    let trendUp: Bool
    let tokens: OverviewView.TokenSet
    let appeared: Bool
    let delay: Double
    let reduceMotion: Bool
    var icon: PhosphorIconName = .grid

    var body: some View {
        VStack(alignment: .leading, spacing: HUTokens.spaceXs) {
            PhosphorIcon(name: icon, size: HUTokens.textLg)
                .foregroundStyle(tokens.accent)
            Text(title)
                .font(.custom("Avenir-Medium", size: HUTokens.textXs, relativeTo: .caption))
                .foregroundStyle(tokens.textMuted)
            Text(value)
                .font(.custom("Avenir-Black", size: HUTokens.textXl, relativeTo: .title3))
                .foregroundStyle(tokens.text)
                .monospacedDigit()
            HStack(spacing: 4) {
                Image(systemName: trendUp ? "arrow.up.right" : "arrow.down.right")
                    .font(.custom("Avenir-Heavy", size: 10))
                    .foregroundStyle(trendUp ? tokens.accent : tokens.textMuted)
                Text(trend)
                    .font(.custom("Avenir-Medium", size: HUTokens.textXs, relativeTo: .caption))
                    .foregroundStyle(trendUp ? tokens.accent : tokens.textMuted)
            }
        }
        .padding(HUTokens.spaceMd)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(.regularMaterial)
        .clipShape(RoundedRectangle(cornerRadius: HUTokens.radiusMd, style: .continuous))
        .accessibilityElement(children: .combine)
        .accessibilityLabel("\(title): \(value), \(trend)")
        .scaleEffect(appeared ? 1 : 0.95)
        .opacity(appeared ? 1 : 0)
        .animation(reduceMotion ? nil : HUTokens.springExpressive.delay(delay), value: appeared)
    }
}

private struct ActivityRow: View {
    let title: String
    let source: String
    let timeAgo: String
    let accent: Color
    let text: Color
    let textMuted: Color

    var body: some View {
        HStack(spacing: HUTokens.spaceMd) {
            Circle()
                .fill(accent.opacity(HUTokens.opacityOverlayLight))
                .frame(width: 32, height: 32)
                .overlay(
                    Circle()
                        .fill(accent)
                        .frame(width: 8, height: 8)
                )
            VStack(alignment: .leading, spacing: 2) {
                Text(title)
                    .font(.custom("Avenir-Medium", size: HUTokens.textSm, relativeTo: .subheadline))
                    .foregroundStyle(text)
                Text(source)
                    .font(.custom("Avenir-Book", size: HUTokens.textXs, relativeTo: .caption))
                    .foregroundStyle(textMuted)
            }
            Spacer()
            Text(timeAgo)
                .font(.custom("Avenir-Book", size: HUTokens.textXs, relativeTo: .caption))
                .foregroundStyle(textMuted)
                .monospacedDigit()
        }
        .padding(.horizontal, HUTokens.spaceMd)
        .padding(.vertical, HUTokens.spaceXs)
        .accessibilityElement(children: .combine)
        .accessibilityLabel("\(title), \(source), \(timeAgo)")
    }
}
