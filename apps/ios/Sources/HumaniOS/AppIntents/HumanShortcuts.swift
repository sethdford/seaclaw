import AppIntents

@available(iOS 16.0, *)
struct HumanShortcuts: AppShortcutsProvider {
    @AppShortcutsBuilder
    static var appShortcuts: [AppShortcut] {
        AppShortcut(
            intent: AskHumanIntent(),
            phrases: [
                "Ask \(.applicationName) \(\.$query)",
                "Hey \(.applicationName) \(\.$query)",
                "Tell \(.applicationName) \(\.$query)",
            ],
            shortTitle: "Ask Human",
            systemImageName: "brain.head.profile"
        )
        AppShortcut(
            intent: SendMessageIntent(),
            phrases: [
                "Send a message via \(.applicationName)",
                "Message \(.applicationName) \(\.$message)",
            ],
            shortTitle: "Send via Human",
            systemImageName: "bubble.left.and.bubble.right"
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
