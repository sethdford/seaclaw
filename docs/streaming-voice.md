# Streaming voice (control UI + gateway)

End-to-end flow: the **Voice** tab streams microphone audio over the control WebSocket, the gateway runs STT, publishes the user turn on the bus for the agent, streams **Cartesia** TTS back as **binary** `pcm_f32le` @ 24 kHz, and the dashboard plays it via an `AudioWorklet`.

## Requirements

- **Gateway** built with POSIX + TLS + Cartesia-capable config (`voice.session.start` opens a Cartesia websocket; see `src/gateway/cp_voice_stream.c`).
- **Cartesia API key** in provider config (same path as other Cartesia usage).
- **STT** configured so `hu_voice_stt_file` can decode the buffered recording (e.g. WebM/Opus chunks concatenated).

## UI behavior

| Piece | Role |
| ----- | ---- |
| `ui/src/gateway.ts` | `binaryType = "arraybuffer"`, `sendBinary`, `setOnBinaryChunk`, voice session RPCs |
| `ui/src/audio-recorder.ts` | `startStreaming` + optional `onLevel` (AnalyserNode) |
| `ui/src/audio-playback.ts` | 24 kHz PCM playback |
| `ui/src/voice-stream-silence.ts` | Shared silence detector (auto end-of-utterance) |
| `ui/src/views/voice-view.ts` | Session lifecycle, interrupt, disconnect cleanup |

Assistant **text** still arrives as normal **`chat`** events with payload `id: "voice"` (from `event_bridge.c`), not a separate `voice.text.chunk` event. Assistant **audio** is **only** binary frames, not JSON `voice.audio.chunk`.

## Demo mode

`DemoGatewayClient` simulates transcript + fake PCM + `voice.audio.done` for UI testing without a live gateway.

## Floating mic (`hu-floating-mic`)

Uses **`voice.transcribe`** + `hu-voice-transcribe` / `hu-voice-transcript-result` so dictation **does not** start a full agent+TTS voice session (which `voice.audio.end` would trigger on the gateway).

## Operational notes

- If the WebSocket drops, pending RPCs are rejected, the binary handler is cleared, and the Voice view stops capture and shows a short toast.
- Constants for silence auto-stop live in `voice-stream-silence.ts` (`VOICE_STREAM_SILENCE`).

## See also

- `docs/CONCEPT_INDEX.md` — gateway, WebSocket, voice entries
- `src/gateway/cp_voice_stream.c` — server-side session + bus + TTS drain
