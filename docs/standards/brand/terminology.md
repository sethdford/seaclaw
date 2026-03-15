---
title: Terminology
---

# Terminology

Canonical terms used across the human project (code, docs, UI, CLI). Single source of truth for naming consistency.

**Cross-references:** [../ai/conversation-design.md](../ai/conversation-design.md), [../ai/disclosure.md](../ai/disclosure.md)

---

## Brand Identity

| Element       | Canonical Form                     | Usage                                                                                       |
| ------------- | ---------------------------------- | ------------------------------------------------------------------------------------------- |
| Product name  | human                              | Lowercase in running text. Title case ("Human") only at sentence start or in `<title>` tags |
| Stylized name | h-uman                             | Domain, logos, hero branding, social cards, footer lockups                                  |
| Tagline       | not quite human.                   | Always lowercase, always ends with a period. Used in subtitles, meta descriptions, footers  |
| Mission       | Bring AI to every device on Earth. | Hero headlines, OG titles, README header                                                    |

### Tagline Usage Rules

- **Standalone**: "not quite human." — lowercase, period, no dash prefix
- **With brand**: "h-uman — not quite human." — em dash separator
- **Never**: "Not Quite Human", "NOT QUITE HUMAN", "not quite human" (without period)
- **Context**: the tagline conveys self-aware AI identity — honest about what the runtime is and isn't

---

## Canonical Product Terms

| Term       | Definition                                           | Never Use                                                 |
| ---------- | ---------------------------------------------------- | --------------------------------------------------------- |
| human      | The product/runtime name (lowercase)                 | Human (capitalized, except sentence start), HUMAN         |
| persona    | A configured identity profile for the agent          | profile, personality, character                           |
| channel    | A messaging transport (Telegram, Discord, CLI, etc.) | platform, integration, connector                          |
| provider   | An AI model backend (OpenAI, Anthropic, etc.)        | model (when referring to the provider), service, API      |
| model      | A specific AI model (gpt-4, claude-3.5-sonnet)       | provider (when referring to a specific model)             |
| tool       | An executable capability (shell, web_search, etc.)   | function, action, command, plugin (tools are not plugins) |
| plugin     | A loadable extension package (`hu_plugin_t`)         | addon, module (in plugin context)                         |
| memory     | Stored knowledge/context (`hu_memory_t`)             | database, knowledge base, storage                         |
| gateway    | The HTTP/WebSocket server                            | server (in gateway context), API server                   |
| runtime    | The execution environment (native, docker, wasm)     | container (unless specifically Docker)                    |
| peripheral | A hardware board (Arduino, STM32, RPi)               | device (ok as synonym), IoT (avoid)                       |
| sandbox    | The security isolation layer                         | jail, container (in sandbox context)                      |
| pairing    | The device authentication handshake                  | linking, connecting, bonding                              |

---

## Technical Terms

| Preferred | Definition                                    | Avoid                                        |
| --------- | --------------------------------------------- | -------------------------------------------- |
| vtable    | Virtual function table pattern                | interface (in C context), protocol           |
| factory   | Object creation function                      | builder, constructor (C has no constructors) |
| allocator | Memory allocation strategy (`hu_allocator_t`) | malloc wrapper                               |
| arena     | Bulk ephemeral allocator                      | bump allocator, pool (different thing)       |
| observer  | Observability hook (`hu_observer_t`)          | logger (observers do more than log), tracker |

---

## UI/CLI Terms

| Preferred | Context                           | Avoid                                                                         |
| --------- | --------------------------------- | ----------------------------------------------------------------------------- |
| session   | A conversation thread             | chat, thread (unless platform-specific)                                       |
| skill     | An installable capability package | app, extension                                                                |
| overview  | The main dashboard view           | home, main                                                                    |
| config    | Settings/configuration            | preferences, options, settings (use "config" in code, "Settings" in UI label) |

---

## Writing Rules

- Use the canonical term consistently across code, docs, UI, and CLI
- First use in a doc may include a parenthetical definition
- Code identifiers use the canonical term: `hu_persona_t`, not `hu_profile_t`
- Error messages use the canonical term: "no provider configured", not "no model configured"
- UI labels may use title case but must use canonical terms

---

## Enforcement

- `scripts/check-terminology.sh` (future) will scan for blacklisted terms in code and docs
- PR reviews should flag incorrect terminology
- This doc is the single source of truth; update it when adding new concepts

---

## Anti-Patterns

```
WRONG -- Use "profile" or "personality" instead of "persona"
RIGHT -- hu_persona_t, "persona config", "active persona"

WRONG -- Use "platform" or "integration" instead of "channel"
RIGHT -- hu_channel_t, "channel type", "Telegram channel"

WRONG -- Use "model" when referring to the provider (OpenAI, Anthropic)
RIGHT -- "provider" for the backend; "model" for the specific model (gpt-4, claude-3.5)

WRONG -- Call tools "functions", "actions", or "plugins"
RIGHT -- hu_tool_t, "tool execution", "registered tools"

WRONG -- Use "database" or "knowledge base" for memory
RIGHT -- hu_memory_t, "memory backend", "memory retrieval"

WRONG -- Use "server" when referring to the gateway
RIGHT -- "gateway" for the HTTP/WebSocket server component

WRONG -- Inconsistent terminology across code and docs
RIGHT -- One canonical term per concept; reference this doc
```
