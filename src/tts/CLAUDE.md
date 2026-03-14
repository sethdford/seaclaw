# src/tts/ — Text-to-Speech Pipeline

TTS pipeline for voice output. MP3 → CAF conversion for iMessage voice memos, Cartesia integration, emotion mapping. Requires `HU_ENABLE_CARTESIA` for full pipeline.

## Key Files

- `audio_pipeline.c` — MP3 to CAF conversion via `afconvert` (macOS)
- `cartesia.c` — Cartesia TTS API integration
- `emotion_map.c` — emotion-to-voice mapping

## Rules

- `HU_IS_TEST`: writes mock MP3, skips `afconvert`
- macOS-only for CAF; use temp files with `mkstemps`
- Free `hu_run_result_t` after `hu_process_run`
