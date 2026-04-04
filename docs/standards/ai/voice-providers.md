---
title: Voice Provider Design Standard
---

# Voice Provider Design Standard

Standards for implementing voice provider backends in the human runtime.

**Cross-references:** [../engineering/api-design.md](../engineering/api-design.md), [../engineering/error-handling.md](../engineering/error-handling.md), [../security/ai-safety.md](../security/ai-safety.md)

---

## Overview

Voice providers implement real-time bidirectional audio I/O for conversational agents. The standard covers the vtable contract, duplex turn-taking protocol, audio emotion extraction, and lifecycle management.

**Implementations:** OpenAI Realtime (`src/voice/realtime.c`), Gemini Live (`src/voice/gemini_live.c`).

---

## Voice Provider Vtable Contract

All voice providers implement `hu_voice_provider_vtable_t` in `include/human/voice/provider.h`.

### Required Methods

| Method | Signature | Purpose | Return |
|--------|-----------|---------|--------|
| `connect` | `hu_error_t (*)(void *ctx)` | Establish connection to provider | HU_OK or HU_ERR_* |
| `send_audio` | `hu_error_t (*)(void *ctx, const void *pcm16, size_t len)` | Send audio chunk (PCM 16-bit) | HU_OK or HU_ERR_* |
| `recv_event` | `hu_error_t (*)(void *ctx, hu_allocator_t *alloc, hu_voice_rt_event_t *out, int timeout_ms)` | Receive next event (blocking with timeout) | HU_OK or HU_ERR_* |
| `add_tool` | `hu_error_t (*)(void *ctx, const char *name, const char *description, const char *parameters_json)` | Register a tool for provider | HU_OK or HU_ERR_* |
| `disconnect` | `void (*)(void *ctx, hu_allocator_t *alloc)` | Close connection and clean up | void |
| `get_name` | `const char *(*)(void *ctx)` | Return stable provider name (lowercase) | "openai-realtime" or "gemini-live" |

### Optional Methods (May Be NULL)

| Method | Signature | Purpose |
|--------|-----------|---------|
| `cancel_response` | `hu_error_t (*)(void *ctx)` | Cancel ongoing LLM response generation |
| `send_activity_start` | `hu_error_t (*)(void *ctx)` | Signal user activity start (manual VAD) |
| `send_activity_end` | `hu_error_t (*)(void *ctx)` | Signal user activity end (manual VAD) |
| `send_audio_stream_end` | `hu_error_t (*)(void *ctx)` | Mark end of audio stream |
| `reconnect` | `hu_error_t (*)(void *ctx)` | Resume session after disconnect |
| `send_tool_response` | `hu_error_t (*)(void *ctx, const char *name, const char *call_id, const char *response_json)` | Send result of tool execution |

### Caller Responsibilities

1. Allocator ownership: caller owns the allocator; provider must not cache it
2. Event ownership: caller owns output buffer in `recv_event` (do not free provider internals)
3. Cleanup: caller must call `disconnect` before freeing provider
4. Error propagation: all errors must be returned via `hu_error_t` (no silent failures)

---

## Duplex Turn-Taking Protocol

Voice providers must implement a finite-state machine (FSM) for turn-taking:

### States

- `IDLE`: No active turn
- `USER_SPEAKING`: User audio being captured/sent
- `AGENT_RESPONDING`: Agent generating response
- `USER_LISTENING`: Agent speaking; user can interrupt

### Turn Transitions

```
IDLE → USER_SPEAKING (on audio received)
USER_SPEAKING → AGENT_RESPONDING (on semantic EOT or activity_end)
AGENT_RESPONDING → USER_LISTENING (on response_start event)
USER_LISTENING → USER_SPEAKING (on user interrupt or response_end)
```

### Semantic End-of-Turn (EOT) Detection

Semantic EOT combines multiple signals to detect natural conversation breaks:

1. **Text-based**: Last token is sentence-final punctuation (`.`, `?`, `!`)
2. **Acoustic**: Pitch drop, decreased energy, extended pause (>300ms)
3. **Linguistic**: Utterance length > 10 words AND matches intent boundaries
4. **Turn-signal tokens**: Provider-specific special tokens in stream

**Default timeout if no semantic EOT:** 15 seconds of user audio.

### Manual VAD (Voice Activity Detection)

When semantic EOT is unreliable, use explicit `send_activity_start` and `send_activity_end` calls:

```c
// Option A: Provider detects VAD
hu_voice_provider_send_audio(&p, pcm_chunk, chunk_len);
// provider emits activity_start, activity_end events

// Option B: Caller detects VAD and signals
hu_voice_provider_send_activity_start(&p);
hu_voice_provider_send_audio(&p, pcm_chunk, chunk_len);
hu_voice_provider_send_activity_end(&p);
```

---

## Audio Emotion Extraction and Mapping

Voice providers must support emotion classification from audio features:

### Extracted Features

| Feature | Range | Meaning |
|---------|-------|---------|
| Pitch (F0) | 50–500 Hz | Fundamental frequency; high = excited, low = sad |
| Energy (RMS) | 0–1 (normalized) | Loudness; high = engaged, low = withdrawn |
| Speech rate | 0.5–2.0x | Words per minute relative to baseline; high = urgent |
| Jitter | 0–1 | Pitch variability; high = nervous |
| Shimmer | 0–1 | Amplitude variability; high = rough voice |
| Pause duration | 0–5+ seconds | Silence between words; long = hesitation |

### Emotion Classification

Emotion mapped from features to five categories:

| Emotion | Pitch | Energy | Rate | Use Case |
|---------|-------|--------|------|----------|
| Happy | High | High | Fast | Positive engagement |
| Sad | Low | Low | Slow | Empathy response needed |
| Angry | Moderate–High | High | Fast | Conflict or urgent need |
| Neutral | Medium | Medium | Normal | Factual discussion |
| Surprised | High (brief spike) | High | Variable | Unexpected input |

### Voice Parameter Mapping

Emotion state drives voice synthesis parameters in agent response:

```
Happy → rate +10%, pitch +15%, add emphasis
Sad → rate -10%, pitch -10%, speak slowly
Angry → rate +5%, pitch +5%, crisp articulation
Surprised → pitch spike, rate varies, strategic pauses
```

**Emotional residue:** Emotion state persists across 2–3 turns (halflife 60 seconds) to maintain conversational coherence.

---

## Provider Lifecycle

### 1. Creation

```c
hu_voice_provider_t provider;
hu_voice_provider_openai_create(alloc, config, &provider);
```

Factory functions handle API key lookup, model defaults, and backend-specific initialization. No connection yet.

### 2. Connection

```c
hu_error_t err = provider.vtable->connect(provider.ctx);
if (err != HU_OK) {
    // Handle connection failure
}
```

Establishes WebSocket or other transport. On failure, return error; state is unchanged.

### 3. Tool Registration

```c
hu_error_t err = provider.vtable->add_tool(provider.ctx,
    "get_weather",
    "Fetch current weather for a location",
    "{\"type\": \"object\", \"properties\": {...}}");
```

Register all tools before starting agent loop. If provider doesn't support tools, return `HU_ERR_NOT_SUPPORTED`.

### 4. Audio Loop

```c
while (should_continue) {
    // Send audio
    err = provider.vtable->send_audio(provider.ctx, pcm_buf, pcm_len);
    
    // Receive events
    err = provider.vtable->recv_event(provider.ctx, alloc, &event, 5000);
    
    // Handle event (transcription, response_start, tool_call, etc.)
}
```

Non-blocking with timeout. `recv_event` returns `HU_ERR_TIMEOUT` if no event within timeout_ms.

### 5. Tool Response (if applicable)

```c
hu_error_t err = provider.vtable->send_tool_response(provider.ctx,
    "get_weather", call_id, "{\"temperature\": 72}");
```

Send tool result back to provider for LLM context. Optional method; NULL if not supported.

### 6. Disconnection

```c
provider.vtable->disconnect(provider.ctx, alloc);
// provider.ctx is now invalid; do not reuse
```

Cleanup WebSocket, buffers, and resources. Idempotent; safe to call multiple times.

---

## Safety: No-Op Stubs and Testing

### No-Op Stubs for Unsupported Features

If optional method is not implemented, stub it to return `HU_ERR_NOT_SUPPORTED`:

```c
hu_error_t provider_cancel_response(void *ctx) {
    (void)ctx;
    return HU_ERR_NOT_SUPPORTED;
}
```

Do NOT call methods that are NULL. Use `provider.vtable->cancel_response` only after checking non-NULL.

### HU_IS_TEST Guards

Tests must not establish real network connections or consume API quota:

```c
void test_voice_provider_emits_transcription(void) {
    hu_voice_provider_t p;
    #ifdef HU_IS_TEST
    hu_voice_provider_create(&p, "mock", test_config);
    #else
    hu_voice_provider_create(&p, "openai", real_config);
    #endif
}
```

Mock providers emit deterministic, canned events; no real audio or API calls.

---

## Error Handling

All vtable methods return `hu_error_t`. Error codes follow `include/human/core/error.h`:

| Code | Meaning | Recovery |
|------|---------|----------|
| `HU_OK` | Success | Continue normally |
| `HU_ERR_INVALID_PARAM` | Bad input (NULL, invalid range) | Check caller input |
| `HU_ERR_NOT_SUPPORTED` | Provider doesn't support this | Degrade gracefully (optional methods only) |
| `HU_ERR_NETWORK` | Connection failed or lost | Retry with exponential backoff |
| `HU_ERR_TIMEOUT` | `recv_event` timed out (no event) | Continue waiting; not fatal |
| `HU_ERR_AUTHENTICATION` | API key invalid or revoked | Re-authenticate; update config |
| `HU_ERR_RATE_LIMITED` | Provider rate limit exceeded | Back off; inform user |
| `HU_ERR_INTERNAL` | Unexpected provider error | Log and reconnect |

---

## Testing Expectations

Providers must pass these test suites:

```bash
./human_tests --suite=Voice                # Core provider contract
./human_tests --suite=VoiceProvider        # Vtable methods
./human_tests --suite=VoiceDuplex          # Turn-taking FSM
./human_tests --suite=GatewayVoice         # Gateway integration
```

Required test coverage:

- Happy path: connect → send_audio → recv_event → tool_call → tool_response → disconnect
- Error paths: NULL inputs, invalid audio, network failures, reconnection
- Edge cases: empty audio chunks, rapid tool calls, user interrupt during response
- Emotion extraction: feature detection from test audio files
- ASan clean: zero memory leaks, no buffer overruns

---

## Normative References

| ID | Source | Relevance |
|---|--------|-----------|
| [RFC3551] | RTP Payload Formats | Audio codec negotiation, PCM-16 encoding |
| [TIMIT] | CMU TIMIT Corpus | Emotion-annotated speech data for testing |
| [VAD] | WebRTC VAD | Voice activity detection baseline algorithms |

---

## Anti-Patterns

```
WRONG -- Hardcode API keys in provider implementation
RIGHT -- Load from hu_config_t via factory function

WRONG -- Call NULL vtable method without checking
RIGHT -- Check non-NULL before calling optional methods

WRONG -- Block recv_event indefinitely without timeout
RIGHT -- Use finite timeout_ms; handle HU_ERR_TIMEOUT

WRONG -- Reuse provider.ctx after disconnect()
RIGHT -- Create new provider if reconnection needed

WRONG -- Emit duplicate or out-of-order events
RIGHT -- Maintain event sequence invariants (turn_start before turn_end)
```

---

## Key Paths

- Provider headers: `include/human/voice/provider.h`
- Provider implementations: `src/voice/realtime.c`, `src/voice/gemini_live.c`
- Duplex FSM: `src/voice/session.c`
- Emotion extraction: `src/voice/audio_emotion.c`
- Gateway integration: `src/gateway/cp_voice.c`
- Tests: `tests/test_gemini_live.c`, `tests/test_gateway_voice.c`
