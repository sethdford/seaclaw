# Add a New Messaging Channel

Step-by-step workflow for adding a new messaging channel to the human runtime.

## Pre-flight

1. Read `src/channels/CLAUDE.md` for module context
2. Read `include/human/channel.h` for the `hu_channel_vtable_t` definition
3. Check `src/channels/` for similar channels to use as a reference

## Steps

### 1. Create the channel source

Create `src/channels/<name>.c` implementing `hu_channel_vtable_t`:

Required methods:

- `start` — initialize connection (authenticate, start polling/webhook)
- `stop` — teardown connection
- `send` — deliver a message to a target (handle platform message limits)
- `name` — return stable lowercase name (e.g., `"mychannel"`)
- `health_check` — return true if channel is operational

Optional methods (set to NULL if not supported):

- `send_event` — rich event delivery
- `start_typing` / `stop_typing` — typing indicators
- `load_conversation_history` — retrieve past messages
- `get_response_constraints` — platform-specific limits (max chars, etc.)
- `react` — add reactions to messages

Use `src/channels/telegram.c` (polling) or `src/channels/discord.c` (WebSocket) as references.

### 2. Register in factory

Add the channel to `src/channels/factory.c`:

- Include the header
- Add the factory entry

### 3. Add feature flag

Add `HU_ENABLE_CHANNEL_<NAME>` to CMakeLists.txt. Ensure it's included in `HU_ENABLE_ALL_CHANNELS`.

### 4. Add tests

Add tests for:

- Vtable wiring (create channel, verify name)
- Auth/config validation (missing credentials should return error)
- Health check behavior
- Send with various message sizes (test splitting if needed)

Use `HU_IS_TEST` guards — no real network in tests.

### 5. Build and verify

```bash
cmake --build build -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)
./build/human_tests --suite=channel
```

## Checklist

- [ ] Implements all required vtable methods
- [ ] Registered in factory
- [ ] Feature flag added and included in `HU_ENABLE_ALL_CHANNELS`
- [ ] Tests pass with 0 ASan errors
- [ ] No hardcoded API keys or tokens
- [ ] `send` handles platform message limits (splits long messages)
- [ ] `start` validates credentials before connecting
- [ ] `HU_IS_TEST` guards on all network operations
