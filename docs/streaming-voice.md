---
title: Streaming voice
description: Control UI and gateway flow for voice streaming (STT, TTS, WebSocket)
updated: 2026-03-21
---

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
| `ui/src/audio-recorder.ts` | `startStreaming` + optional `onLevel` (AnalyserNode for RMS) |
| `ui/src/audio-playback.ts` | 24 kHz PCM playback (AudioWorklet, idle auto-dispose after 60s) |
| `ui/src/voice-stream-silence.ts` | Shared silence detector (auto end-of-utterance) |
| `ui/src/views/voice-view.ts` | Session lifecycle, interrupt, disconnect cleanup, audio level state |
| `ui/src/components/hu-voice-orb.ts` | Visual feedback orb (see **Orb states** below) |

Assistant **text** still arrives as normal **`chat`** events with payload `id: "voice"` (from `event_bridge.c`), not a separate `voice.text.chunk` event. Assistant **audio** is **only** binary frames, not JSON `voice.audio.chunk`.

## Orb states and audio level

The `hu-voice-orb` component renders four visual states via CSS class mapping:

| State | Glow | Rings | Button | Aria-label |
| ----- | ---- | ----- | ------ | ---------- |
| **idle** | dim accent | none | default | "Start listening" |
| **listening** | accent, opacity/scale driven by `--_level` | expanding, speed/radius driven by `--_level` | accent fill | "Stop listening" |
| **processing** | tertiary/indigo slow pulse | tertiary slow expand/fade | tertiary border, opacity pulse, `aria-busy=true` | "Processing voice" |
| **speaking** | secondary amber pulse | none | secondary border/glow | "Interrupt and speak" |

The `audioLevel` property (0–1 RMS from the recorder's `AnalyserNode`) is stored as `_audioLevel` reactive state in `voice-view.ts` and forwarded to the orb. It drives the CSS custom property `--_level`, which controls glow opacity/scale and ring expansion speed/radius. When recording stops, `_audioLevel` resets to 0.

All animations respect `prefers-reduced-motion: reduce` (glow, rings, and button animations are disabled; static fallback styles applied).

## Demo mode

`DemoGatewayClient` simulates transcript + fake PCM + `voice.audio.done` for UI testing without a live gateway. The pipeline fires `voice.transcript` immediately, 3 PCM chunks at ~120/210/300ms, chat response at ~200ms, and `voice.audio.done` at ~3400ms.

## Floating mic (`hu-floating-mic`)

Uses **`voice.transcribe`** + `hu-voice-transcribe` / `hu-voice-transcript-result` so dictation **does not** start a full agent+TTS voice session (which `voice.audio.end` would trigger on the gateway). Uses the streaming recorder with silence auto-stop for better UX while keeping batch STT semantics. Dynamic `aria-label` reflects recording/transcribing state; `aria-busy` set during transcription.

## Operational notes

- If the WebSocket drops, pending RPCs are rejected, the binary handler is cleared, and the Voice view stops capture and shows a short toast.
- Constants for silence auto-stop live in `voice-stream-silence.ts` (`VOICE_STREAM_SILENCE`).
- `AudioPlaybackEngine` auto-disposes its `AudioContext` after 60 seconds of idle (post `markEndOfStream` or `interrupt`) to free system audio resources.

## See also

- `docs/CONCEPT_INDEX.md` — gateway, WebSocket, voice entries
- `src/gateway/cp_voice_stream.c` — server-side session + bus + TTS drain
