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

