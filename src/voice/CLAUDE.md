# src/voice/ — Voice Realtime & WebRTC

Real-time voice session management and WebRTC connectivity for low-latency audio streaming.
These modules provide session creation, connect/send/receive APIs; actual media handling is delegated to platform implementations.

## Key Files

- `realtime.c` — real-time voice session (`hu_voice_rt_session_t`), connect/send_audio/disconnect
- `webrtc.c` — WebRTC session (`hu_webrtc_session_t`), SDP exchange, connect/send_audio

## Build

Part of core; no separate gate. The Voice channel (Sonata) is gated by `HU_ENABLE_VOICE`.

## Testing

- `test_voice.c` — voice session tests
- `test_webrtc.c` — WebRTC session tests

## Rules

- `HU_IS_TEST`: mock connect/send paths, no real network or media
- Non-supported platforms return `HU_ERR_NOT_SUPPORTED` for connect/send
