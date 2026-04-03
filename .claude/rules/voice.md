---
paths: src/voice/**/*.c, src/voice/**/*.h, include/human/voice/**/*.h
---

# Voice Provider Vtable Rules

Voice provider abstraction (OpenAI Realtime, Gemini Live) with session lifecycle, duplex FSM, and audio emotion. Read `src/voice/CLAUDE.md` and `AGENTS.md` section 7.3 before modifying.

## Voice Provider Vtable Contract

All providers implement `hu_voice_provider_vtable_t`. Required methods:

- `connect(config, observer)` → session handle
- `send_audio(audio_chunk, duration)` → error code
- `recv_event()` → voice_event (delta, turn_start, turn_end, transcript, etc.)
- `add_tool(tool_def)` → register tool for provider
- `send_tool_response(tool_id, result)` → error code
- `get_name()` → stable lowercase string (e.g., "openai-realtime", "gemini-live")
- `disconnect()` → cleanup

Optional: `cancel_response`, `reconnect`, `send_activity_start`, `send_activity_end`.

## Duplex Turn-Taking FSM

- User turn → Agent turn transition controlled by semantic EOT (end-of-turn)
- Semantic EOT uses text + acoustic features (pitch, energy)
- Turn signal extraction from LLM token stream (special tokens or heuristics)
- Valid transitions: USER → AGENT, AGENT → USER, IDLE → {USER, AGENT}

## Audio Emotion Detection

- `audio_emotion.c` extracts pitch, energy, timing features from audio
- Emotion classification (happy, sad, angry, neutral, surprised)
- Emotion → voice parameter mapping: rate (0.8-1.2x), pitch (+/-20%), emphasis
- Emotion state persists across turns (emotional residue)

## Provider-Specific Rules

- **OpenAI Realtime**: WebSocket, binary audio (PCM 24kHz), UTF-8 text events
- **Gemini Live**: WebSocket JSON, supports multimodal (text + audio), batched audio
- Both: handle reconnection, audio codec negotiation, session timeout

## HU_IS_TEST Guards

- No real audio streaming in tests
- Mock provider events with realistic payloads
- Deterministic turn-taking (no random timing)
- Mute actual TTS/STT on platform with `HU_IS_TEST`

## Validation

```bash
./human_tests --suite=Voice
./human_tests --suite=VoiceProvider
./human_tests --suite=Duplex
./human_tests --suite=GatewayVoice
```

## Standards

- Read `docs/standards/engineering/error-handling.md` for error propagation
- Read `docs/standards/security/ai-safety.md` for audio data handling
