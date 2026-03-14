import SwiftUI
import HumanChatUI

struct OverviewView: View {
    @Environment(\.colorScheme) private var colorScheme
    @EnvironmentObject var connectionManager: ConnectionManager
    @State private var appeared = false

    private var tokens: (bgSurface: Color, surfaceContainer: Color, text: Color, textMuted: Color, success: Color, error: Color, accent: Color) {
        if colorScheme == .dark {
            return (
                HUTokens.Dark.bgSurface,
                HUTokens.Dark.surfaceContainer,
                HUTokens.Dark.text,
                HUTokens.Dark.textMuted,
                HUTokens.Dark.success,
                HUTokens.Dark.error,
                HUTokens.Dark.accent
            )
        } else {
            return (
                HUTokens.Light.bgSurface,
                HUTokens.Light.surfaceContainer,
                HUTokens.Light.text,
                HUTokens.Light.textMuted,
                HUTokens.Light.success,
                HUTokens.Light.error,
                HUTokens.Light.accent
            )
        }
    }

    private let stats = [
        ("Providers", 9, "cpu"),
        ("Channels", 34, "bubble.left.and.bubble.right"),
        ("Tools", 67, "wrench.and.screwdriver"),
    ]

    private let recentActivity: [(String, String, String)] = [
        ("Chat message sent", "2 min ago", "bubble.left.fill"),
        ("Tool: shell executed", "5 min ago", "terminal"),
        ("Connected to gateway", "12 min ago", "antenna.radiowaves.left.and.right"),
        ("Session started", "1 hr ago", "clock"),
    ]

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: HUTokens.spaceLg) {
                    connectionCard
                    statsSection
                    recentActivitySection
                }
                .padding(HUTokens.spaceMd)
            }
            .background(tokens.bgSurface)
            .navigationTitle("Overview")
            .refreshable {
                try? await Task.sleep(nanoseconds: 500_000_000)
            }
        }
        .onAppear {
            withAnimation(HUTokens.springExpressive) {
                appeared = true
            }
        }
    }

    @ViewBuilder
    private var connectionCard: some View {
        HStack(spacing: HUTokens.spaceMd) {
            Image(systemName: connectionManager.isConnected ? "antenna.radiowaves.left.and.right" : "wifi.slash")
                .font(.custom("Avenir-Heavy", size: HUTokens.textXl, relativeTo: .title3))
                .foregroundStyle(connectionManager.isConnected ? tokens.success : tokens.error)
                .frame(width: 44, height: 44)
                .background((connectionManager.isConnected ? tokens.success : tokens.error).opacity(0.15))
                .clipShape(RoundedRectangle(cornerRadius: HUTokens.radiusLg, style: .continuous))

            VStack(alignment: .leading, spacing: HUTokens.spaceXs) {
                Text(connectionManager.isConnected ? "Connected" : "Disconnected")
                    .font(.custom("Avenir-Heavy", size: HUTokens.textLg, relativeTo: .body))
                    .foregroundStyle(tokens.text)
                Text(connectionManager.gatewayURL)
                    .font(.custom("Avenir-Book", size: HUTokens.textSm, relativeTo: .subheadline))
                    .foregroundStyle(tokens.textMuted)
                    .lineLimit(1)
                    .truncationMode(.middle)
            }
            Spacer()
        }
        .padding(HUTokens.spaceMd)
        .background(tokens.surfaceContainer)
        .clipShape(RoundedRectangle(cornerRadius: HUTokens.radiusLg, style: .continuous))
        .accessibilityElement(children: .combine)
        .accessibilityLabel("Gateway \(connectionManager.isConnected ? "connected" : "disconnected") at \(connectionManager.gatewayURL)")
        .opacity(appeared ? 1 : 0)
        .offset(y: appeared ? 0 : 12)
        .animation(HUTokens.springExpressive.delay(0), value: appeared)
        .animation(HUTokens.springExpressive, value: connectionManager.isConnected)
    }

    @ViewBuilder
    private var statsSection: some View {
        VStack(alignment: .leading, spacing: HUTokens.spaceSm) {
            Text("Stats")
                .font(.custom("Avenir-Heavy", size: HUTokens.textLg, relativeTo: .body))
                .foregroundStyle(tokens.text)

            LazyVGrid(columns: [
                GridItem(.flexible(), spacing: HUTokens.spaceMd),
                GridItem(.flexible(), spacing: HUTokens.spaceMd),
                GridItem(.flexible(), spacing: HUTokens.spaceMd),
            ], spacing: HUTokens.spaceMd) {
                ForEach(Array(stats.enumerated()), id: \.offset) { index, stat in
                    StatCard(
                        title: stat.0,
                        value: "\(stat.1)",
                        icon: stat.2,
                        surfaceContainer: tokens.surfaceContainer,
                        text: tokens.text,
                        textMuted: tokens.textMuted,
                        accent: tokens.accent,
                        appeared: appeared,
                        delay: Double(index) * 0.05
                    )
                }
            }
        }
    }

    @ViewBuilder
    private var recentActivitySection: some View {
        VStack(alignment: .leading, spacing: HUTokens.spaceSm) {
            Text("Recent Activity")
                .font(.custom("Avenir-Heavy", size: HUTokens.textLg, relativeTo: .body))
                .foregroundStyle(tokens.text)

            VStack(spacing: 0) {
                ForEach(Array(recentActivity.enumerated()), id: \.offset) { index, item in
                    HStack(spacing: HUTokens.spaceMd) {
                        Image(systemName: item.2)
                            .font(.custom("Avenir-Medium", size: HUTokens.textBase, relativeTo: .body))
                            .foregroundStyle(tokens.accent)
                            .frame(width: 32, alignment: .center)
                        VStack(alignment: .leading, spacing: HUTokens.spaceXs) {
                            Text(item.0)
                                .font(.custom("Avenir-Book", size: HUTokens.textBase, relativeTo: .body))
                                .foregroundStyle(tokens.text)
                            Text(item.1)
                                .font(.custom("Avenir-Book", size: HUTokens.textXs, relativeTo: .caption))
                                .foregroundStyle(tokens.textMuted)
                        }
                        Spacer()
                    }
                    .padding(HUTokens.spaceMd)
                    .accessibilityElement(children: .combine)
                    .accessibilityLabel("\(item.0), \(item.1)")
                    .opacity(appeared ? 1 : 0)
                    .offset(y: appeared ? 0 : 8)
                    .animation(HUTokens.springExpressive.delay(0.15 + Double(index) * 0.03), value: appeared)

                    if index < recentActivity.count - 1 {
                        Divider()
                            .background(tokens.textMuted.opacity(0.3))
                            .padding(.leading, 48)
                    }
                }
            }
            .background(tokens.surfaceContainer)
            .clipShape(RoundedRectangle(cornerRadius: HUTokens.radiusLg, style: .continuous))
        }
    }
}

private struct StatCard: View {
    let title: String
    let value: String
    let icon: String
    let surfaceContainer: Color
    let text: Color
    let textMuted: Color
    let accent: Color
    let appeared: Bool
    let delay: Double

    var body: some View {
        VStack(alignment: .leading, spacing: HUTokens.spaceSm) {
            Image(systemName: icon)
                .font(.custom("Avenir-Medium", size: HUTokens.textLg, relativeTo: .body))
                .foregroundStyle(accent)
            Text(value)
                .font(.custom("Avenir-Heavy", size: HUTokens.textXl, relativeTo: .title3))
                .foregroundStyle(text)
            Text(title)
                .font(.custom("Avenir-Book", size: HUTokens.textXs, relativeTo: .caption))
                .foregroundStyle(textMuted)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(HUTokens.spaceMd)
        .background(surfaceContainer)
        .clipShape(RoundedRectangle(cornerRadius: HUTokens.radiusLg, style: .continuous))
        .accessibilityElement(children: .combine)
        .accessibilityLabel("\(title): \(value)")
        .opacity(appeared ? 1 : 0)
        .scaleEffect(appeared ? 1 : 0.9)
        .animation(HUTokens.springExpressive.delay(delay), value: appeared)
    }
}
