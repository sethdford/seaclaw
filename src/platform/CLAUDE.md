# src/platform/ — Platform-Specific Code

Platform-specific integrations that only compile on their target OS. Currently macOS-only with Apple Calendar access.

## Key Files

- `calendar_macos.c` — macOS EventKit bridge for calendar events

## Rules

- Guard with `#ifdef __APPLE__` or equivalent platform macros
- Return `HU_ERR_NOT_SUPPORTED` on unsupported platforms
- `HU_IS_TEST`: mock all OS API calls
