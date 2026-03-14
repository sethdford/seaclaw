import SwiftUI
import HumanChatUI

struct SettingsView: View {
    @Environment(\.colorScheme) private var colorScheme
    @EnvironmentObject var connectionManager: ConnectionManager

    private var tokens: (success: Color, error: Color, text: Color, textMuted: Color) {
        if colorScheme == .dark {
            return (HUTokens.Dark.success, HUTokens.Dark.error, HUTokens.Dark.text, HUTokens.Dark.textMuted)
        } else {
            return (HUTokens.Light.success, HUTokens.Light.error, HUTokens.Light.text, HUTokens.Light.textMuted)
        }
    }

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    TextField("Gateway URL", text: $connectionManager.gatewayURL)
                        .autocorrectionDisabled()
                        .font(.custom("Avenir-Book", size: HUTokens.textBase, relativeTo: .body))
                        .accessibilityLabel("Server URL")
#if os(iOS)
                        .textInputAutocapitalization(.never)
                        .keyboardType(.URL)
#endif
                } header: {
                    Text("Connection")
                        .font(.custom("Avenir-Medium", size: HUTokens.textSm, relativeTo: .subheadline))
                } footer: {
                    Text("WebSocket URL, e.g. wss://localhost:3000/ws")
                        .font(.custom("Avenir-Book", size: HUTokens.textSm, relativeTo: .footnote))
                }

                Section {
                    HStack {
                        Text("Status")
                            .font(.custom("Avenir-Book", size: HUTokens.textBase, relativeTo: .body))
                            .foregroundStyle(tokens.text)
                        Spacer()
                        HStack(spacing: HUTokens.spaceSm) {
                            Circle()
                                .fill(connectionManager.isConnected ? tokens.success : tokens.error)
                                .frame(width: HUTokens.spaceSm, height: HUTokens.spaceSm)
                            Text(connectionManager.isConnected ? "Connected" : "Disconnected")
                                .font(.custom("Avenir-Book", size: HUTokens.textSm, relativeTo: .subheadline))
                                .foregroundStyle(tokens.textMuted)
                        }
                        .accessibilityLabel("Connection status: \(connectionManager.isConnected ? "Connected" : "Disconnected")")
                        .animation(HUTokens.springExpressive, value: connectionManager.isConnected)
                    }

                    Button(connectionManager.isConnected ? "Disconnect" : "Connect") {
                        withAnimation(HUTokens.springExpressive) {
                            if connectionManager.isConnected {
                                connectionManager.disconnect()
                            } else {
                                connectionManager.connect()
                            }
                        }
                    }
                    .font(.custom("Avenir-Medium", size: HUTokens.textBase, relativeTo: .body))
                    .accessibilityLabel(connectionManager.isConnected ? "Disconnect from gateway" : "Connect to gateway")

                    if connectionManager.isConnected {
                        Button("Reconnect") {
                            withAnimation(HUTokens.springExpressive) {
                                connectionManager.reconnect()
                            }
                        }
                        .font(.custom("Avenir-Medium", size: HUTokens.textBase, relativeTo: .body))
                        .accessibilityLabel("Reconnect to gateway")
                    }
                } header: {
                    Text("Connection Status")
                        .font(.custom("Avenir-Medium", size: HUTokens.textSm, relativeTo: .subheadline))
                }
            }
            .navigationTitle("Settings")
        }
    }
}
