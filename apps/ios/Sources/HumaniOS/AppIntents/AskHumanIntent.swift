import AppIntents
import HumanClient

@available(iOS 16.0, *)
struct AskHumanIntent: AppIntent {
    static var title: LocalizedStringResource = "Ask Human"
    static var description: IntentDescription = "Ask your Human AI assistant a question"
    static var openAppWhenRun: Bool = false

    @Parameter(title: "Question")
    var query: String

    func perform() async throws -> some IntentResult & ReturnsValue<String> {
        let client = HumanGatewayClient.shared
        let response = try await client.sendMessage(query)
        return .result(value: response)
    }
}
