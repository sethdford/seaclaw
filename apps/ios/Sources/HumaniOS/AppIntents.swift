import AppIntents
import SwiftUI

#if canImport(UIKit)
import UIKit
#endif
#if canImport(AppKit)
import AppKit
#endif

private func openHumanDeepLink(_ url: URL) {
    #if canImport(UIKit)
    UIApplication.shared.open(url, options: [:], completionHandler: nil)
    #elseif canImport(AppKit)
    NSWorkspace.shared.open(url)
    #endif
}

struct SendMessageIntent: AppIntent {
    static var title: LocalizedStringResource = "Send Message to h-uman"
    static var description = IntentDescription("Send a message to your h-uman AI assistant.")
    static var openAppWhenRun = true

    @Parameter(title: "Message")
    var message: String

    func perform() async throws -> some IntentResult {
        var c = URLComponents(string: "human://chat")!
        c.queryItems = [URLQueryItem(name: "message", value: message)]
        if let url = c.url {
            await MainActor.run { openHumanDeepLink(url) }
        }
        return .result()
    }
}

struct CheckStatusIntent: AppIntent {
    static var title: LocalizedStringResource = "Check h-uman Status"
    static var description = IntentDescription("Open h-uman and jump to Overview to see connection status.")
    static var openAppWhenRun = true

    func perform() async throws -> some IntentResult {
        if let url = URL(string: "human://overview") {
            await MainActor.run { openHumanDeepLink(url) }
        }
        return .result()
    }
}

struct HumanShortcuts: AppShortcutsProvider {
    @AppShortcutsBuilder
    static var appShortcuts: [AppShortcut] {
        AppShortcut(
            intent: SendMessageIntent(),
            phrases: [
                "Send \(\.$message) to \(.applicationName)",
                "Ask \(.applicationName) \(\.$message)",
                "Tell \(.applicationName) \(\.$message)",
            ],
            shortTitle: "Send Message",
            systemImageName: "bubble.left.fill"
        )
        AppShortcut(
            intent: CheckStatusIntent(),
            phrases: [
                "Check \(.applicationName) status",
                "Is \(.applicationName) running",
            ],
            shortTitle: "Check Status",
            systemImageName: "antenna.radiowaves.left.and.right"
        )
    }
}
