# src/pwa/ — Progressive Web App Browser Automation

AppleScript-based browser automation for macOS. Drives Chrome, Arc, Brave, Edge, or Safari tabs via `osascript` and `execute javascript`. Used for PWA context injection and tab control.

## Key Files

- `bridge.c` — browser detection, JS string escaping, AppleScript execution
- `context.c` — PWA context management
- `drivers.c` — browser-specific drivers
- `learner.c` — learning/adaptation logic

## Rules

- `HU_IS_TEST`: mock browser detection, no `osascript` spawning
- macOS-only; returns `HU_ERR_NOT_SUPPORTED` on other platforms
- Escape JS strings for safe injection
