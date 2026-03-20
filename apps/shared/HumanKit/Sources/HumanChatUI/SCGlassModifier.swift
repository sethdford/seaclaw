import SwiftUI

public struct SCGlassModifier: ViewModifier {
    public enum Tier { case subtle, standard, prominent }
    let tier: Tier

    @Environment(\.accessibilityReduceTransparency) private var reduceTransparency
    @Environment(\.colorScheme) private var colorScheme

    public func body(content: Content) -> some View {
        Group {
            if reduceTransparency {
                content.background(solidFill).clipShape(RoundedRectangle(cornerRadius: glassRadius))
            } else {
                #if swift(>=6.2)
                if #available(iOS 26.0, macOS 26.0, *) {
                    switch tier {
                    case .subtle:
                        content.glassEffect(.clear, in: .rect(cornerRadius: glassRadius))
                    case .standard:
                        content.glassEffect(.regular, in: .rect(cornerRadius: glassRadius))
                    case .prominent:
                        content.glassEffect(.regular, in: .rect(cornerRadius: glassRadius))
                    }
                } else {
                    content.background(materialForTier).clipShape(RoundedRectangle(cornerRadius: glassRadius))
                }
                #else
                content.background(materialForTier).clipShape(RoundedRectangle(cornerRadius: glassRadius))
                #endif
            }
        }
    }

    private var materialForTier: Material {
        switch tier {
        case .subtle: return .ultraThinMaterial
        case .standard: return .thinMaterial
        case .prominent: return .regularMaterial
        }
    }

    private var solidFill: Color {
        switch tier {
        case .subtle:
            return colorScheme == .dark ? HUTokens.Dark.surfaceContainer : HUTokens.Light.surfaceContainer
        case .standard:
            return colorScheme == .dark ? HUTokens.Dark.surfaceContainerHigh : HUTokens.Light.surfaceContainerHigh
        case .prominent:
            return colorScheme == .dark ? HUTokens.Dark.surfaceContainerHighest : HUTokens.Light.surfaceContainerHighest
        }
    }

    private var glassRadius: CGFloat {
        switch tier {
        case .subtle: return HUTokens.radiusMd
        case .standard: return HUTokens.radiusLg
        case .prominent: return HUTokens.radiusXl
        }
    }
}

public extension View {
    func scGlass(_ tier: SCGlassModifier.Tier = .standard) -> some View {
        modifier(SCGlassModifier(tier: tier))
    }
}
