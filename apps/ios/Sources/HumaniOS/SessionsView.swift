import SwiftUI
import HumanChatUI

struct ChatSession: Identifiable {
    let id: UUID
    let title: String
    let lastMessage: String?
    let timestamp: Date
    var isArchived: Bool

    init(id: UUID = UUID(), title: String, lastMessage: String? = nil, timestamp: Date, isArchived: Bool = false) {
        self.id = id
        self.title = title
        self.lastMessage = lastMessage
        self.timestamp = timestamp
        self.isArchived = isArchived
    }
}

struct SessionsView: View {
    @Environment(\.colorScheme) private var colorScheme
    @State private var sessions: [ChatSession] = [
        ChatSession(title: "Weather & Planning", lastMessage: "I'll check the forecast for you.", timestamp: Date().addingTimeInterval(-300)),
        ChatSession(title: "Code Review Help", lastMessage: "Here's my suggested refactor...", timestamp: Date().addingTimeInterval(-3600)),
        ChatSession(title: "Travel Ideas", lastMessage: "Based on your preferences...", timestamp: Date().addingTimeInterval(-86400)),
        ChatSession(title: "Quick Questions", lastMessage: "Sure, I can help with that.", timestamp: Date().addingTimeInterval(-172800), isArchived: true),
    ]
    @State private var selectedSession: ChatSession?

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

    private var activeSessions: [ChatSession] {
        sessions.filter { !$0.isArchived }
    }

    private var archivedSessions: [ChatSession] {
        sessions.filter { $0.isArchived }
    }

    private func formatTimestamp(_ date: Date) -> String {
        let formatter = RelativeDateTimeFormatter()
        formatter.unitsStyle = .abbreviated
        return formatter.localizedString(for: date, relativeTo: Date())
    }

    var body: some View {
        NavigationStack {
            List {
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
                    .swipeActions(edge: .trailing, allowsFullSwipe: false) {
                        Button(role: .destructive) {
#if os(iOS)
                            HUTokens.Haptic.medium.trigger()
#endif
                            withAnimation(HUTokens.springExpressive) {
                                sessions.removeAll { $0.id == session.id }
                            }
                        } label: {
                            Label("Delete", systemImage: "trash")
                        }
                        Button {
#if os(iOS)
                            HUTokens.Haptic.light.trigger()
#endif
                            withAnimation(HUTokens.springExpressive) {
                                if let idx = sessions.firstIndex(where: { $0.id == session.id }) {
                                    sessions[idx].isArchived = true
                                }
                            }
                        } label: {
                            Label("Archive", systemImage: "archivebox")
                        }
                        .tint(tokens.accent)
                    }
                }

                if !archivedSessions.isEmpty {
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
                            .swipeActions(edge: .trailing, allowsFullSwipe: false) {
                                Button(role: .destructive) {
#if os(iOS)
                                    HUTokens.Haptic.medium.trigger()
#endif
                                    withAnimation(HUTokens.springExpressive) {
                                        sessions.removeAll { $0.id == session.id }
                                    }
                                } label: {
                                    Label("Delete", systemImage: "trash")
                                }
                                Button {
#if os(iOS)
                                    HUTokens.Haptic.light.trigger()
#endif
                                    withAnimation(HUTokens.springExpressive) {
                                        if let idx = sessions.firstIndex(where: { $0.id == session.id }) {
                                            sessions[idx].isArchived = false
                                        }
                                    }
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
            .navigationTitle("Sessions")
            .navigationDestination(item: $selectedSession) { session in
                SessionDetailView(session: session, tokens: tokens)
            }
        }
    }
}

private struct SessionRow: View {
    let session: ChatSession
    let tokens: (bgSurface: Color, surfaceContainer: Color, text: Color, textMuted: Color, accent: Color)
    let formatTimestamp: (Date) -> String

    var body: some View {
        HStack(spacing: HUTokens.spaceMd) {
            Image(systemName: "bubble.left.and.bubble.right")
                .font(.custom("Avenir-Medium", size: HUTokens.textLg, relativeTo: .body))
                .foregroundStyle(tokens.accent)
                .frame(width: 36, alignment: .center)

            VStack(alignment: .leading, spacing: HUTokens.spaceXs) {
                Text(session.title)
                    .font(.custom("Avenir-Heavy", size: HUTokens.textBase, relativeTo: .body))
                    .foregroundStyle(tokens.text)
                if let msg = session.lastMessage {
                    Text(msg)
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
        .background(tokens.surfaceContainer)
        .clipShape(RoundedRectangle(cornerRadius: HUTokens.radiusMd, style: .continuous))
        .listRowInsets(EdgeInsets(top: HUTokens.spaceXs, leading: HUTokens.spaceMd, bottom: HUTokens.spaceXs, trailing: HUTokens.spaceMd))
        .listRowSeparator(.hidden)
        .listRowBackground(Color.clear)
    }
}

private struct SessionDetailView: View {
    let session: ChatSession
    let tokens: (bgSurface: Color, surfaceContainer: Color, text: Color, textMuted: Color, accent: Color)

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
                .background(tokens.surfaceContainer)
                .clipShape(RoundedRectangle(cornerRadius: HUTokens.radiusLg, style: .continuous))

                if let msg = session.lastMessage {
                    Text(msg)
                        .font(.custom("Avenir-Book", size: HUTokens.textBase, relativeTo: .body))
                        .foregroundStyle(tokens.text)
                }
            }
            .padding(HUTokens.spaceMd)
        }
        .background(tokens.bgSurface)
        .navigationTitle(session.title)
#if os(iOS)
        .navigationBarTitleDisplayMode(.inline)
#endif
    }
}

extension ChatSession: Hashable {
    func hash(into hasher: inout Hasher) {
        hasher.combine(id)
    }

    static func == (lhs: ChatSession, rhs: ChatSession) -> Bool {
        lhs.id == rhs.id
    }
}
