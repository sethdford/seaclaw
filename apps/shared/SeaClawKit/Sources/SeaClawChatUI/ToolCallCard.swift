import SwiftUI

/// Card displaying tool call status (running, completed, failed).
public struct ToolCallCard: View {
    @Environment(\.colorScheme) private var colorScheme

    public enum Status {
        case running
        case completed
        case failed
    }

    public let name: String
    public let arguments: String?
    public let status: Status
    public let result: String?

    public init(name: String, arguments: String? = nil, status: Status, result: String? = nil) {
        self.name = name
        self.arguments = arguments
        self.status = status
        self.result = result
    }

    private var tokens: (bgElevated: Color, textMuted: Color, warning: Color, success: Color, error: Color) {
        if colorScheme == .dark {
            return (SCTokens.Dark.bgElevated, SCTokens.Dark.textMuted, SCTokens.Dark.warning, SCTokens.Dark.success, SCTokens.Dark.error)
        } else {
            return (SCTokens.Light.bgElevated, SCTokens.Light.textMuted, SCTokens.Light.warning, SCTokens.Light.success, SCTokens.Light.error)
        }
    }

    public var body: some View {
        VStack(alignment: .leading, spacing: SCTokens.spaceSm) {
            HStack(spacing: SCTokens.spaceSm) {
                Image(systemName: statusIcon)
                    .foregroundStyle(statusColor)
                Text(name)
                    .font(.custom("Avenir-Medium", size: SCTokens.textSm, relativeTo: .subheadline))
                Spacer()
                if status == .running {
                    ProgressView()
                        .scaleEffect(0.8)
                }
            }

            if let args = arguments, !args.isEmpty {
                Text(args)
                    .font(.custom("Avenir-Book", size: SCTokens.textXs, relativeTo: .caption))
                    .foregroundStyle(tokens.textMuted)
                    .lineLimit(2)
            }

            if let res = result, !res.isEmpty {
                Text(res)
                    .font(.custom("Avenir-Book", size: SCTokens.textXs, relativeTo: .caption))
                    .foregroundStyle(tokens.textMuted)
                    .lineLimit(3)
            }
        }
        .padding(SCTokens.spaceMd)
        .background(tokens.bgElevated)
        .clipShape(RoundedRectangle(cornerRadius: SCTokens.radiusLg, style: .continuous))
    }

    private var statusIcon: String {
        switch status {
        case .running: return "gearshape.2"
        case .completed: return "checkmark.circle.fill"
        case .failed: return "exclamationmark.triangle.fill"
        }
    }

    private var statusColor: Color {
        switch status {
        case .running: return tokens.warning
        case .completed: return tokens.success
        case .failed: return tokens.error
        }
    }
}

#Preview {
    VStack(spacing: SCTokens.spaceMd) {
        ToolCallCard(name: "shell", arguments: "{\"cmd\":\"ls\"}", status: .running)
        ToolCallCard(name: "browser", status: .completed, result: "Opened page")
    }
    .padding()
}
