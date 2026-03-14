# src/plugins/ — Plugin Registry

Plugin system for dynamically registering and discovering tools. Provides a registry where tools can be loaded and looked up by name.

## Key Files

- `registry.c` — Plugin registration and lookup

## Rules

- Plugins register `hu_tool_t` implementations via the registry
- Never load untrusted plugins without sandbox restrictions
