# h-uman — not quite human.

C11 autonomous AI assistant runtime. ~1696 KB binary, <6 MB RAM, <30 ms startup.
Zero dependencies beyond libc (optional SQLite and libcurl).

Read `AGENTS.md` for the full engineering protocol. This file is the quick reference.

## Build & Test

```bash
# Dev build — use CMake presets (recommended)
cmake --preset dev                 # ASan, all channels, SQLite, persona, skills, compile_commands.json
cmake --build --preset dev

# Other presets: test (no ASan), release (MinSizeRel+LTO), fuzz (Clang), minimal
cmake --list-presets               # show all available presets

# Run tests (6075+ tests, must be 0 failures, 0 ASan errors)
./build/human_tests                          # full suite
./build/human_tests --suite=JSON             # run suites matching "JSON"
./build/human_tests --filter=config_parse    # run tests matching "config_parse"

# Agent workflow: targeted tests during iteration, full suite before commit
scripts/what-to-test.sh src/tools/shell.c    # find relevant suites for changed files
scripts/agent-preflight.sh                   # change-aware validation (auto-detects what changed)
```

## Architecture

See `ARCHITECTURE.md` for diagrams (system topology, request flow, module dependencies).

Vtable-driven and modular. Extend by implementing vtable structs + factory registration:

- `src/providers/` — `hu_provider_t` vtable (AI model providers)
- `src/channels/` — `hu_channel_t` vtable (messaging channels)
- `src/tools/` — `hu_tool_t` vtable (tool execution)
- `src/memory/` — `hu_memory_t` vtable (memory backends)
- `src/security/` — policy, pairing, secrets, sandboxing
- `src/runtime/` — `hu_runtime_t` vtable (native, docker are real; wasm/cloudflare/gce return `HU_ERR_NOT_SUPPORTED`)
- `src/peripherals/` — `hu_peripheral_t` vtable (Arduino, STM32, RPi)
- `src/persona/` — persona profiles, prompt builder, example banks

## Naming

- Functions, variables, fields, files: `snake_case`
- Types/structs: `hu_<name>_t` (e.g. `hu_provider_t`)
- Constants/macros: `HU_SCREAMING_SNAKE` (e.g. `HU_OK`, `HU_ERR_NOT_SUPPORTED`)
- Public functions: `hu_<module>_<action>` (e.g. `hu_provider_create`)
- Test functions: `subject_expected_behavior`

## Rules (mandatory)

- C11 standard. Compiles with `-Wall -Wextra -Wpedantic -Werror`.
- Free every allocation. ASan catches leaks. No exceptions.
- Never use `SQLITE_TRANSIENT` — use `SQLITE_STATIC` (null).
- Use `HU_IS_TEST` guards for side effects (network, spawning, hardware I/O).
- Tests: no real network, no browser, no process spawning, deterministic.
- Security: deny-by-default, HTTPS-only for outbound, never log secrets.
- KISS/YAGNI: no speculative abstractions or config flags without a caller.
- One concern per change. Don't mix feature + refactor + infra.
- Use `--hu-surface-container*` for branded tonal surfaces, `--hu-bg-surface` for neutral.
- Use tinted state overlays (`--hu-hover-overlay`, etc.) — they are primary-colored, not neutral.

## Commit Format

Conventional commits enforced by `.githooks/commit-msg`:

```
<type>[(<scope>)]: <description>
```

Types: `feat fix refactor test docs chore perf ci build style`

## CI Pipeline

| Workflow                    | What it checks                                                                                                                                    |
| --------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------- |
| `ci.yml`                    | C build + ~6075 tests (Linux + macOS), UI tsc + vitest + build, website build, clang-tidy, E2E, visual regression, axe accessibility, Lighthouse |
| `native-apps-fleet.yml`     | Multi-simulator iOS XCUITest + multi-API Android instrumented tests + SOTA gate (apps path / schedule / dispatch) |
| `.github/actions/ios-uitest` | Composite: XcodeGen + HumaniOS XCUITest (shared by `ci.yml` + fleet) |
| `benchmark.yml`             | Performance regression (binary size, startup time, RSS)                                                                                           |
| `codeql.yml`                | Static analysis security scanning                                                                                                                 |
| `security.yml`              | Dependency audit, SBOM generation                                                                                                                 |
| `release.yml`               | Build release artifacts (Linux x86_64 + macOS aarch64), Docker image, Trivy scan                                                                  |
| `competitive-benchmark.yml` | Weekly PageSpeed competitive analysis (15 brands, 7 scoring dimensions)                                                                           |

Rule: if CI will catch it, run the equivalent locally first.

## Persona System

Persona profiles live in `~/.human/personas/` (JSON). Key structs in `include/human/persona.h`:

- `hu_persona_t` — identity, traits, vocab, communication rules, values, decision style
- `hu_persona_overlay_t` — per-channel formality/length/emoji overrides
- `hu_persona_example_bank_t` — example conversations per channel

Extend via: `src/persona/` (persona.c, creator.c, analyzer.c, sampler.c, examples.c, feedback.c, cli.c).

## Key Paths

| Path                              | What                                                                  |
| --------------------------------- | --------------------------------------------------------------------- |
| `src/`                            | All C source (~1,093 files, ~233K lines)                              |
| `include/human/`                  | Public headers                                                        |
| `tests/`                          | 291 test files, 6075+ tests                                           |
| `fuzz/`                           | libFuzzer harnesses                                                   |
| `ui/`                             | LitElement web dashboard                                              |
| `website/`                        | Astro marketing site                                                  |
| `apps/`                           | iOS, macOS, Android native apps + shared HumanKit                     |
| `design-tokens/`                  | W3C design tokens (source of truth for all UI)                        |
| `docs/`                           | Guides, plans, design docs                                            |
| `docs/standards/`                 | Canonical standards (AI, design, engineering, ops, quality, security) |
| `docs/CONCEPT_INDEX.md`           | Concept-to-file mapping (find the right file fast)                    |
| `docs/error-codes.md`             | All `HU_ERR_*` codes with usage guidelines                            |
| `scripts/`                        | Build, release, benchmark, check scripts                              |
| `scripts/agent-preflight.sh`      | Change-aware validation for agents                                    |
| `scripts/what-to-test.sh`         | Maps changed files to relevant test suites                            |
| `scripts/gen-include-graph.sh`    | Module dependency graph (Mermaid or JSON)                             |
| `ARCHITECTURE.md`                 | System topology, request flow, module dependency diagrams             |
| `.claude/rules/`                  | Path-scoped rules for Claude Code agents                              |
| `.claude/skills/`                 | Executable playbooks (add-provider, add-channel, add-tool, preflight) |
| `.github/copilot-instructions.md` | GitHub Copilot agent context                                          |
| `CMakePresets.json`               | Named build presets (dev, test, release, fuzz, minimal)               |
| `.clang-tidy`                     | Static analysis config (matches CI)                                   |

## Standards

All project standards live in `docs/standards/`. This is the single source of truth -- read the applicable standard before writing code. Full index: `docs/standards/README.md`.

| Area        | Path                          | Covers                                                                                            |
| ----------- | ----------------------------- | ------------------------------------------------------------------------------------------------- |
| AI          | `docs/standards/ai/`          | Agent architecture, conversation, hallucination prevention, prompts, evaluation, disclosure       |
| Brand       | `docs/standards/brand/`       | Terminology governance                                                                            |
| Design      | `docs/standards/design/`      | Visual standards, motion, UX patterns, design strategy, design system                             |
| Engineering | `docs/standards/engineering/` | Principles, naming, testing, workflow, memory management, performance, API design, cross-platform |
| Operations  | `docs/standards/operations/`  | Incident response, monitoring                                                                     |
| Quality     | `docs/standards/quality/`     | Governance, ceremonies, code review, channel testing                                              |
| Security    | `docs/standards/security/`    | Threat model, sandbox, AI safety, data privacy                                                    |

## Risk Tiers

- **Low**: docs, comments, test additions, formatting
- **Medium**: most `src/` behavior changes
- **High**: `src/security/`, `src/gateway/gateway.c`, `src/tools/`, `src/runtime/`, config schema, vtable interfaces

## Design System (all platforms)

- Typeface: **Avenir** (web: `var(--hu-font)`, never Google Fonts)
- Icons: **Phosphor Regular** (web: `ui/src/icons.ts`)
- Tokens: `--hu-*` CSS custom properties from `design-tokens/`
- Never use raw hex colors, pixel spacing, or pixel radii in any UI code.
