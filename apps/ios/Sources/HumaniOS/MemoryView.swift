import SwiftUI
import HumanChatUI
import HumanProtocol

/// Loads memory entries from the gateway (`memory.list`).
@MainActor
final class MemoryViewModel: ObservableObject {
    @Published var isLoading = false
    @Published var errorMessage: String?

    func refresh(connectionManager: ConnectionManager) {
        isLoading = true
        errorMessage = nil
        connectionManager.fetchMemoryList { [weak self] result in
            Task { @MainActor in
                guard let self else { return }
                self.isLoading = false
                switch result {
                case .success:
                    break
                case .failure(let err):
                    self.errorMessage = err.localizedDescription
                }
            }
        }
    }

    func deleteEntry(_ entry: MemoryEntrySummary, connectionManager: ConnectionManager) {
        Task {
            _ = try? await connectionManager.request(
                method: "memory.forget",
                params: ["key": AnyCodable(entry.key)]
            )
            refresh(connectionManager: connectionManager)
        }
    }
}

struct MemoryView: View {
    @Environment(\.colorScheme) private var colorScheme
    @EnvironmentObject private var connectionManager: ConnectionManager
    @StateObject private var viewModel = MemoryViewModel()
    @State private var searchText = ""

    private var tokens: (text: Color, textMuted: Color, textFaint: Color, accent: Color, surface: Color) {
        if colorScheme == .dark {
            return (HUTokens.Dark.text, HUTokens.Dark.textMuted, HUTokens.Dark.textFaint,
                    HUTokens.Dark.accent, HUTokens.Dark.bgSurface)
        }
        return (HUTokens.Light.text, HUTokens.Light.textMuted, HUTokens.Light.textFaint,
                HUTokens.Light.accent, HUTokens.Light.bgSurface)
    }

    private var filteredEntries: [MemoryEntrySummary] {
        guard !searchText.isEmpty else { return connectionManager.memoryEntries }
        let query = searchText.lowercased()
        return connectionManager.memoryEntries.filter {
            $0.key.lowercased().contains(query) ||
            $0.content.lowercased().contains(query) ||
            $0.category.lowercased().contains(query)
        }
    }

    var body: some View {
        NavigationStack {
            memoryContent
                .navigationTitle("Memory")
                .searchable(text: $searchText, prompt: "Search memories")
                .tint(tokens.accent)
                .task {
                    connectionManager.connect()
                    viewModel.refresh(connectionManager: connectionManager)
                }
                .refreshable {
                    viewModel.refresh(connectionManager: connectionManager)
                }
        }
    }

    @ViewBuilder
    private var memoryContent: some View {
        if let err = viewModel.errorMessage {
            VStack(spacing: HUTokens.spaceMd) {
                Image(systemName: "exclamationmark.triangle")
                    .font(.system(size: HUTokens.textXl * 1.5))
                    .foregroundStyle(tokens.textMuted)
                Text("Could not load memories")
                    .font(.custom("Avenir-Medium", size: HUTokens.textLg, relativeTo: .body))
                    .foregroundStyle(tokens.text)
                Text(err)
                    .font(.custom("Avenir-Book", size: HUTokens.textSm, relativeTo: .body))
                    .foregroundStyle(tokens.textMuted)
                    .multilineTextAlignment(.center)
                    .padding(.horizontal, HUTokens.spaceLg)
                Button("Retry") {
                    viewModel.refresh(connectionManager: connectionManager)
                }
                .font(.custom("Avenir-Medium", size: HUTokens.textSm, relativeTo: .body))
                .foregroundStyle(tokens.accent)
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .background(tokens.surface)
        } else if viewModel.isLoading && connectionManager.memoryEntries.isEmpty {
            ProgressView()
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                .background(tokens.surface)
        } else if connectionManager.memoryEntries.isEmpty {
            VStack(spacing: HUTokens.spaceMd) {
                Image(systemName: "brain")
                    .font(.system(size: HUTokens.textHero))
                    .foregroundStyle(tokens.textMuted)
                Text("No memories yet")
                    .font(.custom("Avenir-Medium", size: HUTokens.textLg, relativeTo: .body))
                    .foregroundStyle(tokens.text)
                Text("Memories will appear as you chat")
                    .font(.custom("Avenir-Book", size: HUTokens.textSm, relativeTo: .body))
                    .foregroundStyle(tokens.textMuted)
                    .multilineTextAlignment(.center)
                    .padding(.horizontal, HUTokens.spaceLg)
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .background(tokens.surface)
        } else {
            List {
                ForEach(filteredEntries) { entry in
                    VStack(alignment: .leading, spacing: HUTokens.spaceXs) {
                        HStack(alignment: .top) {
                            Text(entry.key)
                                .font(.custom("Avenir-Medium", size: HUTokens.textSm, relativeTo: .body))
                                .foregroundStyle(tokens.text)
                            Spacer()
                            Text(entry.timestamp)
                                .font(.custom("Avenir-Book", size: HUTokens.textXs, relativeTo: .caption2))
                                .foregroundStyle(tokens.textFaint)
                        }
                        Text(entry.content)
                            .font(.custom("Avenir-Book", size: HUTokens.textSm, relativeTo: .body))
                            .foregroundStyle(tokens.textMuted)
                            .lineLimit(2)
                        HStack(spacing: HUTokens.spaceXs) {
                            Text(entry.category)
                                .font(.custom("Avenir-Book", size: HUTokens.textXs, relativeTo: .caption2))
                                .foregroundStyle(tokens.accent)
                            if !entry.source.isEmpty {
                                Text("· \(entry.source)")
                                    .font(.custom("Avenir-Book", size: HUTokens.textXs, relativeTo: .caption2))
                                    .foregroundStyle(tokens.textMuted)
                            }
                        }
                    }
                    .padding(.vertical, HUTokens.spaceXs)
                    .accessibilityElement(children: .combine)
                    .swipeActions(edge: .trailing, allowsFullSwipe: true) {
                        Button(role: .destructive) {
                            viewModel.deleteEntry(entry, connectionManager: connectionManager)
                        } label: {
                            Label("Delete", systemImage: "trash")
                        }
                    }
                }
            }
            .listStyle(.plain)
        }
    }
}
