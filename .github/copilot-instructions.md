# GitHub Copilot Instructions — h-uman

not quite human. C11 autonomous AI assistant runtime. See `AGENTS.md` for the full engineering protocol.

## Tech Stack

- **Language**: C11 (`-Wall -Wextra -Wpedantic -Werror`)
- **Dependencies**: libc only (optional SQLite, libcurl)
- **Build**: CMake 3.20+, presets in `CMakePresets.json`
- **Test**: Custom framework in `tests/test_framework.h`, 5,897+ tests

## Architecture

Vtable-driven and modular. Extend by implementing vtable structs + factory registration:

- `src/providers/` — `hu_provider_t` (AI model providers)
- `src/channels/` — `hu_channel_t` (messaging channels)
- `src/tools/` — `hu_tool_t` (tool execution)
- `src/memory/` — `hu_memory_t` (memory backends)
- `src/runtime/` — `hu_runtime_t` (execution environments)

## Naming Conventions

- Functions, variables, fields, files: `snake_case`
- Types/structs: `hu_<name>_t`
- Constants/macros: `HU_SCREAMING_SNAKE`
- Public functions: `hu_<module>_<action>`

## Critical Rules

- Free every allocation (ASan enforced)
- Use `SQLITE_STATIC` (null), never `SQLITE_TRANSIENT`
- Use `HU_IS_TEST` guards for side effects (network, spawning, hardware I/O)
- Tests: no real network, no browser, no process spawning, deterministic
- Security: deny-by-default, HTTPS-only, never log secrets
- One concern per change

## Build & Test

```bash
cmake --preset dev && cmake --build build -j$(nproc)
./build/human_tests
./build/human_tests --suite=<name>    # targeted
```

## UI Stack

- Web: LitElement, `--hu-*` design tokens, Phosphor icons, Avenir font
- iOS: SwiftUI + HUTokens
- Android: Jetpack Compose + HUTokens

## Key References

- `AGENTS.md` — full engineering protocol
- `docs/CONCEPT_INDEX.md` — concept-to-file mapping
- `docs/error-codes.md` — error code reference
- Per-module `CLAUDE.md` files in each `src/` subdirectory

## Standards

All project standards live in `docs/standards/`. Read the applicable standard before writing code.

| Area        | Path                          | Key Standards                                                                        |
| ----------- | ----------------------------- | ------------------------------------------------------------------------------------ |
| AI          | `docs/standards/ai/`          | Agent architecture, hallucination prevention, evaluation, responsible AI, disclosure |
| Design      | `docs/standards/design/`      | Visual standards, motion design, UX patterns, accessibility testing                  |
| Engineering | `docs/standards/engineering/` | Principles, naming, testing, error handling, performance, dependencies               |
| Operations  | `docs/standards/operations/`  | Incident response, monitoring, observability and SLOs                                |
| Quality     | `docs/standards/quality/`     | Governance, ceremonies, code review                                                  |
| Security    | `docs/standards/security/`    | Threat model, sandbox, AI safety, data privacy, compliance                           |

Full index: `docs/standards/README.md`
