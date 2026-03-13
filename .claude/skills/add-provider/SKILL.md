# Add a New AI Provider

Step-by-step workflow for adding a new AI model provider to the human runtime.

## Pre-flight

1. Read `src/providers/CLAUDE.md` for module context
2. Read `include/human/provider.h` for the `hu_provider_vtable_t` definition
3. Check if the provider uses an OpenAI-compatible API — if so, extend `src/providers/compatible.c` instead of creating a new file

## Steps

### 1. Create the provider source

Create `src/providers/<name>.c` implementing `hu_provider_vtable_t`:

- `chat` — structured chat with `hu_chat_request_t` / `hu_chat_response_t`
- `chat_with_system` — simple string-in/string-out chat
- `supports_native_tools` — return true if the API handles tool calls
- `get_name` — return stable lowercase name (e.g., `"myprovider"`)
- `deinit` — free all allocated resources

Use `src/providers/openai.c` or `src/providers/anthropic.c` as reference implementations.

### 2. Create the header

Create `include/human/providers/<name>.h` with the factory function declaration.

### 3. Register in factory

Add the provider to `src/providers/factory.c`:

- Include the header
- Add the factory entry mapping the name string to the constructor

### 4. Add to CMakeLists.txt

Add the source file to the provider sources list. If the provider is optional, add a `HU_ENABLE_PROVIDER_<NAME>` feature flag.

### 5. Add tests

Add tests in `tests/` for:

- Vtable wiring (create provider, verify name, verify supports_native_tools)
- Error paths (missing API key, invalid config, network failure)
- Config parsing (if the provider has custom config options)

Use `HU_IS_TEST` guards — no real API calls in tests.

### 6. Build and verify

```bash
cmake --build build -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)
./build/human_tests --suite=provider
```

## Checklist

- [ ] Implements all required vtable methods
- [ ] Registered in `factory.c`
- [ ] Added to CMakeLists.txt
- [ ] Tests pass with 0 ASan errors
- [ ] No hardcoded API keys
- [ ] Uses `provider_http.c` for HTTP (not custom curl)
- [ ] `scrub.c` redacts sensitive fields
