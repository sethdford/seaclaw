import SwiftUI

/// Chat message bubble with user vs assistant styling.
/// Uses SCTokens for accent, spacing, and radius.
public struct ChatBubble: View {
    @Environment(\.colorScheme) private var colorScheme

    public enum Role {
        case user
        case assistant
    }

    public let text: String
    public let role: Role

    public init(text: String, role: Role) {
        self.text = text
        self.role = role
    }

    private var tokens: (bgElevated: Color, accent: Color) {
        colorScheme == .dark ? (SCTokens.Dark.bgElevated, SCTokens.Dark.accent) : (SCTokens.Light.bgElevated, SCTokens.Light.accent)
    }

    public var body: some View {
        HStack(alignment: .bottom, spacing: SCTokens.spaceSm) {
            if role == .user { Spacer(minLength: SCTokens.space2xl) }
            Text(text)
                .font(.custom("Avenir-Book", size: SCTokens.textBase, relativeTo: .body))
                .padding(.horizontal, SCTokens.spaceMd)
                .padding(.vertical, SCTokens.spaceSm)
                .background(role == .user ? tokens.accent : tokens.bgElevated)
                .foregroundColor(role == .user ? .white : .primary)
                .clipShape(RoundedRectangle(cornerRadius: SCTokens.radiusXl, style: .continuous))
            if role == .assistant { Spacer(minLength: SCTokens.space2xl) }
        }
        .padding(.horizontal, SCTokens.spaceXs)
    }
}

#Preview {
    VStack(spacing: SCTokens.spaceMd) {
        ChatBubble(text: "Hello, how can I help?", role: .assistant)
        ChatBubble(text: "What's the weather today?", role: .user)
    }
    .padding()
}
