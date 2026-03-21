# apps/ios/ — iOS App

SwiftUI app using HumanKit shared library.

## Rules

- Import and use `HUTokens` from `DesignTokens.swift` for all colors, spacing, radius, fonts, springs.
- Colors: `HUTokens.Dark.*` or `HUTokens.Light.*` — never `Color(red:green:blue:)` or `Color(hex:)`.
- Spacing: `HUTokens.spaceXs` through `HUTokens.space2xl` — never hardcode CGFloat spacing.
- Radius: `HUTokens.radiusSm` through `HUTokens.radiusXl` — never hardcode `.cornerRadius(12)`.
- Font: `Font.custom("Avenir-Book", size:)` / `"Avenir-Medium"` / `"Avenir-Heavy"` / `"Avenir-Black"`.
- Animation: `HUTokens.springMicro`, `HUTokens.springStandard`, `HUTokens.springExpressive`, `HUTokens.springDramatic`.
- Accessibility: support Dynamic Type, VoiceOver, reduce motion.

## Fleet / CI (XCUITest)

- Xcode project is generated: `brew install xcodegen && cd apps/ios && xcodegen generate` → `HumaniOS.xcodeproj`.
- UI tests live in `UITests/` (scheme `HumaniOS`, target `HumaniOSUITests`). Launch argument `-uitestSkipOnboarding` skips onboarding for deterministic automation.
- Workflow: `.github/workflows/native-apps-fleet.yml` (matrix + award-tier gate). Default `ci.yml` runs the same suite on one simulator.
