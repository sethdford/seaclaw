---
title: HuLa — structured plans for the Human agent
description: How HuLa makes multi-step tool use clearer, safer, and easier to improve over time
updated: 2026-03-24
---

# HuLa — structured plans for the Human agent

**HuLa** (Human Language for agent programs) is a small **JSON format** for describing how the agent should run tools: in sequence, in parallel, with branches, loops, optional sub-agents, and named outputs. The runtime parses it, checks it against your **security policy**, runs it, and can **save traces** so you can audit behavior and turn repeated patterns into **skills**.

This guide is for **people** using or operating Human: operators, prompt authors, and anyone who wants more predictable, inspectable automation—not only for LLM training.

## Why this matters

- **Clarity:** A HuLa program is an explicit plan. You can diff it, version it, and discuss it without inferring intent from a long chat log.
- **Safety:** Every `call` goes through the same **policy and observer** path as normal tool use. Parallel steps still respect your autonomy and risk settings.
- **Honesty:** The model must use **real tool names** from your deployment. Invalid tools fail validation—Human does not silently invent capabilities.
- **Improvement:** Execution traces under `~/.human/hula_traces/` can be scanned; frequent patterns can be **promoted** into installable skill manifests (see [Emergence](#emergence-from-traces) below).

HuLa is **not** a separate “AGI layer.” It is a **structured intermediate representation** between model output and your existing tool vtables.

## Turn it on

| Setting | Effect |
| ------- | ------ |
| **`agent.hula`** in `~/.human/config.json` | **On by default** after config merge. When enabled, Human can use HuLa for eligible turns (see [Three ways HuLa runs](#three-ways-hula-runs)). Set `"hula": false` to disable. |

With HuLa enabled, the system prompt also teaches the optional **`<hula_program>...</hula_program>`** embedding convention so the model can attach a JSON program to its reply (see [Native text programs](#2-native-text-programs)).

Canonical opcode and field reference: [`docs/plans/2026-03-22-hula-language.md`](../plans/2026-03-22-hula-language.md).

## Three ways HuLa runs

### 1. LLM compiler (three or more tool calls in one turn)

When the model returns **three or more** tool calls and HuLa is enabled, Human may ask the model (JSON mode) for a **full HuLa program**, validate it, then execute it. This prefers a coherent tree over ad-hoc dispatch when compilation succeeds.

### 2. Native text programs

If the assistant message contains **no** tool calls but includes a block:

`<hula_program>{ ... valid HuLa JSON ... }</hula_program>`

Human extracts the JSON, runs it, strips the block from user-visible text, and records results. Use this when you want the **main** model to output a plan in prose plus an executable program.

### 3. Trivial IR from tool calls

When the compiler path does not run, Human can still build a minimal program (`par` or a single `call`) from the model’s tool calls and execute it through the HuLa executor—so you get **unified tracing and policy** even for simple batches.

## Command line: validate and run

You do **not** need a running agent to check JSON.

```bash
# Validate only (syntax + structure)
./build/human hula validate path/to/program.json

# Parse, validate against demo tool names, execute (demo tools: echo, search, write, analyze)
./build/human hula run path/to/program.json

# Inline JSON
./build/human hula validate '{"name":"x","version":1,"root":{"op":"call","id":"c","tool":"echo","args":{"text":"ok"}}}'

# Re-run a program from a persisted trace file (must contain a "program" object; see HU_HULA_TRACE_DIR)
./build/human hula replay ~/.human/hula_traces/some_file.json
```

Quick smoke test (requires built binary):

```bash
./scripts/hula-smoke.sh
```

**Important:** `human hula run` uses **fixed demo tool names** (`echo`, `search`, `write`, `analyze`) for the CLI. Your **live agent** only accepts tools that are actually registered for that session (e.g. `web_fetch`, `memory_store`). See [`examples/README.md`](../../examples/README.md).

## HuLa in practice (schema, lite, templates, traces UI)

### JSON Schema

The canonical machine-readable shape lives at [`schemas/hula-program.schema.json`](../../schemas/hula-program.schema.json). From a built binary, run:

```bash
./build/human hula schema
```

This prints the resolved path (repo root, parent paths, or `HUMAN_SOURCE_ROOT`) and the schema file contents when found—use it to wire editors, CI checks, or LLM JSON-mode prompts.

### HuLa lite (indentation syntax)

For hand-authored plans without raw JSON, use **lite**: two spaces per indent level, op lines such as `program <name>`, `seq <id>`, `par <id>`, `call <id> <tool>`, and child lines for `call` args (`key value ...`). Comments start with `#` after indentation.

```bash
# Compile lite → JSON on stdout
./build/human hula compile --lite examples/hula/hello.lite

# Validate or run lite sources directly
./build/human hula validate --lite examples/hula/hello.lite
./build/human hula run --lite examples/hula/hello.lite
```

### Template expansion

To fill `{{placeholders}}` in a program template (JSON text) from a small vars JSON object:

```bash
./build/human hula expand examples/hula/workflow.template.json examples/hula/workflow.vars.example.json
```

Example files live under [`examples/hula/`](../../examples/hula/).

### Persisted traces and dashboard

- **Filesystem:** Traces are written under `~/.human/hula_traces/` when `HU_HULA_TRACE_DIR` is unset (POSIX), or to the directory you set. New writes include a **`program`** field (source JSON) so **`human hula replay <file.json>`** can execute the same plan again with demo tools.
- **Gateway RPC** (control protocol): `hula.traces.list`, `hula.traces.get`, `hula.traces.delete`, `hula.traces.analytics` power the **HuLa traces** view in the web dashboard. The gateway reads the **same** trace directory as the CLI: **`HU_HULA_TRACE_DIR`** when set, otherwise `~/.human/hula_traces` (POSIX). **Observability** shows **HuLa tool turns (BTH)** with a shortcut to that view.
- **Provider JSON mode:** For compiler reliability, see [`docs/providers/hula-json-mode-matrix.md`](../providers/hula-json-mode-matrix.md).

## Opcodes at a glance

| Opcode | Meaning |
| ------ | ------- |
| `call` | Invoke one tool with JSON `args`. |
| `seq` | Run children in order; stop on first failing `call`. |
| `par` | Run children concurrently; join results. |
| `branch` | If `pred` matches prior context, run `then` or `else`. |
| `loop` | Bounded loop over `body` (use `max_iter`). |
| `delegate` | Spawn a sub-agent with a `goal` (POSIX, non-test, requires agent pool; inherits parent tools/policy when configured). |
| `emit` | Produce a string value; `emit_value` can reference prior nodes as `$node_id`. |

**Note:** `$node_id` substitution applies to **`emit_value`** and to **JSON string values inside `call` `args`** (after parse, before the tool runs). The executor picks the **longest** matching node id at each `$`; the referenced node must already be **done**. Unknown ids or nodes that are not done make the `call` fail. Expansion per string value is capped (256 KiB). After a match, the next character must **not** continue an id (`[A-Za-z0-9_]`); use a delimiter such as `.` before literal suffix text (so `$first_suf` does **not** substitute `first`—it looks for a node id `first_suf`). **Literal dollar sign:** use **`$$`** in a string value to produce a single **`$`** (for example currency `$$5` → `$5`).

**Gateway trace fetch:** optional RPC params **`trace_offset`** and **`trace_limit`** (include either to enable paging; default window size 200, max 1000) slice the **`trace`** array in the JSON file. The response adds **`trace_total_steps`**, **`trace_truncated`**, etc. Omit both params to return the full **`trace`** array (still subject to the server file size cap).

## Emergence from traces

After runs, trace JSON may be written under:

`~/.human/hula_traces/`

APIs (for tooling and advanced users):

- **`hu_hula_trace_persist`** — write a trace file (CLI, agent, and compiler paths embed a **`program`** snapshot via `hu_hula_to_json` when possible so **`hula replay`** works on `~/.human/hula_traces/*.json`).
- **`hu_hula_emergence_scan`** — find repeated tool-call n-grams in trace files.
- **`hu_hula_emergence_promote`** — materialize a pattern as `*.skill.json` and `*_HULA.md` under your skills directory.

That closes the loop from **observed behavior** to **named, reusable programs**—useful for teams standardizing workflows.

## Evaluation and teaching models

- **Suite:** `eval_suites/hula_orchestration.json` — checks whether a model can emit **HuLa-shaped** JSON (parallel gather + branch). See [`eval_suites/MANIFEST.md`](../../eval_suites/MANIFEST.md).
- **Getting started:** [Evaluation section in Getting Started](getting-started.md#8-evaluation-and-adversarial-testing).

For LLM fine-tuning or distillation, align training targets with the same JSON schema as in `src/agent/hula_compiler.c` and validate with `hu_hula_validate` / `human hula validate`.

## Production operations

- **Metacognition + telemetry:** [Metacognition and HuLa in production](../operations/metacog-hula-production.md) — BTH counters (`hula_tool_turns`), tuning, CI parity.
- **Architecture overview:** [ARCHITECTURE.md](../../ARCHITECTURE.md) (HuLa section).

## Respectful use

- **Do not** present HuLa traces or plans as proof of sentience or unstoppable autonomy; they are **machine-checkable schedules** over tools you configured.
- **Do** use traces for **debugging, compliance, and teaching**—especially when delegating to sub-agents or running parallel fetches.
- **Keep** API keys and secrets out of programs and docs; use placeholders in examples.

## See also

| Resource | Purpose |
| -------- | ------- |
| **Published docs** | [HuLa Programs](https://h-uman.ai/features/hula/) on h-uman.ai (Starlight; mirrors this guide) |
| [`docs/plans/2026-03-22-hula-language.md`](../plans/2026-03-22-hula-language.md) | Authoritative field-level spec |
| [`examples/README.md`](../../examples/README.md) | Example JSON and lite files (CLI vs agent) |
| [`examples/hula/`](../../examples/hula/) | Template + lite samples for `expand` / `--lite` |
| [`schemas/hula-program.schema.json`](../../schemas/hula-program.schema.json) | JSON Schema for programs |
| [`docs/CONCEPT_INDEX.md`](../CONCEPT_INDEX.md) | Source file map |
| [`docs/api/config.md`](../api/config.md) | `agent.hula` and related config |
