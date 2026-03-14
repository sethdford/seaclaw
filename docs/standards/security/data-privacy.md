---
title: Data Privacy & Lifecycle
---

# Data Privacy & Lifecycle

Standards for how human stores, accesses, and disposes of user data. human is a C11 AI assistant runtime that stores user conversations, memories, personas, and secrets locally and optionally in SQLite.

**Cross-references:** [threat-model.md](threat-model.md), [ai-safety.md](ai-safety.md), [../ai/human-in-the-loop.md](../ai/human-in-the-loop.md)

---

## Data Classification

| Class        | Examples                                              | Handling                                                           |
| ------------ | ----------------------------------------------------- | ------------------------------------------------------------------ |
| Public       | persona name, channel type, tool names                | No restrictions                                                    |
| Internal     | conversation logs, memory entries, session history    | Encrypted at rest, scoped access                                   |
| Confidential | persona traits, user preferences, behavioral patterns | Encrypted, no logging, user-deletable                              |
| Restricted   | API keys, tokens, passwords (`hu_secrets_t`)          | AEAD encrypted, never in prompts, never logged, rotate on exposure |

---

## Data Lifecycle

```
Ingest --> Store --> Access --> Decay --> Delete
```

- **Ingest:** Messages arrive via channels; memories created via agent loop
- **Store:** SQLite (`hu_memory_t`), file-based markdown, LRU cache
- **Access:** Scoped by persona + session + user; memory retrieval via `hu_memory_t` vtable
- **Decay:** Forgetting curve (`src/memory/forgetting.c`, `degradation.c`) reduces relevance over time
- **Delete:** Explicit via `memory_forget` tool, user request, or retention policy

---

## Retention Policy

| Data Type                | Default Retention                               | Configurable | Deletion Method            |
| ------------------------ | ----------------------------------------------- | ------------ | -------------------------- |
| Conversation messages    | 90 days                                         | Yes (config) | Auto-purge + manual forget |
| Memory entries (core)    | Indefinite (forgetting curve reduces relevance) | Yes          | `memory_forget` tool       |
| Memory entries (daily)   | 30 days                                         | Yes          | Auto-purge                 |
| Session history          | 7 days                                          | Yes          | Session clear              |
| Secrets (`hu_secrets_t`) | Until rotated                                   | No           | Manual rotation            |
| Persona profiles         | Indefinite                                      | No           | Manual deletion            |

---

## User Rights

- **Access:** User can request export of all stored data (memory entries, conversation history, persona config)
- **Deletion:** `memory_forget` tool deletes specific memories; "forget everything" deletes all user data
- **Correction:** User can correct stored memories via conversation
- **Portability:** Data export in JSON format (memory entries, config, persona)
- **Opt-out:** User can disable memory storage entirely via config

---

## Data Isolation

- **Per-persona scoping:** Memory queries are scoped to active persona
- **Per-session scoping:** Session store isolates conversation threads
- **Cross-persona access:** Not permitted without explicit configuration
- **Multi-user:** Each user's data stored and retrieved independently

---

## What Never Enters a Prompt

Repeated from [ai-safety.md](ai-safety.md) for emphasis:

- API keys, tokens, passwords from `hu_secrets_t`
- Other users' private data
- Full file contents of sensitive files
- Raw database queries or internal error details

---

## Logging and Observability

- **Production logs:** Session IDs and event types only; no message content
- **Debug logs:** May include message snippets; never include secrets
- **Observer metrics:** Token counts, latency, error codes -- no PII
- **Audit trail:** Tool executions logged with actor + action + timestamp; content redacted

---

## Anti-Patterns

```
WRONG -- Store secrets in plaintext or include them in prompts
RIGHT -- Use hu_secrets_t with AEAD encryption; never interpolate secrets into prompts

WRONG -- Log full message content or user data in production
RIGHT -- Log session IDs, event types, and sanitized summaries only

WRONG -- Access memory across personas without explicit configuration
RIGHT -- Scope all memory queries to active persona and user

WRONG -- Retain data indefinitely without user control
RIGHT -- Honor retention policy; support memory_forget and "forget everything"

WRONG -- Export data in formats that leak internal structure
RIGHT -- Export in user-friendly JSON; no raw database dumps

WRONG -- Disable memory storage without informing the user
RIGHT -- Opt-out is explicit; user understands what is not being stored
```
