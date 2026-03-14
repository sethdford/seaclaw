# src/websocket/ — WebSocket Client

RFC 6455 WebSocket client. Builds frames, handles masking, connects over TCP/TLS. Used by gateway and SSE-like streaming. Requires `HU_GATEWAY_POSIX` for real sockets.

## Key Files

- `websocket.c` — frame building, connect, send, receive, close; TLS via OpenSSL when `HU_HAS_TLS`

## Rules

- `HU_IS_TEST`: mock path, no real sockets
- Max frame payload: 4MB (`HU_WS_MAX_FRAME_PAYLOAD`)
- Max message: 64KB (`HU_WS_MAX_MSG`)
- Mask client→server frames (RFC 6455)
