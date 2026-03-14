---
title: Gateway API Design
---

# Gateway API Design

Standards for the HTTP/WebSocket gateway server in `src/gateway/gateway.c`.

**Cross-references:** [api-design.md](api-design.md), [../security/threat-model.md](../security/threat-model.md), [../security/ai-safety.md](../security/ai-safety.md)

---

## Overview

The gateway (`hu_gateway_run`) is the HTTP + WebSocket + static file server. It handles:

- Webhook endpoints for channel integrations (Telegram, Discord, Slack, etc.)
- WebSocket RPC for the control UI (real-time agent communication)
- Static file serving for the SPA dashboard
- API endpoints (pairing, OAuth, health, config)

## REST Conventions

- Path format: `/api/<resource>` for REST, `/webhook/<channel>` for webhooks
- Methods: GET (read), POST (create/webhook), PATCH (update), DELETE (remove)
- Trailing slashes: not used
- Path traversal: rejected by `hu_gateway_path_has_traversal()`

## Request/Response Format

- Content-Type: `application/json` for API; varies for webhooks
- Max body size: `HU_GATEWAY_MAX_BODY_SIZE` (65536 bytes)
- Rate limiting: `HU_GATEWAY_RATE_LIMIT_PER_MIN` (60/min) per client

## Error Response Format

All errors return JSON:

```json
{
  "error": { "code": "INVALID_PARAM", "message": "Human-readable description" }
}
```

| HTTP Status | Code                  | When                                                      |
| ----------- | --------------------- | --------------------------------------------------------- |
| 200         | OK                    | Successful response                                       |
| 201         | Created               | Resource created (e.g., pairing)                          |
| 204         | No Content            | Successful deletion                                       |
| 400         | Bad Request           | Malformed JSON, missing fields, invalid parameters        |
| 401         | Unauthorized          | Missing or invalid auth token                             |
| 403         | Forbidden             | Valid auth but insufficient permissions                   |
| 404         | Not Found             | Unknown endpoint or resource                              |
| 413         | Payload Too Large     | Body exceeds HU_GATEWAY_MAX_BODY_SIZE                     |
| 429         | Too Many Requests     | Rate limit exceeded                                       |
| 500         | Internal Server Error | Unexpected failure (log internally, never expose details) |

## Security

- CORS: configurable origins; defaults to localhost only
- Auth: WebSocket RPC requires auth token when `require_pairing` is true
- HMAC: webhook signatures verified via `hmac_secret` when configured
- Path security: traversal detection blocks `..`, `%2e%2e`, `%00`
- Input validation: Content-Length parsed and bounded before reading body

## WebSocket RPC

- Protocol: JSON-RPC over WebSocket at `/ws`
- Authentication: first message must be `{"method": "auth.token", "params": {"token": "..."}}`
- Methods scoped to authenticated sessions
- Binary frames rejected; text-only protocol

---

## Anti-Patterns

```c
// WRONG -- skip path traversal check for static file serving
if (strcmp(url_path, "/") == 0) strcat(url_path, "index.html");
serve_file(url_path);  /* /../etc/passwd could be served */

// RIGHT -- reject traversal before serving
if (hu_gateway_path_has_traversal(url_path)) {
    return 403;
}
serve_file(url_path);
```

```c
// WRONG -- read body without validating Content-Length
char buf[65536];
read(sock, buf, sizeof(buf));  /* unbounded read; no size check */

// RIGHT -- parse and bound Content-Length before reading
size_t len;
if (hu_gateway_parse_content_length(header, HU_GATEWAY_MAX_BODY_SIZE, &len) != HU_OK)
    return 413;
if (len > HU_GATEWAY_MAX_BODY_SIZE) return 413;
read_body(sock, buf, len);
```

```c
// WRONG -- expose internal error details in 500 response
return json_error(500, "sqlite3_step failed: %s", sqlite3_errmsg(db));

// RIGHT -- log internally; return generic message
log_error("sqlite3_step failed: %s", sqlite3_errmsg(db));
return json_error(500, "Internal Server Error");
```

```
WRONG -- use trailing slashes in API paths (/api/health/)
RIGHT -- no trailing slashes (/api/health)

WRONG -- return 200 for resource creation
RIGHT -- return 201 Created for POST that creates a resource

WRONG -- skip HMAC verification when hmac_secret is configured
RIGHT -- verify webhook signatures when hmac_secret is set
```
