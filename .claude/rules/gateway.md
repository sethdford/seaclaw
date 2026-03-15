---
paths: src/gateway/**, include/human/gateway/**
---

# Gateway Module (High Risk)

The gateway handles external HTTP/WebSocket traffic. Read AGENTS.md sections 3.5 and 7.5 before any change.

## Threat Model

- Gateway is the primary attack surface — all input is untrusted
- Every change must include threat/risk notes in the commit message
- Validate Content-Type, Content-Length, method, and path before processing
- Reject oversized requests early (before parsing body)

## Wire Protocol

- JSON-RPC envelope with `noun.verb` method naming
- Response format: always return valid JSON, even for errors
- Never leak internal error details or stack traces to clients

## Demo Gateway Sync

- When adding a new gateway method in C, also add the mock response in `ui/src/demo-gateway.ts`
- Mock responses must mirror real gateway structure — the UI depends on this for development

## Security

- Authentication checked before processing on all non-public endpoints
- Rate limiting on all endpoints
- CORS headers configured restrictively
- Never return more data than necessary in responses
- Log request metadata without logging request bodies or auth tokens

## Required Tests

- At least one test per new endpoint or method
- Test malformed requests (bad JSON, missing fields, wrong types)
- Test authentication and authorization boundaries
- Test with oversized and edge-case inputs

## Standards

- Read `docs/standards/engineering/gateway-api.md` for API design rules.
- Read `docs/standards/security/threat-model.md` for gateway threat model.
- Read `docs/standards/operations/observability.md` for logging and metrics.
