import SwiftUI

public struct SCGlassModifier: ViewModifier {
    public enum Tier { case subtle, standard, prominent }
    let tier: Tier

    public func body(content: Content) -> some View {
        if #available(iOS 26.0, macOS 26.0, *) {
            content.glassEffect(.regular, in: .rect(cornerRadius: glassRadius))
        } else {
            content.background(.ultraThinMaterial).clipShape(RoundedRectangle(cornerRadius: glassRadius))
        }
    }

    private var glassRadius: CGFloat {
        switch tier {
        case .subtle: return SCTokens.radiusMd
        case .standard: return SCTokens.radiusLg
        case .prominent: return SCTokens.radiusXl
        }
    }
}

public extension View {
    func scGlass(_ tier: SCGlassModifier.Tier = .standard) -> some View {
        modifier(SCGlassModifier(tier: tier))
    }
}
