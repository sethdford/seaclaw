import SwiftUI

/// Brand-specific icons using SF Symbols as iOS convention (Phosphor equivalents).
/// Use for overview cards, tool cards, and other branded surfaces.
public enum PhosphorIconName: String {
    case grid, chat, clock, wrench, gear, terminal
}

public struct PhosphorIcon: View {
    public let name: PhosphorIconName
    public var size: CGFloat = 24

    public init(name: PhosphorIconName, size: CGFloat = 24) {
        self.name = name
        self.size = size
    }

    public var body: some View {
        Image(systemName: sfSymbolName)
            .font(.system(size: size))
    }

    private var sfSymbolName: String {
        switch name {
        case .grid: return "square.grid.2x2"
        case .chat: return "bubble.left.and.bubble.right"
        case .clock: return "clock.arrow.circlepath"
        case .wrench: return "wrench.and.screwdriver"
        case .gear: return "gear"
        case .terminal: return "terminal"
        }
    }
}
