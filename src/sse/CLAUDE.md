# src/sse/ — Server-Sent Events Client

SSE client for streaming HTTP responses. Parses `event:` and `data:` lines, invokes callback per event. Requires `HU_HTTP_CURL`.

## Key Files

- `sse_client.c` — `hu_sse_connect`; libcurl-based streaming, line-by-line parsing

## Rules

- `HU_IS_TEST`: mock implementation emits one event, no network
- Event size limit: 256KB (`HU_SSE_MAX_EVENT_SIZE`)
- Buffer size: 8KB (`HU_SSE_MAX_BUFFER_SIZE`)
- Free event strings in callback when done
