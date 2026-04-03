---
paths: src/channels/**/*.c, src/channels/**/*.h, include/human/channels/**/*.h
---

# Channels Module Rules

Extension points for messaging integrations (Telegram, Discord, iMessage, voice, etc.). Read `src/channels/CLAUDE.md` and `AGENTS.md` section 7.2 before modifying.

## Channel Vtable Contract

All channels implement `hu_channel_t`. Required methods:

- `connect(config, observer)` → session handle
- `send(msg, dest)` → error code (or message ID)
- `recv_batch(limit)` → array of messages
- `recv_single()` → single message or timeout
- `get_name()` → stable lowercase string (e.g., "telegram", "imessage")
- `get_capabilities()` → bitmask (text, voice, video, reactions, threads, etc.)
- `get_members()` → member list (for group channels)
- `disconnect()` → cleanup

Optional: `upload_file`, `get_profile`, `update_typing`, `react`, `mark_read`.

## HU_IS_TEST Guards

- Add guards on network operations (connect, send, recv)
- No real API calls in test mode — use mock responses
- Mock message payloads with realistic structure
- Test channels: use in-memory queues, deterministic timestamps

## Error Handling

- Transient errors (network): return `HU_ERR_TEMPORARY`, agent will retry
- Permanent errors (auth, rate limit): return `HU_ERR_FAILED`, escalate
- Missing fields in API responses: use sensible defaults or skip message
- Malformed JSON from API: log and continue, don't crash

## HTTP Auth Patterns

- API key: store in vault (never hardcode or log)
- OAuth tokens: refresh proactively, handle 401 gracefully
- Connection retry: exponential backoff (max 5 attempts)
- Webhook signature verification (HMAC-SHA256): validate before processing

## Channel-Specific Notes

- **Telegram**: use long-polling or webhook; handle rate limits (30/sec)
- **Discord**: use WebSocket; handle gateway compress/heartbeat
- **iMessage**: macOS/iOS only; requires sandbox permissions
- **Email**: handle attachments, threading, HTML parsing
- **Voice**: duplex FSM in gateway; see `src/gateway/cp_voice.c`

## Validation

```bash
./human_tests --suite=Channel
./human_tests --suite=ChannelAll
./human_tests --suite=IMMessage
```

## Standards

- Read `docs/standards/security/ai-safety.md` for input sanitization
- Read `docs/standards/engineering/error-handling.md` for error propagation
