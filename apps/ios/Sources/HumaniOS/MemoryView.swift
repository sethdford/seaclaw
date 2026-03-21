import SwiftUI
import HumanChatUI

/// Loads memory entries from the gateway (`memory.list`). Stub UI: list + pull-to-refresh.
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
}

struct MemoryView: View {
    @Environment(\.colorScheme) private var colorScheme
    @EnvironmentObject private var connectionManager: ConnectionManager
    @StateObject private var viewModel = MemoryViewModel()

    private var tokens: (text: Color, textMuted: Color, accent: Color, surface: Color) {
        if colorScheme == .dark {
            return (HUTokens.Dark.text, HUTokens.Dark.textMuted, HUTokens.Dark.accent, HUTokens.Dark.bgSurface)
        }
        return (HUTokens.Light.text, HUTokens.Light.textMuted, HUTokens.Light.accent, HUTokens.Light.bgSurface)
    }

    var body: some View {
        NavigationStack {
            memoryContent
                .navigationTitle("Memory")
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
                Text("No memory entries yet")
                    .font(.custom("Avenir-Medium", size: HUTokens.textLg, relativeTo: .body))
                    .foregroundStyle(tokens.text)
                Text("When the gateway stores memories, they appear here.")
                    .font(.custom("Avenir-Book", size: HUTokens.textSm, relativeTo: .body))
                    .foregroundStyle(tokens.textMuted)
                    .multilineTextAlignment(.center)
                    .padding(.horizontal, HUTokens.spaceLg)
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .background(tokens.surface)
        } else {
            List(connectionManager.memoryEntries) { entry in
                VStack(alignment: .leading, spacing: HUTokens.spaceXs) {
                    Text(entry.key)
                        .font(.custom("Avenir-Medium", size: HUTokens.textSm, relativeTo: .body))
                        .foregroundStyle(tokens.text)
                    Text(entry.content)
                        .font(.custom("Avenir-Book", size: HUTokens.textSm, relativeTo: .body))
                        .foregroundStyle(tokens.textMuted)
                        .lineLimit(4)
                    HStack {
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
            }
            .listStyle(.plain)
        }
    }
}
