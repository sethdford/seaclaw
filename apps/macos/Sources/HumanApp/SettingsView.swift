import SwiftUI
import HumanChatUI

struct SettingsView: View {
    @Environment(\.colorScheme) private var colorScheme
    @EnvironmentObject var status: StatusViewModel
    @State private var gatewayURL: String = "ws://localhost:3000"
    @State private var autoStartOnLogin: Bool = false
    @State private var binaryPath: String = ""

    private var accent: Color {
        colorScheme == .dark ? HUTokens.Dark.accent : HUTokens.Light.accent
    }

    private var bgSurface: Color {
        colorScheme == .dark ? HUTokens.Dark.bgSurface : HUTokens.Light.bgSurface
    }

    var body: some View {
        Form {
            Section {
                HStack {
                    Circle()
                        .fill(status.isGatewayConnected ? (colorScheme == .dark ? HUTokens.Dark.success : HUTokens.Light.success) : (colorScheme == .dark ? HUTokens.Dark.error : HUTokens.Light.error))
                        .frame(width: HUTokens.spaceSm, height: HUTokens.spaceSm)
                    Text(status.isGatewayConnected ? "Connected" : "Disconnected")
                        .font(.custom("Avenir-Book", size: HUTokens.textSm))
                        .foregroundStyle(.secondary)
                    if status.isGatewayConnected {
                        Spacer()
                        Text("42 ms")
                            .font(.custom("Avenir-Book", size: HUTokens.textXs))
                            .foregroundStyle(.secondary)
                            .monospacedDigit()
                            .accessibilityLabel("Connection latency: 42 milliseconds")
                    }
                }
                .accessibilityElement(children: .combine)
                .accessibilityLabel("Gateway \(status.isGatewayConnected ? "connected" : "disconnected")")
            } header: {
                Text("Connection")
            }

            DisclosureGroup("Advanced") {
                TextField("Gateway URL", text: $gatewayURL)
                    .textFieldStyle(.roundedBorder)
                    .accessibilityLabel("Gateway URL")
                Toggle("Auto-start on login", isOn: $autoStartOnLogin)
                    .accessibilityLabel("Auto-start on login")
                    .accessibilityValue(autoStartOnLogin ? "On" : "Off")
            } label: {
                Text("Advanced")
            }

            Section {
                Text(binaryPath.isEmpty ? "Not found" : binaryPath)
                    .font(.custom("Avenir-Book", size: HUTokens.textSm))
                    .foregroundColor(binaryPath.isEmpty ? .secondary : .primary)
            } header: {
                Text("Binary")
            }
            .listRowBackground(
                RoundedRectangle(cornerRadius: HUTokens.radiusSm)
                    .fill(bgSurface)
                    .padding(.vertical, HUTokens.spaceXs)
            )
        }
        .padding(HUTokens.spaceSm)
        .frame(minWidth: 400, minHeight: 200)
        .background(bgSurface)
        .tint(accent)
        .animation(HUTokens.springInteractive, value: binaryPath)
        .onAppear {
            let pm = ProcessManager()
            binaryPath = pm.humanPath() ?? ""
        }
    }
}
