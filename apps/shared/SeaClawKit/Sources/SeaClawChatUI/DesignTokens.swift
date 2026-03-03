// Auto-generated from design-tokens/ — do not edit manually
import SwiftUI

public enum SCTokens {
    // MARK: - Colors (Dark)
    public enum Dark {
        public static let accent = Color(hex: 0xF97066)
        public static let accentHover = Color(hex: 0xFB8A82)
        public static let accentStrong = Color(hex: 0xFF5C50)
        public static let accentSubtle = Color(red: 0.9765, green: 0.4392, blue: 0.4, opacity: 0.12)
        public static let bg = Color(hex: 0x0C0C0C)
        public static let bgElevated = Color(hex: 0x1E1E1E)
        public static let bgInset = Color(hex: 0x111111)
        public static let bgOverlay = Color(hex: 0x252525)
        public static let bgSurface = Color(hex: 0x161616)
        public static let border = Color(hex: 0x262626)
        public static let borderSubtle = Color(hex: 0x1E1E1E)
        public static let disabledOverlay = Color(red: 1, green: 1, blue: 1, opacity: 0.04)
        public static let error = Color(hex: 0xEF4444)
        public static let errorDim = Color(red: 0.9373, green: 0.2667, blue: 0.2667, opacity: 0.15)
        public static let focusRing = Color(hex: 0xF97066)
        public static let hoverOverlay = Color(red: 1, green: 1, blue: 1, opacity: 0.08)
        public static let info = Color(hex: 0x3B82F6)
        public static let infoDim = Color(red: 0.2314, green: 0.5098, blue: 0.9647, opacity: 0.15)
        public static let link = Color(hex: 0xF97066)
        public static let linkActive = Color(hex: 0xFF5C50)
        public static let linkHover = Color(hex: 0xFB8A82)
        public static let linkVisited = Color(hex: 0xFB8A82)
        public static let onAccent = Color(hex: 0x18181B)
        public static let pressedOverlay = Color(red: 1, green: 1, blue: 1, opacity: 0.12)
        public static let success = Color(hex: 0x22C55E)
        public static let successDim = Color(red: 0.1333, green: 0.7725, blue: 0.3686, opacity: 0.15)
        public static let text = Color(hex: 0xEBEBEB)
        public static let textFaint = Color(hex: 0x525252)
        public static let textMuted = Color(hex: 0x8A8A8A)
        public static let warning = Color(hex: 0xEAB308)
        public static let warningDim = Color(red: 0.9176, green: 0.702, blue: 0.0314, opacity: 0.15)
    }

    // MARK: - Colors (Light)
    public enum Light {
        public static let accent = Color(hex: 0xE11D48)
        public static let accentHover = Color(hex: 0xBE123C)
        public static let accentStrong = Color(hex: 0xF43F5E)
        public static let accentSubtle = Color(red: 0.8824, green: 0.1137, blue: 0.2824, opacity: 0.08)
        public static let bg = Color(hex: 0xFAFAFA)
        public static let bgElevated = Color(hex: 0xF4F4F5)
        public static let bgInset = Color(hex: 0xF0F0F2)
        public static let bgOverlay = Color(hex: 0xFFFFFF)
        public static let bgSurface = Color(hex: 0xFFFFFF)
        public static let border = Color(hex: 0xE4E4E7)
        public static let borderSubtle = Color(hex: 0xF4F4F5)
        public static let disabledOverlay = Color(red: 0, green: 0, blue: 0, opacity: 0.04)
        public static let error = Color(hex: 0xDC2626)
        public static let errorDim = Color(red: 0.8627, green: 0.149, blue: 0.149, opacity: 0.1)
        public static let focusRing = Color(hex: 0xE11D48)
        public static let hoverOverlay = Color(red: 0, green: 0, blue: 0, opacity: 0.04)
        public static let info = Color(hex: 0x2563EB)
        public static let infoDim = Color(red: 0.1451, green: 0.3882, blue: 0.9216, opacity: 0.1)
        public static let link = Color(hex: 0xE11D48)
        public static let linkActive = Color(hex: 0xF43F5E)
        public static let linkHover = Color(hex: 0xBE123C)
        public static let linkVisited = Color(hex: 0xBE123C)
        public static let onAccent = Color(hex: 0xFFFFFF)
        public static let pressedOverlay = Color(red: 0, green: 0, blue: 0, opacity: 0.08)
        public static let success = Color(hex: 0x16A34A)
        public static let successDim = Color(red: 0.0863, green: 0.6392, blue: 0.2902, opacity: 0.1)
        public static let text = Color(hex: 0x18181B)
        public static let textFaint = Color(hex: 0xA1A1AA)
        public static let textMuted = Color(hex: 0x71717A)
        public static let warning = Color(hex: 0xCA8A04)
        public static let warningDim = Color(red: 0.7922, green: 0.5412, blue: 0.0157, opacity: 0.1)
    }

    // MARK: - Spacing
    public static let spaceXs: CGFloat = 4
    public static let spaceSm: CGFloat = 8
    public static let spaceMd: CGFloat = 16
    public static let spaceLg: CGFloat = 24
    public static let spaceXl: CGFloat = 32
    public static let space2xl: CGFloat = 48

    // MARK: - Radius
    public static let radiusSm: CGFloat = 4
    public static let radiusMd: CGFloat = 8
    public static let radiusLg: CGFloat = 12
    public static let radiusXl: CGFloat = 16

    // MARK: - Typography
    public static let fontSans = "Avenir"
    public static let fontMono = "Geist Mono"

    // MARK: - Font sizes
    public static let textBase: CGFloat = 14
    public static let textLg: CGFloat = 16
    public static let textSm: CGFloat = 13
    public static let textXl: CGFloat = 20
    public static let textXs: CGFloat = 11

    // MARK: - Font weights
    public static let weightBlack: CGFloat = 900
    public static let weightBold: CGFloat = 700
    public static let weightLight: CGFloat = 300
    public static let weightMedium: CGFloat = 500
    public static let weightNormal: CGFloat = 400
    public static let weightSemibold: CGFloat = 600

    // MARK: - Duration
    public static let durationFast: Double = 0.1
    public static let durationInstant: Double = 0
    public static let durationNormal: Double = 0.2
    public static let durationSlow: Double = 0.35
    public static let durationSlower: Double = 0.5
    public static let durationSlowest: Double = 0.7

    // MARK: - Opacity
    public static let opacityDisabled: Double = 0.38
    public static let opacityDragged: Double = 0.16
    public static let opacityFocus: Double = 0.12
    public static let opacityHover: Double = 0.08
    public static let opacityOverlayheavy: Double = 0.64
    public static let opacityOverlaylight: Double = 0.08
    public static let opacityOverlaymedium: Double = 0.32
    public static let opacityPressed: Double = 0.12

    // MARK: - Motion (Spring)
    public static let springMicro = Animation.spring(response: 0.314, dampingFraction: 0.75)
    public static let springStandard = Animation.spring(response: 0.444, dampingFraction: 0.707)
    public static let springExpressive = Animation.spring(response: 0.574, dampingFraction: 0.639)
    public static let springDramatic = Animation.spring(response: 0.702, dampingFraction: 0.559)
}

extension Color {
    init(hex: UInt, alpha: Double = 1) {
        self.init(
            .sRGB,
            red: Double((hex >> 16) & 0xFF) / 255,
            green: Double((hex >> 8) & 0xFF) / 255,
            blue: Double(hex & 0xFF) / 255,
            opacity: alpha
        )
    }
}