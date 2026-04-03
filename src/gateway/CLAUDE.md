# src/gateway/ — HTTP/WebSocket Gateway (High Risk)

The gateway is the primary network-facing server, handling external HTTP and WebSocket traffic. It bridges external clients (web UI, mobile apps, third-party integrations) to the agent core. **All input is untrusted.**

## Architecture

```
gateway.c              Main HTTP server (listen, route, dispatch)
ws_server.c            WebSocket server for real-time communication
event_bridge.c         Event bridge for async notifications
oauth.c                OAuth flow handling
openai_compat.c        OpenAI-compatible API endpoint
tenant.c               Multi-tenant isolation
rate_limit.c           Per-endpoint rate limiting
thread_pool.c          Worker thread pool for request handling
push.c                 Push notification delivery
```

## Control Protocol

```
control_protocol.c     JSON-RPC dispatcher (noun.verb method routing)
cp_admin.c             Admin control methods (status, config, restart)
cp_chat.c              Chat control methods (send, receive, history)
cp_config.c            Config control methods (get, set, reload)
cp_memory.c            Memory control methods (store, recall, search)
cp_voice.c             Voice control methods (transcribe, config, clone)
cp_voice_stream.c      Streaming voice (provider vtable dispatch, Gemini Live/OpenAI Realtime)
cp_internal.h          Internal shared definitions
```

## Wire Protocol

- **Format**: JSON-RPC envelope with `noun.verb` method naming
- **Examples**: `chat.send`, `admin.status`, `memory.recall`, `config.get`
- **Responses**: always valid JSON, even for errors
- **Never** leak internal error details or stack traces to clients

## Voice Streaming (`cp_voice_stream.c`)

Real-time bidirectional voice via WebSocket:
- **Provider abstraction**: `hu_voice_provider_t` vtable dispatches to OpenAI Realtime or Gemini Live
- **Modes**: `gemini_live` (default), `openai_realtime`, or Cartesia STT+TTS
- **Binary path**: WebSocket binary frames → `provider.vtable->send_audio()` → provider backend
- **Event poll**: `hu_voice_stream_poll_gemini_live()` drains events from provider, sends to client
- **Tool execution**: Poll loop finds tools by name in `app->tools`, executes, sends result via `provider.vtable->send_tool_response()`
- **Reconnect**: goAway events trigger `provider.vtable->reconnect()` for session resumption
- **Tests**: `test_gateway_voice.c` (24 tests covering Cartesia, Gemini Live, and provider vtable paths)

## Demo Gateway Sync

When adding a new gateway method in C:

1. Add the handler in the appropriate `cp_*.c` file
2. Add the mock response in `ui/src/demo-gateway.ts`
3. Mock responses must mirror real gateway structure

## Rules (Mandatory)

- **All input is untrusted** — validate Content-Type, Content-Length, method, path
- **Reject oversized requests** early (before parsing body)
- **Authentication** checked before processing on all non-public endpoints
- **Rate limiting** on all endpoints
- **CORS** headers configured restrictively
- **Never** return more data than necessary
- **Never** log request bodies or auth tokens
- Include **threat/risk notes** in every commit message
- Test malformed requests (bad JSON, missing fields, wrong types)
- Test authentication and authorization boundaries
