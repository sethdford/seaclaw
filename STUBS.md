# SeaClaw — Project Status

Last updated: 2026-03-03

## Summary

| Metric                         | Value                  |
| ------------------------------ | ---------------------- |
| Source files (src/ + include/) | **463**                |
| Lines of C/H code              | **~91K**               |
| Test files                     | **72**                 |
| Tests passing                  | **2,177/2,177 (100%)** |
| Binary size (MinSizeRel + LTO) | **366 KB**             |

## Channels — Honest Status

### Full send + receive (5 channels)

| Channel  | send()         | listen()               | Config Required               |
| -------- | -------------- | ---------------------- | ----------------------------- |
| CLI      | stdout         | readline               | None                          |
| Telegram | HTTP API       | Long-poll (getUpdates) | `token`                       |
| Discord  | HTTP API       | GET /messages poll     | `token`, `channel_ids`        |
| Email    | curl SMTP      | curl IMAP poll         | SMTP/IMAP config              |
| iMessage | AppleScript    | chat.db SQLite poll    | `default_target` (macOS only) |
| Signal   | signal-cli RPC | signal-cli poll        | `http_url`, `account`         |

### Send only — no inbound polling (13 channels)

| Channel    | send()         | listen()            | Config Required                       |
| ---------- | -------------- | ------------------- | ------------------------------------- |
| Slack      | HTTP API       | Webhook only        | `token`                               |
| WhatsApp   | Graph API      | Webhook only        | `phone_number_id`, `token`            |
| Matrix     | HTTP PUT       | Not implemented     | `homeserver`, `access_token`          |
| IRC        | TCP socket     | Not implemented     | `server`, `port`                      |
| LINE       | Push API       | Webhook only        | `channel_token`                       |
| Lark       | HTTP API       | Webhook only        | `app_id`, `app_secret`                |
| Mattermost | HTTP API       | Webhook only        | `url`, `token`                        |
| OneBot     | HTTP API       | Webhook only        | `api_base`, `access_token`            |
| DingTalk   | HTTP POST      | Webhook only        | `target` (webhook URL)                |
| Nostr      | nak CLI        | Not implemented     | `nak_path`, `relay_url`, `seckey_hex` |
| QQ         | HTTP API       | Webhook only        | `app_id`, `bot_token`                 |
| MaixCam    | Serial (Linux) | Send-only by design | `host` (serial path)                  |
| Web        | In-memory      | Gateway events      | `auth_token` (optional)               |

### Special

| Channel  | Purpose                  |
| -------- | ------------------------ |
| Dispatch | Forwards to sub-channels |

## Tools — All 42 Real

Every tool has a real implementation. In test mode (`SC_IS_TEST`), they return mock
data to avoid side effects. Highlights:

- **File I/O**: read, write, edit, append, diff, apply_patch
- **Shell/Process**: shell, spawn, claude_code, delegate
- **Web**: web_search (Brave/DuckDuckGo/Tavily), web_fetch, http_request
- **Browser**: open, read (HTTP), CDP automation (click/type/scroll — needs Chrome)
- **Memory**: store, recall, list, forget (backed by SQLite/LRU/Markdown)
- **Cron**: add, list, remove, run, runs, update
- **Hardware**: i2c, spi (Linux only), hardware_info, hardware_memory
- **Other**: database (SQLite), notebook, canvas, schema, pushover, composio, message, image, screenshot, agent*query, agent_spawn, mcp*\*

## Providers — All Real

9 core providers + 90+ compatible service mappings. All make real HTTP API calls:

| Provider     | Streaming | Env Var                           |
| ------------ | --------- | --------------------------------- |
| openai       | Yes (SSE) | `OPENAI_API_KEY`                  |
| anthropic    | Yes (SSE) | `ANTHROPIC_API_KEY`               |
| gemini       | Yes (SSE) | `GEMINI_API_KEY`                  |
| ollama       | No        | None (local)                      |
| openrouter   | No        | `OPENROUTER_API_KEY`              |
| compatible   | No        | Per-provider or `SEACLAW_API_KEY` |
| claude_cli   | No        | `ANTHROPIC_API_KEY`               |
| codex_cli    | No        | `OPENAI_API_KEY`                  |
| openai-codex | No        | API key from config               |

Compatible covers: Groq, Mistral, DeepSeek, xAI, Cerebras, Perplexity, Cohere,
Together, Fireworks, HuggingFace, LMStudio, and 80+ more.

## Gateway Control Protocol — 27+ Methods

All real (backed by app context), except:

| Method       | Status  | Notes                         |
| ------------ | ------- | ----------------------------- |
| update.check | Stub    | Always returns "up_to_date"   |
| update.run   | Stub    | Always returns "up_to_date"   |
| nodes.list   | Partial | Single hardcoded "local" node |

Everything else (health, capabilities, config.get/set, chat.send/history/abort,
sessions.list/patch/delete, tools.catalog, channels.status, cron._, skills._,
models.list, usage.summary, push.\*) returns real data and modifies real state.

## What's Stubbed (Returns SC_ERR_NOT_SUPPORTED)

- **postgres.c, redis.c, lancedb.c, lucid.c, api.c**: Memory engines — need external libs
- **store_pgvector.c**: Vector store — needs libpq + pgvector
- **MCP client transport**: Protocol defined, stdio/HTTP transport not implemented
- **Voice I/O (server-side)**: No TTS/STT integration; UI uses browser SpeechRecognition
- **Self-update**: Always returns "up_to_date"
- **Sub-agent spawning**: Interface defined, partially wired

## Web UI Dashboard

14 views, all connected to the gateway via WebSocket:

| View     | Data Source                              | Interactive?                       |
| -------- | ---------------------------------------- | ---------------------------------- |
| Overview | health, capabilities, channels, sessions | Refresh                            |
| Chat     | chat.send/history, events                | Full: type, send, streaming, abort |
| Sessions | sessions.list/patch/delete, chat.history | Select, rename, delete, resume     |
| Agents   | config.get, sessions.list, capabilities  | Navigate to config                 |
| Models   | models.list, config.get                  | Display only                       |
| Voice    | chat.send + browser STT                  | Mic button, speech-to-text         |
| Tools    | tools.catalog                            | Search, expand params              |
| Channels | channels.status                          | Display only                       |
| Skills   | skills.list/install/enable/disable       | Install, toggle                    |
| Cron     | cron.list/add/remove/run                 | Add, run, delete jobs              |
| Config   | config.get/set/schema                    | Edit fields, raw JSON, save        |
| Nodes    | nodes.list, health                       | Display only                       |
| Usage    | usage.summary                            | Display only                       |
| Logs     | WebSocket events                         | Filter, clear                      |

## External Dependencies

| Dependency         | Status         | Required For                      |
| ------------------ | -------------- | --------------------------------- |
| libc               | Linked         | Everything                        |
| SQLite3 (vendored) | Linked         | Memory engine                     |
| libcurl            | Linked         | HTTP client, provider API calls   |
| pthread            | Linked         | Agent dispatcher                  |
| libpq              | Optional (OFF) | PostgreSQL memory engine          |
| OpenSSL            | Optional       | WSS, TLS for WebSocket            |
| math (-lm)         | Linked         | Vector math, retrieval algorithms |
