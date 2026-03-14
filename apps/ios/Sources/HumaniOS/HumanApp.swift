import SwiftUI

@main
struct HumanApp: App {
    @StateObject private var connectionManager = ConnectionManager()
    @AppStorage("hu-onboarded") private var hasOnboarded = false

    var body: some Scene {
        WindowGroup {
            if hasOnboarded {
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
        case "settings":
            NotificationCenter.default.post(name: .navigateToTab, object: nil, userInfo: ["tab": AppTab.settings])
        case "sessions":
            NotificationCenter.default.post(name: .navigateToTab, object: nil, userInfo: ["tab": AppTab.sessions])
        default:
            break
        }
    }
}

extension Notification.Name {
    static let navigateToTab = Notification.Name("hu.navigateToTab")
}
