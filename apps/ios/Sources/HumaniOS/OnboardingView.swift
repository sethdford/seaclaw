import SwiftUI
import HumanChatUI

struct OnboardingView: View {
    @Binding var hasOnboarded: Bool
    @EnvironmentObject var connectionManager: ConnectionManager
    @Environment(\.colorScheme) private var colorScheme
    @State private var currentPage = 0

    private var tokens: (bgSurface: Color, surfaceContainer: Color, text: Color, textMuted: Color, accent: Color, success: Color, error: Color) {
        if colorScheme == .dark {
            return (HUTokens.Dark.bgSurface, HUTokens.Dark.surfaceContainer, HUTokens.Dark.text, HUTokens.Dark.textMuted, HUTokens.Dark.accent, HUTokens.Dark.success, HUTokens.Dark.error)
        } else {
            return (HUTokens.Light.bgSurface, HUTokens.Light.surfaceContainer, HUTokens.Light.text, HUTokens.Light.textMuted, HUTokens.Light.accent, HUTokens.Light.success, HUTokens.Light.error)
        }
    }

    private let pages: [(String, String, String)] = [
        ("antenna.radiowaves.left.and.right", "Welcome to h-uman", "Your autonomous AI assistant runtime. Minimal footprint, maximum capability."),
        ("bolt.fill", "Lightning Fast", "~1696 KB binary, <6 MB RAM, <30 ms startup. Zero dependencies beyond libc."),
        ("bubble.left.and.bubble.right.fill", "34 Channels", "Connect Telegram, Discord, Slack, email, and 30 more messaging platforms."),
    ]

    var body: some View {
        VStack(spacing: 0) {
            TabView(selection: $currentPage) {
                ForEach(Array(pages.enumerated()), id: \.offset) { index, page in
                    VStack(spacing: HUTokens.spaceLg) {
                        Spacer()
                        Image(systemName: page.0)
                            .font(.system(size: 64))
                            .foregroundStyle(tokens.accent)
                            .frame(height: 100)
                        Text(page.1)
                            .font(.custom("Avenir-Heavy", size: HUTokens.text2Xl, relativeTo: .title))
                            .foregroundStyle(tokens.text)
                            .multilineTextAlignment(.center)
                        Text(page.2)
                            .font(.custom("Avenir-Book", size: HUTokens.textBase, relativeTo: .body))
                            .foregroundStyle(tokens.textMuted)
                            .multilineTextAlignment(.center)
                            .padding(.horizontal, HUTokens.spaceXl)
                        Spacer()
                    }
                    .tag(index)
                    .accessibilityElement(children: .combine)
                    .accessibilityLabel("\(page.1). \(page.2)")
                }
            }
            .tabViewStyle(.page(indexDisplayMode: .always))

            VStack(spacing: HUTokens.spaceMd) {
                TextField("Gateway URL", text: $connectionManager.gatewayURL)
                    .font(.custom("Avenir-Book", size: HUTokens.textBase, relativeTo: .body))
                    .padding(HUTokens.spaceMd)
                    .background(tokens.surfaceContainer)
                    .clipShape(RoundedRectangle(cornerRadius: HUTokens.radiusMd, style: .continuous))
                    .autocorrectionDisabled()
#if os(iOS)
                    .textInputAutocapitalization(.never)
                    .keyboardType(.URL)
#endif
                    .accessibilityLabel("Gateway URL")

                Button {
#if os(iOS)
                    let impact = UIImpactFeedbackGenerator(style: .medium)
                    impact.impactOccurred()
#endif
                    withAnimation(HUTokens.springExpressive) {
                        connectionManager.connect()
                        hasOnboarded = true
                    }
                } label: {
                    Text("Get Started")
                        .font(.custom("Avenir-Heavy", size: HUTokens.textBase, relativeTo: .body))
                        .foregroundStyle(.white)
                        .frame(maxWidth: .infinity)
                        .padding(HUTokens.spaceMd)
                        .background(tokens.accent)
                        .clipShape(RoundedRectangle(cornerRadius: HUTokens.radiusMd, style: .continuous))
                }
                .accessibilityLabel("Get started and connect to gateway")

                Button {
                    withAnimation(HUTokens.springExpressive) {
                        hasOnboarded = true
                    }
                } label: {
                    Text("Skip for now")
                        .font(.custom("Avenir-Book", size: HUTokens.textSm, relativeTo: .subheadline))
                        .foregroundStyle(tokens.textMuted)
                }
                .accessibilityLabel("Skip onboarding")
            }
            .padding(HUTokens.spaceLg)
        }
        .background(tokens.bgSurface)
    }
}
