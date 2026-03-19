import SwiftUI
import HumanChatUI
import HumanProtocol

struct ChatView: View {
    @Environment(\.colorScheme) private var colorScheme
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @EnvironmentObject var connectionManager: ConnectionManager
    @FocusState private var isInputFocused: Bool
    @State private var messages: [ChatMessage] = []
    @State private var inputText = ""
    @State private var toolCalls: [ToolCallInfo] = []
    @State private var errorBanner: String?
    @State private var sendTrigger: Int = 0
    @State private var isSending = false

    private static let suggestionChips = ["What can you do?", "Summarize my notes", "Help me plan my day"]

    private var tokens: (bgSurface: Color, error: Color, errorDim: Color, success: Color, text: Color, textMuted: Color, accent: Color) {
        if colorScheme == .dark {
            return (HUTokens.Dark.bgSurface, HUTokens.Dark.error, HUTokens.Dark.errorDim, HUTokens.Dark.success, HUTokens.Dark.text, HUTokens.Dark.textMuted, HUTokens.Dark.accent)
        } else {
            return (HUTokens.Light.bgSurface, HUTokens.Light.error, HUTokens.Light.errorDim, HUTokens.Light.success, HUTokens.Light.text, HUTokens.Light.textMuted, HUTokens.Light.accent)
        }
    }

    private let messageTransition = AnyTransition.asymmetric(
        insertion: .move(edge: .bottom).combined(with: .opacity),
        removal: .opacity
    )

    var body: some View {
        NavigationStack {
            mainContent
                .navigationTitle("Chat")
                .toolbar { toolbarContent }
                .onAppear {
                    connectionManager.setEventHandler { event, payload in
                        handleEvent(event, payload: payload)
                    }
                    connectionManager.connect()
                }
        }
    }

    @ViewBuilder
    private var mainContent: some View {
        VStack(spacing: 0) {
            messageList
            if isSending {
                HStack(spacing: HUTokens.spaceSm) {
                    ProgressView()
                        .scaleEffect(0.9)
                    Text("Sending…")
                        .font(.custom("Avenir-Book", size: HUTokens.textXs, relativeTo: .caption))
                        .foregroundStyle(tokens.textMuted)
                }
                .frame(maxWidth: .infinity)
                .padding(.vertical, HUTokens.spaceSm)
            }
            if let err = errorBanner { errorBannerView(err) }
            ChatInputBar(text: $inputText, onSend: { sendMessage() }, sendTrigger: sendTrigger, focus: $isInputFocused)
                .background(.thinMaterial)
        }
    }

    @ViewBuilder
    private var messageList: some View {
        Group {
            if messages.isEmpty && toolCalls.isEmpty && !isSending {
                emptyStateView
            } else {
                ScrollViewReader { proxy in
                    ScrollView {
                        LazyVStack(alignment: .leading, spacing: HUTokens.spaceMd) {
                            ForEach(messages) { msg in
                                ChatBubble(text: msg.text, role: msg.role)
                                    .id(msg.id)
                                    .transition(messageTransition)
                                    .accessibilityLabel(msg.text)
#if os(iOS)
                                    .contextMenu {
                                        Button {
                                            UIPasteboard.general.string = msg.text
                                        } label: { Label("Copy", systemImage: "doc.on.doc") }
                                        ShareLink(item: msg.text) {
                                            Label("Share", systemImage: "square.and.arrow.up")
                                        }
                                        Button(role: .destructive) {
                                            if reduceMotion {
                                                messages.removeAll { $0.id == msg.id }
                                            } else {
                                                withAnimation(HUTokens.springInteractive) {
                                                    messages.removeAll { $0.id == msg.id }
                                                }
                                            }
                                        } label: { Label("Delete", systemImage: "trash") }
                                    }
#endif
                            }
                            ForEach(toolCalls) { tc in
                                ToolCallCard(name: tc.name, arguments: tc.arguments, status: tc.status, result: tc.result)
                                    .id(tc.id)
                                    .transition(messageTransition)
                            }
                        }
                        .animation(reduceMotion ? nil : HUTokens.springInteractive, value: messages.count + toolCalls.count)
                        .padding()
                    }
                    .onChange(of: messages.count) { _, _ in
                        if let last = messages.last {
                            if reduceMotion {
                                proxy.scrollTo(last.id, anchor: .bottom)
                            } else {
                                withAnimation(HUTokens.springInteractive) { proxy.scrollTo(last.id, anchor: .bottom) }
                            }
                        }
                    }
                }
            }
        }
    }

    @ViewBuilder
    private var emptyStateView: some View {
        VStack(spacing: HUTokens.spaceLg) {
            Spacer()
            VStack(spacing: HUTokens.spaceMd) {
                Image(systemName: "bubble.left.and.bubble.right")
                    .font(.custom("Avenir-Medium", size: HUTokens.text3Xl, relativeTo: .title))
                    .foregroundStyle(tokens.accent)
                    .accessibilityHidden(true)
                Text("Start a conversation")
                    .font(.custom("Avenir-Heavy", size: HUTokens.textLg, relativeTo: .body))
                    .foregroundStyle(tokens.text)
                Text("Send a message or tap a suggestion below")
                    .font(.custom("Avenir-Book", size: HUTokens.textSm, relativeTo: .subheadline))
                    .foregroundStyle(tokens.textMuted)
                VStack(spacing: HUTokens.spaceSm) {
                    ForEach(Self.suggestionChips, id: \.self) { chip in
                        Button {
#if os(iOS)
                            HUTokens.Haptic.selection.trigger()
#endif
                            inputText = chip
                            isInputFocused = true
                        } label: {
                            Text(chip)
                                .font(.custom("Avenir-Medium", size: HUTokens.textSm, relativeTo: .subheadline))
                                .foregroundStyle(tokens.accent)
                                .padding(.horizontal, HUTokens.spaceMd)
                                .padding(.vertical, HUTokens.spaceSm)
                        }
                        .buttonStyle(.plain)
                    }
                }
                .padding(.top, HUTokens.spaceXs)
            }
            .padding(HUTokens.spaceXl)
            .background(.ultraThinMaterial)
            .clipShape(RoundedRectangle(cornerRadius: HUTokens.radiusXl, style: .continuous))
            .padding(.horizontal, HUTokens.spaceLg)
            Spacer()
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }

    @ViewBuilder
    private func errorBannerView(_ err: String) -> some View {
        HStack {
            Text(err)
                .font(.custom("Avenir-Book", size: HUTokens.textXs, relativeTo: .caption))
                .foregroundStyle(tokens.error)
            Spacer()
            Button("Dismiss") { errorBanner = nil }
                .font(.custom("Avenir-Book", size: HUTokens.textXs, relativeTo: .caption))
        }
        .padding(HUTokens.spaceSm)
        .background(tokens.errorDim)
        .cornerRadius(HUTokens.radiusMd)
    }

    @ToolbarContentBuilder
    private var toolbarContent: some ToolbarContent {
        ToolbarItem(placement: .primaryAction) {
            HStack(spacing: HUTokens.spaceSm) {
                connectionIndicator
                if connectionManager.isConnected {
                    Text("Connected")
                        .font(.custom("Avenir-Book", size: HUTokens.textXs, relativeTo: .caption))
                        .foregroundStyle(.secondary)
                }
            }
        }
    }

    @ViewBuilder
    private var connectionIndicator: some View {
        if #available(iOS 17, *) {
            Image(systemName: connectionManager.isConnected ? "antenna.radiowaves.left.and.right" : "wifi.slash")
                .font(.custom("Avenir-Book", size: HUTokens.textXs, relativeTo: .caption))
                .foregroundStyle(connectionManager.isConnected ? tokens.success : tokens.error)
                .contentTransition(.symbolEffect(.replace))
                .accessibilityLabel(connectionManager.isConnected ? "Connected" : "Disconnected")
        } else {
            // Intentional small indicator size; HUTokens.spaceSm (8pt) is the closest token
            Circle()
                .fill(connectionManager.isConnected ? tokens.success : tokens.error)
                .frame(width: HUTokens.spaceSm, height: HUTokens.spaceSm)
                .accessibilityLabel(connectionManager.isConnected ? "Connected" : "Disconnected")
        }
    }

    private func sendMessage() {
        let trimmed = inputText.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return }

#if os(iOS)
        let impact = UIImpactFeedbackGenerator(style: .light)
        impact.impactOccurred()
#endif

        inputText = ""
        sendTrigger += 1
        isSending = true

        if reduceMotion {
            messages.append(ChatMessage(id: UUID(), text: trimmed, role: .user))
        } else {
            withAnimation(HUTokens.springInteractive) {
                messages.append(ChatMessage(id: UUID(), text: trimmed, role: .user))
            }
        }

        Task {
            do {
                _ = try await connectionManager.request(
                    method: Methods.chatSend,
                    params: ["message": AnyCodable(trimmed)]
                )
            } catch {
                await MainActor.run {
                    isSending = false
                    if reduceMotion {
                        messages.append(ChatMessage(
                            id: UUID(),
                            text: "Failed to send: \(error.localizedDescription)",
                            role: .assistant
                        ))
                    } else {
                        withAnimation(HUTokens.springInteractive) {
                            messages.append(ChatMessage(
                                id: UUID(),
                                text: "Failed to send: \(error.localizedDescription)",
                                role: .assistant
                            ))
                        }
                    }
                }
            }
        }
    }

    private func handleEvent(_ event: String, payload: [String: AnyCodable]?) {
        DispatchQueue.main.async {
            switch event {
            case "error":
                isSending = false
                let msg = payload?["message"]?.value as? String ?? payload?["error"]?.value as? String ?? "Unknown error"
                errorBanner = msg.isEmpty ? "Unknown error" : msg
            case "health":
                break // Connection status handled by ConnectionManager state
            case "chat":
                let state = payload?["state"]?.value as? String
                let content = payload?["message"]?.value as? String
                if let content = content, !content.isEmpty {
                    if state == "sent" || state == "chunk" { isSending = false }
                    if reduceMotion {
                        switch state {
                        case "received":
                            messages.append(ChatMessage(id: UUID(), text: content, role: .user))
                        case "sent":
                            messages.append(ChatMessage(id: UUID(), text: content, role: .assistant))
                        case "chunk":
                            if let last = messages.last, last.role == .assistant {
                                messages[messages.count - 1] = ChatMessage(
                                    id: last.id, text: last.text + content, role: .assistant
                                )
                            } else {
                                messages.append(ChatMessage(id: UUID(), text: content, role: .assistant))
                            }
                        default:
                            break
                        }
                    } else {
                        withAnimation(HUTokens.springExpressive) {
                            switch state {
                            case "received":
                                messages.append(ChatMessage(id: UUID(), text: content, role: .user))
                            case "sent":
                                messages.append(ChatMessage(id: UUID(), text: content, role: .assistant))
                            case "chunk":
                                if let last = messages.last, last.role == .assistant {
                                    messages[messages.count - 1] = ChatMessage(
                                        id: last.id, text: last.text + content, role: .assistant
                                    )
                                } else {
                                    messages.append(ChatMessage(id: UUID(), text: content, role: .assistant))
                                }
                            default:
                                break
                            }
                        }
                    }
                }
            case "agent.tool":
                let name = payload?["message"]?.value as? String ?? payload?["tool"]?.value as? String ?? payload?["name"]?.value as? String ?? "tool"
                let args = (payload?["arguments"]?.value as? [String: Any])
                    .flatMap { try? JSONSerialization.data(withJSONObject: $0) }
                    .flatMap { String(data: $0, encoding: .utf8) }
                if reduceMotion {
                    if let ok = payload?["success"]?.value as? Bool, !toolCalls.isEmpty {
                        let idx = toolCalls.count - 1
                        let result = (payload?["detail"]?.value ?? payload?["message"]?.value).map { "\($0)" }
                        toolCalls[idx] = ToolCallInfo(
                            id: toolCalls[idx].id,
                            name: toolCalls[idx].name,
                            arguments: toolCalls[idx].arguments,
                            status: ok ? .completed : .failed,
                            result: result
                        )
                    } else {
                        toolCalls.append(ToolCallInfo(
                            id: UUID(),
                            name: name,
                            arguments: args,
                            status: .running
                        ))
                    }
                } else {
                    withAnimation(HUTokens.springInteractive) {
                    if let ok = payload?["success"]?.value as? Bool, !toolCalls.isEmpty {
                        let idx = toolCalls.count - 1
                        let result = (payload?["detail"]?.value ?? payload?["message"]?.value).map { "\($0)" }
                        toolCalls[idx] = ToolCallInfo(
                            id: toolCalls[idx].id,
                            name: toolCalls[idx].name,
                            arguments: toolCalls[idx].arguments,
                            status: ok ? .completed : .failed,
                            result: result
                        )
                    } else {
                        toolCalls.append(ToolCallInfo(
                            id: UUID(),
                            name: name,
                            arguments: args,
                            status: .running
                        ))
                    }
                    }
                }
            default:
                break
            }
        }
    }
}

struct ChatMessage: Identifiable {
    let id: UUID
    let text: String
    let role: ChatBubble.Role
}

struct ToolCallInfo: Identifiable {
    let id: UUID
    let name: String
    let arguments: String?
    var status: ToolCallCard.Status
    var result: String?
}
