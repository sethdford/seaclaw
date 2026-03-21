# Native App Parity Matrix

Web dashboard views live in `ui/src/views/` (LitElement). Native apps use the gateway WebSocket protocol (`HumanKit` / `GatewayClient`) with `noun.verb` methods.

**Last audited:** 2026-03-21 (source scan).

## Core product views (tabs / primary flows)

| Web View | File | iOS | Android | Primary gateway methods | Priority |
|----------|------|-----|---------|-------------------------|----------|
| Overview | `overview-view.ts` | Yes (`OverviewView`) | Yes (`OverviewScreen`) | `sessions.list`, `activity.recent`, `health` | P0 |
| Chat | `chat-view.ts` | Yes (`ChatView`) | Yes (`ChatScreen`) | `chat.send`, `chat.history`, `chat.abort` | P0 |
| Memory | `memory-view.ts` | Yes (`MemoryView`, stub list) | Yes (`MemoryScreen`, stub list) | `memory.list`, `memory.recall`, `memory.status`, `memory.store`, … | P0 |
| Sessions | `sessions-view.ts` | Yes (`SessionsView`) | Yes (`SessionsScreen`) | `sessions.list`, `sessions.patch`, `sessions.delete` | P1 |
| Tools | `tools-view.ts` | Yes (`ToolsView`) | Yes (`ToolsScreen`) | `tools.catalog` | P2 |
| Config / Settings | `config-view.ts` | Yes (`SettingsView`) | Yes (`SettingsScreen`) | `config.get`, `config.set`, `config.schema`, `config.apply` | P2 |

## Web-only or not in native shell (this repo)

These views exist on the web dashboard but do **not** have a dedicated native screen in `apps/ios/Sources/HumaniOS/` or `apps/android/.../screens/` yet. Use gateway methods when implementing.

| Web View | File | iOS | Android | Gateway methods (typical) | Priority |
|----------|------|-----|---------|---------------------------|----------|
| Skills | `skills-view.ts` | No | No | `skills.list`, `skills.search`, `skills.enable`, … | P1 |
| Voice | `voice-view.ts` | No | No | `voice.transcribe`, `chat.send` | P1 |
| Channels | `channels-view.ts` | No | No | `channels.status` | P2 |
| Models | `models-view.ts` | No | No | `models.list` | P2 |
| Agents | `agents-view.ts` | No | No | `agents.list` | P2 |
| Nodes | `nodes-view.ts` | No | No | `nodes.list`, `nodes.action` | P2 |
| Metrics | `metrics-view.ts` | No | No | `metrics.snapshot` | P2 |
| Usage | `usage-view.ts` | No | No | `usage.summary` | P2 |
| Security | `security-view.ts` | No | No | (config / policy surfaces) | P2 |
| Logs | `logs-view.ts` | No | No | (log stream / tail; not in `Methods.all` as a single verb) | P2 |
| Automations | `automations-view.ts` | No | No | `cron.list`, `cron.add`, `cron.remove`, … | P2 |
| Turing / eval | `turing-view.ts` | No | No | (eval-specific; web dashboard) | P3 |

## HumanKit protocol (`apps/shared/HumanKit`)

`HumanProtocol/Methods.swift` already declares **P0** methods including:

- Chat: `chat.send`, `chat.history`, `chat.abort`
- Memory: `memory.list`, `memory.recall`, `memory.status`, `memory.store`, `memory.forget`, `memory.ingest`, `memory.consolidate`

Android `GatewayClient` uses string method names aligned with the same protocol (e.g. `"memory.list"`).

## Wiring notes (no Xcode / Gradle edits in this change)

- **iOS / macOS:** Swift sources live under `apps/ios/Sources/HumaniOS/`. SPM (`Package.swift`) picks up all files in that directory; **XcodeGen** (`project.yml`) also glob-includes the folder when regenerating the app project.
- **Android:** New screens are under `app/src/main/java/ai/human/app/ui/screens/`. **You must register** `MemoryScreen` in navigation if your tree does not auto-discover composables (this repo wires routes in `MainActivity.kt`).

## Next steps (product)

1. Flesh out Memory UI (search, categories, recall) to match `memory-view.ts`.
2. Add Skills and Voice tabs or drill-ins using existing `Methods` / gateway strings.
3. Align native empty/error states with web dashboard patterns (loading, populated, error).
