# src/voice/ — Voice Provider Abstraction & Session Management

Vtable-driven voice provider abstraction with OpenAI Realtime and Gemini Live backends, session lifecycle, duplex turn-taking FSM, emotion detection, and WebRTC transport.

## Architecture

```
provider.h              Voice provider vtable (hu_voice_provider_vtable_t)
  realtime.c            OpenAI Realtime provider (wraps hu_voice_rt_session_t)
  gemini_live.c         Gemini Live provider (Multimodal Live API over WebSocket)

session.c               Voice session manager (duplex FSM + provider dispatch)
duplex.c                Micro-turn duplex FSM (user/agent turn-taking)
turn_signal.c           Turn signal extraction from token stream
semantic_eot.c          Semantic end-of-turn classifier (text + acoustic features)

audio_emotion.c         Audio-based emotion classification (pitch/energy features)
emotion_voice_map.c     Emotion → voice parameter mapping (rate, pitch, emphasis)

local_stt.c             Local speech-to-text (TensorFlow Lite)
local_tts.c             Local text-to-speech (TensorFlow Lite)

webrtc.c                WebRTC session (SDP, ICE, DTLS, SRTP)
webrtc_dtls.c           DTLS handshake implementation
webrtc_ice.c            ICE candidate handling
webrtc_srtp.c           SRTP encryption/decryption
```

## Voice Provider Vtable

Extension point: `include/human/voice/provider.h` defines `hu_voice_provider_vtable_t`:

```c
connect, send_audio, recv_event, add_tool, cancel_response,
disconnect, get_name, send_activity_start, send_activity_end,
send_audio_stream_end, reconnect, send_tool_response
```

To add a new voice provider:
1. Implement all required vtable slots (connect through get_name)
2. Optional slots (VAD signals, reconnect, tool_response) may be NULL
3. Create factory function: `hu_voice_provider_<name>_create(alloc, config, out)`
4. Tests: `test_voice_provider.c` covers both OpenAI and Gemini vtable paths

## Build

Part of core; no separate gate. The voice channel (Sonata TTS) is gated by `HU_ENABLE_VOICE`.

## Testing

- `test_voice_provider.c` — vtable dispatch for both OpenAI and Gemini (18 tests)
- `test_voice_rt_openai.c` — OpenAI Realtime session API (18 tests)
- `test_gemini_live.c` — Gemini Live session API
- `test_voice_session.c` — session lifecycle and latency
- `test_voice_duplex.c` — duplex FSM transitions
- `test_gateway_voice.c` — gateway integration (24 tests, includes provider dispatch)

## Rules

- `HU_IS_TEST`: mock connect/send paths, no real network or media
- Non-supported platforms return `HU_ERR_NOT_SUPPORTED`
- Provider vtable is the preferred abstraction — avoid direct `hu_voice_rt_*` or `hu_gemini_live_*` calls outside of provider implementations
