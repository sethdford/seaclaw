import SwiftUI
import HumanChatUI

struct OverviewView: View {
    @EnvironmentObject var connectionManager: ConnectionManager
    @Environment(\.colorScheme) private var colorScheme
    @State private var appeared = false

    private var tokens: (bgSurface: Color, surfaceContainer: Color, text: Color, textMuted: Color, accent: Color, success: Color, error: Color) {
        if colorScheme == .dark {
            return (
                HUTokens.Dark.bgSurface,
                HUTokens.Dark.surfaceContainer,
                HUTokens.Dark.text,
                HUTokens.Dark.textMuted,
                HUTokens.Dark.accent,
                HUTokens.Dark.success,
                HUTokens.Dark.error
            )
        } else {
            return (
                HUTokens.Light.bgSurface,
                HUTokens.Light.surfaceContainer,
                HUTokens.Light.text,
                HUTokens.Light.textMuted,
                HUTokens.Light.accent,
                HUTokens.Light.success,
                HUTokens.Light.error
            )
        }
    }

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: HUTokens.spaceLg) {
                    HStack(spacing: HUTokens.spaceSm) {
                        Circle()
                            .fill(connectionManager.isConnected ? tokens.success : tokens.error)
                            .frame(width: 8, height: 8)
                        Text(connectionManager.isConnected ? "Connected" : "Disconnected")
                            .font(.custom("Avenir-Medium", size: HUTokens.textSm, relativeTo: .subheadline))
                            .foregroundStyle(tokens.textMuted)
                        Spacer()
                        if !connectionManager.isConnected {
                            Button {
#if os(iOS)
                                HUTokens.Haptic.medium.trigger()
#endif
                                withAnimation(HUTokens.springExpressive) {
                                    connectionManager.reconnect()
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

                    // Stat cards grid
                    LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: HUTokens.spaceMd) {
                        StatCard(
                            title: "Messages",
                            value: "1,247",
                            trend: "+12%",
                            trendUp: true,
                            tokens: tokens,
                            appeared: appeared,
                            delay: 0
                        )
                        StatCard(
                            title: "Channels",
                            value: "8",
                            trend: "Active",
                            trendUp: true,
                            tokens: tokens,
                            appeared: appeared,
                            delay: 0.05
                        )
                        StatCard(
                            title: "Uptime",
                            value: "99.8%",
                            trend: "30d",
                            trendUp: true,
                            tokens: tokens,
                            appeared: appeared,
                            delay: 0.1
                        )
                        StatCard(
                            title: "Latency",
                            value: "42ms",
                            trend: "avg",
                            trendUp: false,
                            tokens: tokens,
                            appeared: appeared,
                            delay: 0.15
                        )
                    }
                    .padding(.horizontal)

                    // Recent activity
                    VStack(alignment: .leading, spacing: HUTokens.spaceSm) {
                        Text("Recent Activity")
                            .font(.custom("Avenir-Heavy", size: HUTokens.textLg, relativeTo: .body))
                            .foregroundStyle(tokens.text)
                            .padding(.horizontal)

                        VStack(spacing: 0) {
                            ForEach(Array(ActivityRow.activities.enumerated()), id: \.offset) { index, activity in
                                ActivityRow(
                                    title: activity.0,
                                    source: activity.1,
                                    timeAgo: activity.2,
                                    accent: tokens.accent,
                                    text: tokens.text,
                                    textMuted: tokens.textMuted
                                )
                                .opacity(appeared ? 1 : 0)
                                .offset(y: appeared ? 0 : HUTokens.spaceSm)
                                .animation(HUTokens.springExpressive.delay(0.15 + Double(index) * 0.03), value: appeared)

                                if index < ActivityRow.activities.count - 1 {
                                    Divider()
                                        .background(tokens.textMuted.opacity(HUTokens.opacityOverlayMedium))
                                        .padding(.leading, HUTokens.space2xl)
                                }
                            }
                        }
                        .background(tokens.surfaceContainer)
                        .clipShape(RoundedRectangle(cornerRadius: HUTokens.radiusLg, style: .continuous))
                    }
                }
                .padding(.vertical)
            }
            .background(tokens.bgSurface)
            .navigationTitle("Overview")
            .onAppear {
                withAnimation(HUTokens.springExpressive) {
                    appeared = true
                }
            }
        }
    }
}

private struct StatCard: View {
    let title: String
    let value: String
    let trend: String
    let trendUp: Bool
    let tokens: (bgSurface: Color, surfaceContainer: Color, text: Color, textMuted: Color, accent: Color, success: Color, error: Color)
    let appeared: Bool
    let delay: Double

    var body: some View {
        VStack(alignment: .leading, spacing: HUTokens.spaceXs) {
            Text(title)
                .font(.custom("Avenir-Medium", size: HUTokens.textXs, relativeTo: .caption))
                .foregroundStyle(tokens.textMuted)
            Text(value)
                .font(.custom("Avenir-Black", size: HUTokens.textXl, relativeTo: .title3))
                .foregroundStyle(tokens.text)
                .monospacedDigit()
            HStack(spacing: 4) {
                Image(systemName: trendUp ? "arrow.up.right" : "arrow.down.right")
                    .font(.system(size: 10, weight: .bold))
                    .foregroundStyle(trendUp ? tokens.accent : tokens.textMuted)
                Text(trend)
                    .font(.custom("Avenir-Medium", size: HUTokens.textXs, relativeTo: .caption))
                    .foregroundStyle(trendUp ? tokens.accent : tokens.textMuted)
            }
        }
        .padding(HUTokens.spaceMd)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(tokens.surfaceContainer)
        .clipShape(RoundedRectangle(cornerRadius: HUTokens.radiusMd, style: .continuous))
        .accessibilityElement(children: .combine)
        .accessibilityLabel("\(title): \(value), \(trend)")
        .scaleEffect(appeared ? 1 : 0.95)
        .opacity(appeared ? 1 : 0)
        .animation(HUTokens.springExpressive.delay(delay), value: appeared)
    }
}

private struct ActivityRow: View {
    let title: String
    let source: String
    let timeAgo: String
    let accent: Color
    let text: Color
    let textMuted: Color

    static let activities: [(String, String, String)] = [
        ("Chat message received", "Slack", "2m ago"),
        ("Tool executed: web_search", "Agent", "5m ago"),
        ("Session started", "CLI", "12m ago"),
        ("Memory consolidated", "System", "1h ago"),
        ("Channel connected", "Discord", "2h ago"),
    ]

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
