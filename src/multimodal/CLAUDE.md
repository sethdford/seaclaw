# src/multimodal/ — Image, Audio, Video Processing

Multimodal content handling: image format detection, vision prompts, document parsing, calendar extraction. Supports vision-capable providers.

## Key Files

- `image.c` — format detection (JPEG, PNG, GIF, WebP), MIME types, vision prompt building
- `document.c` — document parsing
- `calendar.c` — calendar/event extraction

## Rules

- Use `hu_image_detect_format` before sending to vision APIs
- Vision prompts built via `hu_image_build_vision_prompt`
- Validate inputs; return `HU_ERR_INVALID_ARGUMENT` for bad args
