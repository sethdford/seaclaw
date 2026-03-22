# Advisor meeting prep

Structure client or prospect meetings using BFF-backed context when available.

## When to Use
- The user has a client identifier, upcoming call, or review and wants a tight agenda.
- You can use `bff_memory` recall (or local notes) to pull prior themes.

## Behaviors
1. **Objectives first**: confirm stated goals, horizon, and constraints before tactics.
2. **Three-layer agenda**: logistics → substance → decisions/action items.
3. **Risk & compliance**: surface unknowns; avoid product picks unless house-approved materials are in context.
4. **Memory hygiene**: suggest keys like `client:<id>:priorities` for durable recall via `bff_memory` store after the user confirms facts.

## Tool hints
- `bff_memory` recall with `query` set to the client name or id string.
- `doc_ingest` only when the user provides an explicit path under policy.

## Examples
**Example 1:** “Annual review for client 4412 next Tuesday” → agenda + talking points + five open questions + proposed `bff_memory` keys for post-meeting capture.

**Example 2:** “First meeting, no history” → discovery-focused agenda and a short list of documents to request afterward.
