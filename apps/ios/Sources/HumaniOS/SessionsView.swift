import SwiftUI
import HumanChatUI

struct SessionsView: View {
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @Environment(\.colorScheme) private var colorScheme
    @EnvironmentObject var connectionManager: ConnectionManager
    @State private var selectedSession: SessionSummary?
    @State private var searchText = ""

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

    private var activeSessions: [SessionSummary] {
        let active = connectionManager.sessions.filter { !$0.isArchived }
        if searchText.isEmpty { return active }
        return active.filter {
            $0.title.localizedCaseInsensitiveContains(searchText) ||
            $0.lastMessage.localizedCaseInsensitiveContains(searchText)
        }
    }

    private var archivedSessions: [SessionSummary] {
        connectionManager.sessions.filter { $0.isArchived }
    }

    private func formatTimestamp(_ date: Date) -> String {
        let formatter = RelativeDateTimeFormatter()
        formatter.unitsStyle = .abbreviated
        return formatter.localizedString(for: date, relativeTo: Date())
    }

    var body: some View {
        NavigationStack {
            List {
                if connectionManager.isConnected {
                ForEach(activeSessions) { session in
                    SessionRow(
                        session: session,
                        tokens: tokens,
                        formatTimestamp: formatTimestamp
                    )
                    .contentShape(Rectangle())
                    .onTapGesture {
#if os(iOS)
                        HUTokens.Haptic.selection.trigger()
#endif
                        selectedSession = session
                    }
#if os(iOS)
                    .contextMenu {
                        Button {
                            UIPasteboard.general.string = [session.title, session.lastMessage].joined(separator: "\n")
                        } label: { Label("Copy", systemImage: "doc.on.doc") }
                        if !session.lastMessage.isEmpty {
                            ShareLink(item: "\(session.title)\n\n\(session.lastMessage)") {
                                Label("Share", systemImage: "square.and.arrow.up")
                            }
                        }
                        Button(role: .destructive) {
                            connectionManager.deleteSession(key: session.id)
                        } label: { Label("Delete", systemImage: "trash") }
                    }
#endif
                    .swipeActions(edge: HorizontalEdge.trailing, allowsFullSwipe: false) {
                        Button(role: .destructive) {
#if os(iOS)
                            HUTokens.Haptic.medium.trigger()
#endif
                            connectionManager.deleteSession(key: session.id)
                        } label: {
                            Label("Delete", systemImage: "trash")
                        }
                        Button {
#if os(iOS)
                            HUTokens.Haptic.light.trigger()
#endif
                            connectionManager.updateSessionArchive(key: session.id, archived: true)
                        } label: {
                            Label("Archive", systemImage: "archivebox")
                        }
                        .tint(tokens.accent)
                    }
                }
                } else {
                    ForEach(0..<5, id: \.self) { _ in
                        SessionRowSkeleton(tokens: tokens)
                    }
                }

                if connectionManager.isConnected && !archivedSessions.isEmpty {
                    Section {
                        ForEach(archivedSessions) { session in
                            SessionRow(
                                session: session,
                                tokens: tokens,
                                formatTimestamp: formatTimestamp
                            )
                            .contentShape(Rectangle())
                            .onTapGesture {
#if os(iOS)
                                HUTokens.Haptic.selection.trigger()
#endif
                                selectedSession = session
                            }
#if os(iOS)
                            .contextMenu {
                                Button {
                                    UIPasteboard.general.string = [session.title, session.lastMessage].joined(separator: "\n")
                                } label: { Label("Copy", systemImage: "doc.on.doc") }
                                if !session.lastMessage.isEmpty {
                                    ShareLink(item: "\(session.title)\n\n\(session.lastMessage)") {
                                        Label("Share", systemImage: "square.and.arrow.up")
                                    }
                                }
                                Button(role: .destructive) {
                                    connectionManager.deleteSession(key: session.id)
                                } label: { Label("Delete", systemImage: "trash") }
                            }
#endif
                            .swipeActions(edge: HorizontalEdge.trailing, allowsFullSwipe: false) {
                                Button(role: .destructive) {
#if os(iOS)
                                    HUTokens.Haptic.medium.trigger()
#endif
                                    connectionManager.deleteSession(key: session.id)
                                } label: {
                                    Label("Delete", systemImage: "trash")
                                }
                                Button {
#if os(iOS)
                                    HUTokens.Haptic.light.trigger()
#endif
                                    connectionManager.updateSessionArchive(key: session.id, archived: false)
                                } label: {
                                    Label("Unarchive", systemImage: "archivebox.fill")
                                }
                                .tint(tokens.accent)
                            }
                        }
                    } header: {
                        Text("Archived")
                            .font(.custom("Avenir-Medium", size: HUTokens.textSm, relativeTo: .subheadline))
                            .foregroundStyle(tokens.textMuted)
                    }
                }
            }
            .listStyle(.plain)
            .background(tokens.bgSurface)
            .scrollContentBackground(.hidden)
            .overlay {
                if connectionManager.isConnected && activeSessions.isEmpty && archivedSessions.isEmpty {
                    VStack(spacing: HUTokens.spaceMd) {
                        Image(systemName: "bubble.left.and.bubble.right")
                            .font(.custom("Avenir-Book", size: HUTokens.textXl))
                            .foregroundStyle(tokens.textMuted)
                        Text("No sessions yet")
                            .font(.custom("Avenir-Medium", size: HUTokens.textLg))
                            .foregroundStyle(tokens.textMuted)
                        Text("Start a conversation to see it here.")
                            .font(.custom("Avenir-Book", size: HUTokens.textSm))
                            .foregroundStyle(tokens.textMuted)
                    }
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
                    .padding(HUTokens.spaceLg)
                }
            }
            .navigationTitle("Sessions")
            .searchable(text: $searchText, prompt: "Search sessions")
            .navigationDestination(item: $selectedSession) { session in
                SessionDetailView(session: session, tokens: tokens)
            }
        }
    }
}

private struct SessionRowSkeleton: View {
    let tokens: (bgSurface: Color, surfaceContainer: Color, surfaceContainerHigh: Color, text: Color, textMuted: Color, accent: Color)

    var body: some View {
        HStack(spacing: HUTokens.spaceMd) {
            RoundedRectangle(cornerRadius: HUTokens.radiusSm)
                .fill(.secondary)
                .frame(width: 36, height: 36)
            VStack(alignment: .leading, spacing: HUTokens.spaceXs) {
                RoundedRectangle(cornerRadius: HUTokens.radiusSm)
                    .fill(.secondary)
                    .frame(width: 120, height: HUTokens.textBase)
                RoundedRectangle(cornerRadius: HUTokens.radiusSm)
                    .fill(.secondary)
                    .frame(width: 180, height: HUTokens.textSm)
            }
            Spacer()
            RoundedRectangle(cornerRadius: HUTokens.radiusSm)
                .fill(.secondary)
                .frame(width: 44, height: HUTokens.textXs)
        }
        .padding(HUTokens.spaceMd)
        .background(.ultraThinMaterial)
        .clipShape(RoundedRectangle(cornerRadius: HUTokens.radiusMd, style: .continuous))
        .listRowInsets(EdgeInsets(top: HUTokens.spaceXs, leading: HUTokens.spaceMd, bottom: HUTokens.spaceXs, trailing: HUTokens.spaceMd))
        .listRowSeparator(.hidden)
        .listRowBackground(Color.clear)
        .redacted(reason: .placeholder)
    }
}

private struct SessionRow: View {
    let session: SessionSummary
    let tokens: (bgSurface: Color, surfaceContainer: Color, surfaceContainerHigh: Color, text: Color, textMuted: Color, accent: Color)
    let formatTimestamp: (Date) -> String

    var body: some View {
        HStack(spacing: HUTokens.spaceMd) {
            Image(systemName: "bubble.left.and.bubble.right")
                .font(.custom("Avenir-Medium", size: HUTokens.textLg, relativeTo: .body))
                .foregroundStyle(tokens.textMuted)
                .frame(width: 36, alignment: .center)

            VStack(alignment: .leading, spacing: HUTokens.spaceXs) {
                Text(session.title)
                    .font(.custom("Avenir-Heavy", size: HUTokens.textBase, relativeTo: .body))
                    .foregroundStyle(tokens.text)
                if !session.lastMessage.isEmpty {
                    Text(session.lastMessage)
                        .font(.custom("Avenir-Book", size: HUTokens.textSm, relativeTo: .subheadline))
                        .foregroundStyle(tokens.textMuted)
                        .lineLimit(1)
                }
            }

            Spacer()

            Text(formatTimestamp(session.timestamp))
                .font(.custom("Avenir-Book", size: HUTokens.textXs, relativeTo: .caption))
                .foregroundStyle(tokens.textMuted)
        }
        .padding(HUTokens.spaceMd)
        .background(.ultraThinMaterial)
        .clipShape(RoundedRectangle(cornerRadius: HUTokens.radiusMd, style: .continuous))
        .listRowInsets(EdgeInsets(top: HUTokens.spaceXs, leading: HUTokens.spaceMd, bottom: HUTokens.spaceXs, trailing: HUTokens.spaceMd))
        .listRowSeparator(.hidden)
        .listRowBackground(Color.clear)
        .accessibilityElement(children: .combine)
        .accessibilityLabel("\(session.title), \(session.lastMessage.isEmpty ? "no messages" : session.lastMessage), \(formatTimestamp(session.timestamp))")
    }
}

private struct SessionDetailView: View {
    let session: SessionSummary
    let tokens: (bgSurface: Color, surfaceContainer: Color, surfaceContainerHigh: Color, text: Color, textMuted: Color, accent: Color)

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: HUTokens.spaceLg) {
                VStack(alignment: .leading, spacing: HUTokens.spaceSm) {
                    Text(session.title)
                        .font(.custom("Avenir-Heavy", size: HUTokens.textXl, relativeTo: .title2))
                        .foregroundStyle(tokens.text)
                    Text(session.timestamp, style: .date)
                        .font(.custom("Avenir-Book", size: HUTokens.textSm, relativeTo: .subheadline))
                        .foregroundStyle(tokens.textMuted)
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(HUTokens.spaceMd)
                .background(.thickMaterial)
                .clipShape(RoundedRectangle(cornerRadius: HUTokens.radiusLg, style: .continuous))

                if !session.lastMessage.isEmpty {
                    Text(session.lastMessage)
                        .font(.custom("Avenir-Book", size: HUTokens.textBase, relativeTo: .body))
                        .foregroundStyle(tokens.text)
                }
            }
            .padding(HUTokens.spaceMd)
        }
        .background(tokens.bgSurface)
        .navigationTitle(session.title)
        .accessibilityLabel("Session: \(session.title)")
#if os(iOS)
        .navigationBarTitleDisplayMode(.inline)
#endif
    }
}

extension SessionSummary: Hashable {
    public func hash(into hasher: inout Hasher) {
        hasher.combine(id)
    }

    public static func == (lhs: SessionSummary, rhs: SessionSummary) -> Bool {
        lhs.id == rhs.id
    }
}
