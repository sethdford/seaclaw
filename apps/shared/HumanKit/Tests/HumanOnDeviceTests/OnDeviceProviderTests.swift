import Testing
@testable import HumanOnDevice

@Suite("OnDeviceProvider")
struct OnDeviceProviderTests {
    #if canImport(FoundationModels)
    @Test("Init succeeds on macOS 26+")
    func initSucceeds() {
        if #available(macOS 26.0, iOS 26.0, *) {
            let provider = OnDeviceProvider()
            _ = provider.isAvailable
        }
    }

    @Test("Model name is apple-foundationmodel")
    func modelName() {
        if #available(macOS 26.0, iOS 26.0, *) {
            #expect(OnDeviceProvider.modelName == "apple-foundationmodel")
        }
    }

    @Test("Context window is 4096")
    func contextWindow() {
        if #available(macOS 26.0, iOS 26.0, *) {
            #expect(OnDeviceProvider.contextWindow == 4096)
        }
    }
    #endif
}

@Suite("OnDeviceChatAdapter")
struct OnDeviceChatAdapterTests {
    @Test("Init succeeds")
    func initSucceeds() {
        let adapter = OnDeviceChatAdapter()
        _ = adapter.isAvailable
    }

    @Test("Short text fits in context")
    func shortTextFits() {
        let adapter = OnDeviceChatAdapter()
        #expect(adapter.fitsInContext("Hello, how are you?"))
    }

    @Test("Very long text does not fit in context")
    func longTextDoesNotFit() {
        let adapter = OnDeviceChatAdapter()
        let longText = String(repeating: "word ", count: 10000)
        #expect(!adapter.fitsInContext(longText))
    }

    @Test("ChatMessage creation")
    func chatMessageCreation() {
        let msg = OnDeviceChatAdapter.ChatMessage(role: .user, content: "Hello")
        #expect(msg.role == .user)
        #expect(msg.content == "Hello")
    }

    @Test("System message role")
    func systemMessageRole() {
        let msg = OnDeviceChatAdapter.ChatMessage(role: .system, content: "Be helpful")
        #expect(msg.role.rawValue == "system")
    }
}
