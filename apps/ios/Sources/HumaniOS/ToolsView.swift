import SwiftUI
import HumanChatUI

struct ToolsView: View {
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @Environment(\.colorScheme) private var colorScheme
    @EnvironmentObject var connectionManager: ConnectionManager
    @State private var appeared = false

    private var tokens: (bgSurface: Color, surfaceContainer: Color, surfaceContainerHigh: Color, text: Color, textMuted: Color, accent: Color) {
        if colorScheme == .dark {
            return (
                HUTokens.Dark.bgSurface,
                HUTokens.Dark.surfaceContainer,
                HUTokens.Dark.surfaceContainerHigh,
                HUTokens.Dark.text,
                HUTokens.Dark.textMuted,
                HUTokens.Dark.accent
            )
        } else {
            return (
                HUTokens.Light.bgSurface,
                HUTokens.Light.surfaceContainer,
                HUTokens.Light.surfaceContainerHigh,
                HUTokens.Light.text,
                HUTokens.Light.textMuted,
                HUTokens.Light.accent
            )
        }
    }

    private var toolsByCategory: [(category: String, tools: [ToolInfo])] {
        let grouped = Dictionary(grouping: connectionManager.tools, by: { $0.category })
        let order = ["System", "Files", "Network", "Search", "Other"]
        var result: [(category: String, tools: [ToolInfo])] = []
        for cat in order {
            if let tools = grouped[cat], !tools.isEmpty {
                result.append((category: cat, tools: tools))
            }
        }
        for (cat, tools) in grouped where !order.contains(cat) {
            result.append((category: cat, tools: tools))
        }
        return result
    }

    private var hasAnyTools: Bool {
        !connectionManager.tools.isEmpty
    }

    var body: some View {
        NavigationStack {
            ScrollView {
                LazyVStack(alignment: .leading, spacing: HUTokens.spaceXl) {
                    if connectionManager.isConnected {
                    ForEach(toolsByCategory, id: \.category) { group in
                        categorySection(category: group.category, tools: group.tools)
                    }
                    } else {
                        ToolCardSkeletonGrid()
                    }
                }
                .padding(HUTokens.spaceMd)
            }
            .background(tokens.bgSurface)
            .overlay {
                if connectionManager.isConnected && !hasAnyTools {
                    VStack(spacing: HUTokens.spaceMd) {
                        PhosphorIcon(name: .wrench, size: HUTokens.text3Xl)
                            .foregroundStyle(tokens.textMuted)
                        Text("No tools available")
                            .font(.custom("Avenir-Medium", size: HUTokens.textLg))
                            .foregroundStyle(tokens.textMuted)
                        Text("Tools will appear here when configured.")
                            .font(.custom("Avenir-Book", size: HUTokens.textSm))
                            .foregroundStyle(tokens.textMuted)
                    }
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
                    .padding(HUTokens.spaceLg)
                }
            }
            .navigationTitle("Tools")
        }
        .onAppear {
            if reduceMotion {
                appeared = true
            } else {
                withAnimation(HUTokens.springInteractive) {
                    appeared = true
                }
            }
        }
    }

    @ViewBuilder
    private func categorySection(category: String, tools: [ToolInfo]) -> some View {
        VStack(alignment: .leading, spacing: HUTokens.spaceSm) {
            Text(category)
                .font(.custom("Avenir-Heavy", size: HUTokens.textXl, relativeTo: .body))
                .foregroundStyle(tokens.text)

            LazyVGrid(columns: [
                GridItem(.flexible(), spacing: HUTokens.spaceMd),
                GridItem(.flexible(), spacing: HUTokens.spaceMd),
            ], spacing: HUTokens.spaceMd) {
                ForEach(Array(tools.enumerated()), id: \.element.id) { index, tool in
                    ToolCard(
                        tool: tool,
                        surfaceContainer: tokens.surfaceContainerHigh,
                        text: tokens.text,
                        textMuted: tokens.textMuted,
                        accent: tokens.accent,
                        appeared: appeared,
                        delay: Double(index) * 0.03,
                        reduceMotion: reduceMotion
                    )
                }
            }
        }
    }
}

private struct ToolCardSkeletonGrid: View {
    var body: some View {
        VStack(alignment: .leading, spacing: HUTokens.spaceXl) {
            ForEach(0..<3, id: \.self) { _ in
                VStack(alignment: .leading, spacing: HUTokens.spaceSm) {
                    RoundedRectangle(cornerRadius: HUTokens.radiusSm)
                        .fill(.secondary)
                        .frame(width: 80, height: HUTokens.textBase)
                    LazyVGrid(columns: [
                        GridItem(.flexible(), spacing: HUTokens.spaceMd),
                        GridItem(.flexible(), spacing: HUTokens.spaceMd),
                    ], spacing: HUTokens.spaceMd) {
                        ForEach(0..<4, id: \.self) { _ in
                            ToolCardSkeleton()
                        }
                    }
                }
            }
        }
    }
}

private struct ToolCardSkeleton: View {
    var body: some View {
        VStack(alignment: .leading, spacing: HUTokens.spaceSm) {
            HStack {
                RoundedRectangle(cornerRadius: HUTokens.radiusSm)
                    .fill(.secondary)
                    .frame(width: 24, height: 24)
                Spacer()
            }
            RoundedRectangle(cornerRadius: HUTokens.radiusSm)
                .fill(.secondary)
                .frame(width: 80, height: HUTokens.textBase)
            RoundedRectangle(cornerRadius: HUTokens.radiusSm)
                .fill(.secondary)
                .frame(maxWidth: .infinity)
                .frame(height: HUTokens.textXs)
            RoundedRectangle(cornerRadius: HUTokens.radiusSm)
                .fill(.secondary)
                .frame(width: 120, height: HUTokens.textXs)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(HUTokens.spaceMd)
        .background(.regularMaterial)
        .clipShape(RoundedRectangle(cornerRadius: HUTokens.radiusLg, style: .continuous))
        .redacted(reason: .placeholder)
    }
}

private struct ToolCard: View {
    let tool: ToolInfo
    let surfaceContainer: Color
    let text: Color
    let textMuted: Color
    let accent: Color
    let appeared: Bool
    let delay: Double
    let reduceMotion: Bool

    @ViewBuilder
    private var toolIcon: some View {
        if let phosphor = phosphorIcon(for: tool.name) {
            PhosphorIcon(name: phosphor, size: HUTokens.textLg)
                .foregroundStyle(textMuted)
        } else {
            Image(systemName: "wrench.and.screwdriver")
                .font(.custom("Avenir-Medium", size: HUTokens.textLg, relativeTo: .body))
                .foregroundStyle(textMuted)
        }
    }

    private func phosphorIcon(for name: String) -> PhosphorIconName? {
        let lower = name.lowercased()
        if lower.contains("shell") || lower.contains("terminal") { return .terminal }
        if lower.contains("time") || lower.contains("clock") { return .clock }
        if lower.contains("file") || lower.contains("read") || lower.contains("write") { return .gear }
        if lower.contains("fetch") || lower.contains("curl") || lower.contains("http") { return .terminal }
        if lower.contains("grep") || lower.contains("search") { return .gear }
        return .wrench
    }

    var body: some View {
        VStack(alignment: .leading, spacing: HUTokens.spaceSm) {
            HStack {
                toolIcon
                Spacer()
            }
            Text(tool.name)
                .font(.custom("Avenir-Heavy", size: HUTokens.textBase, relativeTo: .body))
                .foregroundStyle(text)
            Text(tool.description)
                .font(.custom("Avenir-Book", size: HUTokens.textXs, relativeTo: .caption))
                .foregroundStyle(textMuted)
                .lineLimit(2)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(HUTokens.spaceMd)
        .background(.regularMaterial)
        .clipShape(RoundedRectangle(cornerRadius: HUTokens.radiusLg, style: .continuous))
        .accessibilityElement(children: .combine)
        .accessibilityLabel("\(tool.name): \(tool.description)")
        .opacity(appeared ? 1 : 0)
        .scaleEffect(appeared ? 1 : 0.95)
        .animation(reduceMotion ? nil : HUTokens.springInteractive.delay(delay), value: appeared)
    }
}
