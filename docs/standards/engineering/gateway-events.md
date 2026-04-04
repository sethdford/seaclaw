---
title: Gateway Event Naming and Design Standard
---

# Gateway Event Naming and Design Standard

Standards for event design, naming conventions, and payload schemas in the gateway WebSocket and webhook subsystem.

**Cross-references:** [gateway-api.md](gateway-api.md), [api-design.md](api-design.md), [../security/threat-model.md](../security/threat-model.md)

---

## Overview

Gateway events communicate real-time state changes to clients via WebSocket and webhooks. This standard ensures consistent naming, payload structure, and versioning across all event types.

**Related code:** `src/gateway/gateway.c`, `src/gateway/cp_*.c` (control protocol), `ui/src/gateway.ts`.

---

## Event Naming Convention

Events use `noun.verb` or `noun.state` pattern (lowercase, dot-separated):

### Noun Categories

| Noun | Description | Examples |
|------|-------------|----------|
| `voice` | Voice session events | `voice.started`, `voice.transcript` |
| `chat` | Chat turn events | `chat.message`, `chat.tool_call` |
| `session` | Agent session lifecycle | `session.created`, `session.closed` |
| `canvas` | Live Canvas editing | `canvas.updated`, `canvas.undo` |
| `tool` | Tool execution | `tool.executed`, `tool.failed` |
| `agent` | Agent state | `agent.thinking`, `agent.idle` |
| `memory` | Memory operations | `memory.stored`, `memory.recalled` |
| `error` | Error conditions | `error.network`, `error.rate_limited` |

### Verb Categories

| Verb | State | Example |
|------|-------|---------|
| Action verbs | Active process | `started`, `executed`, `received`, `completed` |
| State verbs | Result/status | `ready`, `error`, `timeout`, `unauthorized` |
| Change verbs | Data mutation | `updated`, `created`, `deleted`, `modified` |

**Valid patterns:**
- `voice.started` (noun + action) ✓
- `session.error` (noun + state) ✓
- `chat.message_received` (compound verb) ✓

**Invalid patterns:**
- `started_voice` (verb first) ✗
- `voice_start` (underscore instead of dot) ✗
- `VOICE.START` (uppercase) ✗

---

## Event Taxonomy (Current)

### Voice Events

| Event | Triggers | Identifying Fields |
|-------|----------|-------------------|
| `voice.started` | WebSocket connection established | `session_id`, `timestamp` |
| `voice.transcript` | Speech recognized | `session_id`, `text`, `is_final` |
| `voice.activity_start` | User began speaking | `session_id`, `timestamp` |
| `voice.activity_end` | User stopped speaking | `session_id`, `timestamp` |
| `voice.turn_start` | Agent began response | `session_id`, `timestamp` |
| `voice.turn_end` | Agent finished response | `session_id`, `timestamp` |
| `voice.tool_call` | Agent requested tool | `session_id`, `tool_name`, `call_id` |
| `voice.emotion` | Emotion detected in audio | `session_id`, `emotion`, `confidence` |
| `voice.ended` | Session closed (user or agent) | `session_id`, `reason` |
| `voice.error` | Voice subsystem error | `session_id`, `message`, `error_code` |

### Chat Events

| Event | Triggers | Identifying Fields |
|-------|----------|-------------------|
| `chat.message` | User message received | `session_id`, `message_id`, `from` |
| `chat.tool_call` | Agent requested tool | `session_id`, `tool_name`, `call_id` |
| `chat.tool_result` | Tool execution result | `session_id`, `call_id`, `status` |
| `chat.response_chunk` | Agent response chunk (streaming) | `session_id`, `chunk_id`, `content` |
| `chat.response_complete` | Agent completed response | `session_id`, `message_id` |
| `chat.context_overflow` | Context window approaching limit | `session_id`, `used_pct` |
| `chat.error` | Chat subsystem error | `session_id`, `message`, `error_code` |

### Session Events

| Event | Triggers | Identifying Fields |
|-------|----------|-------------------|
| `session.created` | New agent session started | `session_id`, `agent_id`, `timestamp` |
| `session.active` | Session resumed | `session_id`, `timestamp` |
| `session.paused` | Session paused (user or system) | `session_id`, `reason` |
| `session.closed` | Session ended | `session_id`, `duration_secs`, `reason` |
| `session.config_updated` | Configuration changed | `session_id`, `config_key`, `new_value` |
| `session.memory_consolidated` | Memory consolidation triggered | `session_id`, `entries_consolidated` |
| `session.error` | Session error | `session_id`, `message`, `error_code` |

### Canvas Events

| Event | Triggers | Identifying Fields |
|-------|----------|-------------------|
| `canvas.created` | Live Canvas initialized | `canvas_id`, `format`, `timestamp` |
| `canvas.updated` | Content changed (agent or user) | `canvas_id`, `version_seq`, `updated_by` |
| `canvas.edit_pending` | User made unsaved edits | `canvas_id`, `version_seq` |
| `canvas.version_added` | New version committed | `canvas_id`, `version_seq`, `format` |
| `canvas.undo` | Undo operation applied | `canvas_id`, `version_seq` |
| `canvas.redo` | Redo operation applied | `canvas_id`, `version_seq` |
| `canvas.closed` | Canvas session ended | `canvas_id`, `final_version_seq` |

### Tool Events

| Event | Triggers | Identifying Fields |
|-------|----------|-------------------|
| `tool.executing` | Tool execution started | `session_id`, `tool_name`, `call_id` |
| `tool.executed` | Tool completed successfully | `session_id`, `tool_name`, `call_id`, `duration_ms` |
| `tool.failed` | Tool execution failed | `session_id`, `tool_name`, `call_id`, `error_code` |
| `tool.rate_limited` | Tool rate limit hit | `session_id`, `tool_name`, `retry_after_ms` |

### Agent Events

| Event | Triggers | Identifying Fields |
|-------|----------|-------------------|
| `agent.thinking` | Agent processing input | `session_id`, `duration_secs` |
| `agent.planning` | Agent planning action | `session_id` |
| `agent.idle` | Agent waiting for input | `session_id` |
| `agent.autonomy_level_changed` | Autonomy setting changed | `session_id`, `new_level` |

---

## Payload Schema Patterns

All events follow this envelope:

```json
{
  "event": "noun.verb",
  "timestamp": "2026-04-03T18:30:45.123Z",
  "session_id": "sess_abc123xyz",
  "data": { /* event-specific data */ }
}
```

### Required Fields (All Events)

| Field | Type | Purpose |
|-------|------|---------|
| `event` | string | Event name (e.g., `voice.started`) |
| `timestamp` | ISO 8601 string | When event occurred (server time, UTC) |
| `session_id` | string or UUID | Identifies session or context |
| `data` | object | Event-specific payload |

### Identifying Fields (In `data` object)

Every event must include fields that uniquely identify its subject:

- **Voice events:** Always include `session_id` in both envelope and `data`
- **Chat events:** Always include `session_id` and `message_id` (if applicable)
- **Canvas events:** Always include `canvas_id` and `version_seq` (for mutations)
- **Tool events:** Always include `session_id`, `tool_name`, `call_id`

**Example:**

```json
{
  "event": "tool.executed",
  "timestamp": "2026-04-03T18:30:50.234Z",
  "session_id": "sess_abc123xyz",
  "data": {
    "tool_name": "get_weather",
    "call_id": "call_456def",
    "duration_ms": 245,
    "result": { "temperature": 72 }
  }
}
```

### Error Events Format

Error events must follow a standard format:

```json
{
  "event": "error.{subsystem}",
  "timestamp": "2026-04-03T18:30:50.234Z",
  "session_id": "sess_abc123xyz",
  "data": {
    "error_code": "HU_ERR_NETWORK",
    "message": "Connection to provider failed",
    "details": "timeout after 30s"
  }
}
```

**Required fields in error `data`:**

| Field | Type | Purpose |
|-------|------|---------|
| `error_code` | string | Standardized error code (e.g., `HU_ERR_NETWORK`) |
| `message` | string | Human-readable description |
| `details` | string (optional) | Additional context for debugging |

**Never include:**
- API keys or secrets in `message` or `details`
- Raw exception or stack traces
- Raw provider error responses

---

## Versioning and Backward Compatibility

Events are versioned implicitly through new event names; existing events are immutable.

### Adding New Fields

1. **Optional fields:** Always allowed. Clients must ignore unknown fields.
2. **Required fields:** Create a new event name with `_v2` suffix (e.g., `voice.transcript_v2`).
3. **Field removal:** Never remove; instead deprecate in new version.
4. **Field rename:** Never rename; add new field, deprecate old field.

**Example:**

```json
// OLD (v1)
{ "event": "voice.transcript", "data": { "text": "...", "is_final": true } }

// NEW (v2) — adds confidence field
{ "event": "voice.transcript_v2", "data": { 
  "text": "...", 
  "is_final": true,
  "confidence": 0.95
}}

// Client compatibility
if (event.event === "voice.transcript" || event.event === "voice.transcript_v2") {
  // handle both versions
}
```

---

## WebSocket Event Streaming Rules

1. **Ordering:** Events are delivered in order of emission; no reordering across sessions.
2. **Deduplication:** Duplicate events (same timestamp, session, data) within 100ms are rejected.
3. **Backpressure:** If client doesn't consume events, server queues up to 10,000; then drops oldest.
4. **Heartbeat:** Every 30s of inactivity, server sends `ping` frame (WebSocket control frame); client must respond `pong`.

---

## Webhook Event Format

Webhooks deliver events via POST with HMAC signature:

```http
POST /webhook/voice HTTP/1.1
X-Signature: sha256=abcd1234...
Content-Type: application/json

{
  "event": "voice.transcript",
  "timestamp": "2026-04-03T18:30:50.234Z",
  "session_id": "sess_abc123xyz",
  "data": { ... }
}
```

**Signature validation:** See `src/gateway/gateway.c` — HMAC-SHA256 over JSON body with `hmac_secret` as key.

---

## Anti-Patterns

```
WRONG -- Event name uses underscore: "voice_started"
RIGHT -- Event name uses dot: "voice.started"

WRONG -- Error event omits error_code field
RIGHT -- Always include error_code + message in error events

WRONG -- Duplicate identifying data in envelope and data object
RIGHT -- Use session_id in envelope; include additional IDs in data

WRONG -- Send event with future timestamp
RIGHT -- Use server time (now) as timestamp

WRONG -- Include raw exception or secrets in message field
RIGHT -- Log internally; expose sanitized message to client

WRONG -- Reorder events to match client time
RIGHT -- Maintain server emit order; client should sort if needed
```

---

## Key Paths

- Gateway entry: `src/gateway/gateway.c`
- Voice control protocol: `src/gateway/cp_voice.c`
- Chat control protocol: `src/gateway/cp_chat.c`
- Event emission: `src/gateway/events.c`
- Client handler: `ui/src/gateway.ts`
- Tests: `tests/test_gateway_*.c`

---

## Implementation Checklist

When adding a new event type:

- [ ] Event name follows `noun.verb` convention
- [ ] Payload includes all identifying fields (session, resource IDs)
- [ ] Error variant (if applicable) includes `error_code` and `message`
- [ ] Timestamp is server time in ISO 8601 UTC
- [ ] Test case verifies schema validation
- [ ] Client handler updated (if applicable)
- [ ] Documentation updated
