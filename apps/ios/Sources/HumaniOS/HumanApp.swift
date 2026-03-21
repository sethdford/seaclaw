import SwiftUI

@main
struct HumanApp: App {
    @StateObject private var connectionManager = ConnectionManager()
    @AppStorage("hu-onboarded") private var hasOnboarded = false

    /// XCUITest fleet passes `-uitestSkipOnboarding` (must not rely on `UserDefaults` in `init()` — @AppStorage reads first).
    private var showMainChrome: Bool {
        hasOnboarded || ProcessInfo.processInfo.arguments.contains("-uitestSkipOnboarding")
    }

    var body: some Scene {
        WindowGroup {
            if showMainChrome {
                ContentView()
                    .environmentObject(connectionManager)
                    .onOpenURL { url in
                        handleDeepLink(url)
                    }
            } else {
                OnboardingView(hasOnboarded: $hasOnboarded)
                    .environmentObject(connectionManager)
            }
        }
    }

    private func handleDeepLink(_ url: URL) {
        guard url.scheme == "human" else { return }
        switch url.host {
        case "chat":
            NotificationCenter.default.post(name: .navigateToTab, object: nil, userInfo: ["tab": AppTab.chat])
            if let components = URLComponents(url: url, resolvingAgainstBaseURL: false),
               let q = components.queryItems?.first(where: { $0.name == "message" })?.value,
               !q.isEmpty {
                UserDefaults.standard.set(q, forKey: "Human.pendingChatMessage")
            }
        case "overview":
            NotificationCenter.default.post(name: .navigateToTab, object: nil, userInfo: ["tab": AppTab.overview])
        case "settings":
            NotificationCenter.default.post(name: .navigateToTab, object: nil, userInfo: ["tab": AppTab.settings])
        case "sessions":
            NotificationCenter.default.post(name: .navigateToTab, object: nil, userInfo: ["tab": AppTab.sessions])
        case "memory":
            NotificationCenter.default.post(name: .navigateToTab, object: nil, userInfo: ["tab": AppTab.memory])
        default:
            break
        }
    }
}

extension Notification.Name {
    static let navigateToTab = Notification.Name("hu.navigateToTab")
}
