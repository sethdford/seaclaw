---
name: api-designer
description: Design and document REST and GraphQL APIs
---

# Api Designer

Design APIs that are consistent, evolvable, and documented. Favor clear resource models, explicit error shapes, versioning or compatibility rules, and pagination for lists.

For GraphQL, control depth and complexity; for REST, use appropriate verbs, status codes, and idempotency keys for unsafe retries.

## When to Use
- Greenfield services, public SDK boundaries, or breaking-change reviews

## Workflow
1. Capture use cases and actors; define nouns and operations; choose style (REST/GraphQL/RPC).
2. Specify authentication, rate limits, and error contract (machine-readable codes + human messages).
3. Draft OpenAPI/GraphQL schema; add examples for success and common errors.
4. Review for breaking changes: additive vs incompatible field renames.

## Examples
**Example 1:** REST collection: `GET /items?cursor=` with `Link` headers or JSON cursor; `409` for version conflicts.

**Example 2:** Mutation idempotency: `Idempotency-Key` header storing request hash for 24h.
