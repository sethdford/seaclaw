---
title: Example HuLa programs and fixtures
description: Which JSON files match the CLI demo tools vs a real agent catalog
updated: 2026-03-23
---

# Examples

## HuLa bundle (`hula/`)

| File | Use with |
| ---- | -------- |
| [`hula/hello.lite`](hula/hello.lite) | `human hula validate --lite` / `human hula run --lite` / `human hula compile --lite` (demo `echo`). |
| [`hula/workflow.template.json`](hula/workflow.template.json) | `human hula expand hula/workflow.template.json hula/workflow.vars.example.json` → filled JSON on stdout. |
| [`hula/workflow.vars.example.json`](hula/workflow.vars.example.json) | Vars for `expand` (copy and edit for your run). |

## HuLa programs (`hula_*.json`)

| File | Use with |
| ---- | -------- |
| [`hula_minimal.json`](hula_minimal.json) | `human hula validate` / `human hula run` — single `echo` demo call. |
| [`hula_arg_refs.json`](hula_arg_refs.json) | `human hula validate` / `human hula run` — second `echo` uses `$first` inside `args.text`. |
| [`hula_research_pipeline.json`](hula_research_pipeline.json) | `human hula run` — illustrates `seq`, `par`, `branch`, `emit` using demo tools **`search`**, **`analyze`**, **`write`** (these names match the CLI demo stubs, not every production build). |
| [`hula_parallel_fetch.json`](hula_parallel_fetch.json) | **Live agent** (when `web_fetch` is enabled) — two parallel fetches; adjust URLs for your policy. |

**Rule:** For the **interactive agent**, every `"tool"` string in a `call` node must match a tool registered in that session. Use `human --help` / your configured tool allowlist, or inspect `src/tools/factory.c` for registered names.

## Other JSON

- [`hula_loop_retry.json`](hula_loop_retry.json) — loop-oriented sample (validate tool names before running against a real agent).

Human eval fixtures for HuLa-shaped outputs live under [`eval_suites/hula_orchestration.json`](../eval_suites/hula_orchestration.json).

Full human-oriented documentation: [`docs/guides/hula.md`](../docs/guides/hula.md).
