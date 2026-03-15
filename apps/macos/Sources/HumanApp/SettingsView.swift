import SwiftUI
import HumanChatUI

struct SettingsView: View {
    @Environment(\.colorScheme) private var colorScheme
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
            TextField("Gateway URL", text: $gatewayURL)
                .textFieldStyle(.roundedBorder)
            Toggle("Auto-start on login", isOn: $autoStartOnLogin)
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
