import SwiftUI
import HumanChatUI

struct ToolItem: Identifiable {
    let id: String
    let name: String
    let description: String
    let icon: String
    let category: ToolCategory
}

enum ToolCategory: String, CaseIterable {
    case system = "System"
    case files = "Files"
    case network = "Network"
    case search = "Search"
    case other = "Other"
}

struct ToolsView: View {
    @Environment(\.colorScheme) private var colorScheme
    @State private var appeared = false

    private var tokens: (bgSurface: Color, surfaceContainer: Color, text: Color, textMuted: Color, accent: Color) {
        if colorScheme == .dark {
            return (
                HUTokens.Dark.bgSurface,
                HUTokens.Dark.surfaceContainer,
                HUTokens.Dark.text,
                HUTokens.Dark.textMuted,
                HUTokens.Dark.accent
            )
        } else {
            return (
                HUTokens.Light.bgSurface,
                HUTokens.Light.surfaceContainer,
                HUTokens.Light.text,
                HUTokens.Light.textMuted,
                HUTokens.Light.accent
            )
        }
    }

    private let toolsByCategory: [ToolCategory: [ToolItem]] = [
        .system: [
            ToolItem(id: "shell", name: "Shell", description: "Execute shell commands", icon: "terminal", category: .system),
            ToolItem(id: "eval", name: "Eval", description: "Evaluate expressions", icon: "function", category: .system),
            ToolItem(id: "time", name: "Time", description: "Get current time", icon: "clock", category: .system),
        ],
        .files: [
            ToolItem(id: "read_file", name: "Read File", description: "Read file contents", icon: "doc.text", category: .files),
            ToolItem(id: "write_file", name: "Write File", description: "Write to a file", icon: "doc.badge.plus", category: .files),
            ToolItem(id: "list_dir", name: "List Directory", description: "List directory contents", icon: "folder", category: .files),
        ],
        .network: [
            ToolItem(id: "fetch", name: "Fetch", description: "HTTP request", icon: "network", category: .network),
            ToolItem(id: "curl", name: "Curl", description: "URL transfer", icon: "arrow.down.circle", category: .network),
        ],
        .search: [
            ToolItem(id: "grep", name: "Grep", description: "Search text patterns", icon: "magnifyingglass", category: .search),
            ToolItem(id: "codebase_search", name: "Codebase Search", description: "Semantic code search", icon: "doc.text.magnifyingglass", category: .search),
        ],
        .other: [
            ToolItem(id: "browser", name: "Browser", description: "Web navigation", icon: "globe", category: .other),
            ToolItem(id: "memory", name: "Memory", description: "Store and recall", icon: "brain", category: .other),
        ],
    ]

    var body: some View {
        NavigationStack {
            ScrollView {
                LazyVStack(alignment: .leading, spacing: HUTokens.spaceXl) {
                    ForEach(ToolCategory.allCases, id: \.self) { category in
                        if let tools = toolsByCategory[category], !tools.isEmpty {
                            categorySection(category: category, tools: tools)
                        }
                    }
                }
                .padding(HUTokens.spaceMd)
            }
            .background(tokens.bgSurface)
            .navigationTitle("Tools")
        }
        .onAppear {
            withAnimation(HUTokens.springExpressive) {
                appeared = true
            }
        }
    }

    @ViewBuilder
    private func categorySection(category: ToolCategory, tools: [ToolItem]) -> some View {
        VStack(alignment: .leading, spacing: HUTokens.spaceSm) {
            Text(category.rawValue)
                .font(.custom("Avenir-Heavy", size: HUTokens.textLg, relativeTo: .body))
                .foregroundStyle(tokens.text)

            LazyVGrid(columns: [
                GridItem(.flexible(), spacing: HUTokens.spaceMd),
                GridItem(.flexible(), spacing: HUTokens.spaceMd),
            ], spacing: HUTokens.spaceMd) {
                ForEach(Array(tools.enumerated()), id: \.element.id) { index, tool in
                    ToolCard(
                        tool: tool,
                        surfaceContainer: tokens.surfaceContainer,
                        text: tokens.text,
                        textMuted: tokens.textMuted,
                        accent: tokens.accent,
                        appeared: appeared,
                        delay: Double(index) * 0.03
                    )
                }
            }
        }
    }
}

private struct ToolCard: View {
    let tool: ToolItem
    let surfaceContainer: Color
    let text: Color
    let textMuted: Color
    let accent: Color
    let appeared: Bool
    let delay: Double

    var body: some View {
        VStack(alignment: .leading, spacing: HUTokens.spaceSm) {
            HStack {
                Image(systemName: tool.icon)
                    .font(.custom("Avenir-Medium", size: HUTokens.textLg, relativeTo: .body))
                    .foregroundStyle(accent)
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
        .background(surfaceContainer)
        .clipShape(RoundedRectangle(cornerRadius: HUTokens.radiusLg, style: .continuous))
        .opacity(appeared ? 1 : 0)
        .scaleEffect(appeared ? 1 : 0.95)
        .animation(HUTokens.springExpressive.delay(delay), value: appeared)
    }
}
