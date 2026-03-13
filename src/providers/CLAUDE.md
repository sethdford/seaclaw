# src/providers/ — AI Model Providers

50+ AI provider implementations (9 core + 41 compatible services). Each provider implements the `hu_provider_t` vtable for communicating with LLM APIs.

## Vtable Contract

Every provider must implement `hu_provider_vtable_t`:

- `chat(ctx, alloc, request, model, temperature, out)` — structured chat with tool support
- `chat_with_system(ctx, alloc, system_prompt, message, model, temperature, out, out_len)` — simple chat
- `supports_native_tools(ctx)` — whether the provider handles tool calls natively
- `get_name(ctx)` — stable lowercase name (e.g., `"openai"`, `"anthropic"`)
- `deinit(ctx, alloc)` — cleanup resources

Optional methods (may be NULL):

- `warmup` — pre-connect / initialize
- `chat_with_tools` — extended tool calling
- `supports_streaming` / `stream_chat` — streaming response support
- `supports_vision` / `supports_vision_for_model` — image input support

## Core Providers

```
openai.c             OpenAI API (GPT-4, o-series)
anthropic.c          Anthropic API (Claude)
gemini.c             Google Gemini API
ollama.c             Ollama local models
openai_codex.c       OpenAI Codex integration
openrouter.c         OpenRouter aggregator
claude_cli.c         Claude Code CLI bridge
codex_cli.c          Codex CLI bridge
compatible.c         Generic OpenAI-compatible API adapter (41+ services)
```

## Infrastructure

```
factory.c            Provider registry and creation
from_config.c        Config-driven provider instantiation
api_key.c            API key management and rotation
provider_http.c      Shared HTTP request/response handling
sse.c                Server-Sent Events streaming parser
helpers.c            JSON construction and parsing helpers
error_classify.c     Maps HTTP errors to hu_error_t codes
scrub.c              Redacts sensitive data from logs
reliable.c           Retry and fallback logic
router.c             Routes requests across multiple providers
runtime_bundle.c     Bundles provider with runtime config
```

## Key Types

```c
hu_chat_request_t    — messages, model, temperature, tools, max_tokens
hu_chat_response_t   — content, tool_calls, usage, model
hu_chat_message_t    — role (system/user/assistant/tool), content, tool_calls
hu_tool_call_t       — id, name, arguments
hu_tool_spec_t       — name, description, parameters_json
hu_stream_chunk_t    — delta, is_final, token_count
hu_token_usage_t     — prompt_tokens, completion_tokens, total_tokens
```

## Adding a New Provider

1. Create `src/providers/<name>.c` implementing `hu_provider_vtable_t`
2. Create `include/human/providers/<name>.h`
3. Register in `src/providers/factory.c`
4. Add `HU_ENABLE_PROVIDER_<NAME>` feature flag if optional
5. Test vtable wiring, error paths, and config parsing
6. Use `HU_IS_TEST` — no real API calls in tests

If the provider uses an OpenAI-compatible API, prefer extending `compatible.c` with a new entry in `hu_compatible_provider_url()` rather than creating a new file.

## Rules

- Always free `hu_chat_response_t` via `hu_chat_response_free`
- Never log API keys or raw request/response bodies
- Use `provider_http.c` for HTTP operations — don't roll your own
- `scrub.c` must redact sensitive fields before logging
- SSE parsing must handle partial chunks and reconnection
- `reliable.c` handles retries — don't add retry logic in individual providers
