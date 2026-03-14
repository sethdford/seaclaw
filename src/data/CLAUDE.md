# src/data/ — Embedded Data Resources

Embedded JSON/TXT resources for prompts, persona, memory, conversation, and channel config. Loader resolves `~/.human/data/` overrides first, then falls back to embedded data.

## Key Files

- `loader.c` — `hu_data_load`, `hu_data_load_embedded`; home expansion, file size limits
- `embedded_registry.c` — lookup table for embedded paths
- `data_*.c` — generated/embedded blobs (prompts, persona, memory, conversation, channels)

## Rules

- `HU_IS_TEST`: only embedded data, no filesystem
- Max file size: 1MB (`HU_DATA_MAX_FILE_SIZE`)
- Paths are relative (e.g. `prompts/default_identity.txt`)
