import SwiftUI
import HumanChatUI

@main
struct HumanApp: App {
    @StateObject private var status = StatusViewModel()

    var body: some Scene {
        Settings {
            SettingsView()
        }
        MenuBarExtra {
            VStack(alignment: .leading, spacing: 0) {
                Button("Open Dashboard") {
                    if let url = URL(string: "http://localhost:3000") {
                        NSWorkspace.shared.open(url)
                    }
                }
                .keyboardShortcut("d")
                .accessibilityLabel("Open h-uman dashboard in browser")
                Button("Start Service") {
                    withAnimation(HUTokens.springExpressive) {
                        status.startService()
                    }
                }
                .keyboardShortcut("s")
                .disabled(status.isServiceRunning)
                .accessibilityLabel("Start h-uman background service")
                Button("Stop Service") {
                    withAnimation(HUTokens.springExpressive) {
                        status.stopService()
                    }
                }
                .disabled(!status.isServiceRunning)
                .accessibilityLabel("Stop h-uman background service")
                Divider()
                Button("Settings...") {
                    NSApplication.shared.sendAction(Selector(("showSettingsWindow:")), to: nil, from: nil)
                }
                .keyboardShortcut(",")
                .accessibilityLabel("Open h-uman settings")
                Divider()
                Button("Quit") {
                    status.stopService()
                    NSApplication.shared.terminate(nil)
                }
                .keyboardShortcut("q")
                .accessibilityLabel("Quit h-uman")
            }
            .font(.custom("Avenir-Book", size: HUTokens.textBase))
            .frame(minWidth: 200)
        } label: {
            Image(systemName: "antenna.radiowaves.left.and.right")
                .symbolRenderingMode(.hierarchical)
                .foregroundStyle(status.statusColor)
                .animation(HUTokens.springExpressive, value: "\(status.isServiceRunning)-\(status.isGatewayConnected)")
        }
    }
}
