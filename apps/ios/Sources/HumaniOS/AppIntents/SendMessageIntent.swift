import AppIntents
import HumanClient

@available(iOS 16.0, *)
struct SendMessageIntent: AppIntent {
    static var title: LocalizedStringResource = "Send Message via Human"
    static var description: IntentDescription = "Send a message through a specific channel"

    @Parameter(title: "Message")
    var message: String

    @Parameter(title: "Channel")
    var channel: String?

    func perform() async throws -> some IntentResult & ReturnsValue<String> {
        let client = HumanGatewayClient.shared
        let response = try await client.sendMessage(message, channel: channel)
        return .result(value: response)
    }
}
