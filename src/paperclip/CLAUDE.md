# src/paperclip/ — Paperclip Task Integration

Integration with the Paperclip task management API. Provides task creation, heartbeat tracking, and status updates through the agent loop.

## Key Files

- `client.c` — API client (create task, heartbeat, complete)
- `heartbeat.c` — Periodic heartbeat sender for active tasks

## Rules

- `HU_IS_TEST`: mock all HTTP calls, no real API traffic
- Use `provider_http.c` for HTTP operations
- Never log API keys or task content
