---
title: Agent Architecture
---

# Agent Architecture

Standards for designing, orchestrating, and governing the agent pipeline within the human runtime.

**Cross-references:** [evaluation.md](evaluation.md), [hallucination-prevention.md](hallucination-prevention.md), [human-in-the-loop.md](human-in-the-loop.md), [prompt-engineering.md](prompt-engineering.md)

---

## Architecture Principles

1. **Single responsibility.** Each component does one thing well. The agent loop orchestrates; providers handle model I/O; tools execute actions; memory retrieves context. If a component has two distinct jobs, split it.
2. **Typed contracts.** Inter-component communication uses typed structs (`hu_*_t`), not free-form strings. Vtable interfaces define the contract surface.
3. **Fail-safe defaults.** If a provider fails, the pipeline falls back to the next configured provider -- never to silence. `HU_ERR_*` codes propagate explicitly.
4. **Observability.** Every provider invocation is loggable via `hu_observer_t`: input tokens, output tokens, latency, model name, error code.
5. **Human circuit-breaker.** High-stakes tool executions (shell, file write, network requests) require confirmation via the security policy. See [human-in-the-loop.md](human-in-the-loop.md).

---

## Agent Pipeline

```
User message (via hu_channel_t)
  -> Context assembly (hu_memory_t retrieval + persona + channel overlay)
  -> Provider invocation (hu_provider_t.chat)
  -> Tool dispatch loop (hu_tool_t.execute, if tool calls returned)
  -> Response delivery (hu_channel_t.send)
  -> Memory persistence (hu_memory_t.store)
```

### Component Responsibilities

| Component      | Vtable          | Responsibility                                 | Boundary                                               |
| -------------- | --------------- | ---------------------------------------------- | ------------------------------------------------------ |
| **Agent loop** | `hu_agent_t`    | Orchestration, turn management, context window | Never calls model APIs directly; delegates to provider |
| **Provider**   | `hu_provider_t` | Model I/O, streaming, token counting           | Never reads memory or executes tools                   |
| **Tool**       | `hu_tool_t`     | Single action execution, input validation      | Never calls providers or reads conversation history    |
| **Memory**     | `hu_memory_t`   | Storage, retrieval, embedding, search          | Never modifies conversation flow                       |
| **Channel**    | `hu_channel_t`  | Message transport, formatting                  | Never processes AI logic                               |
| **Persona**    | `hu_persona_t`  | Identity, traits, voice, overlays              | Provides prompt material; never modifies pipeline flow |
| **Observer**   | `hu_observer_t` | Logging, metrics, tracing                      | Read-only; never modifies pipeline state               |

---

## Inter-Component Contracts

Every component boundary uses a typed C struct. No free-form string passing between components.

### Channel -> Agent

```c
// Channel delivers a message to the agent loop
typedef struct {
    const char *text;           // user message content
    const char *channel_name;   // "telegram", "discord", "cli", etc.
    const char *sender_id;      // channel-specific user identifier
    const char *conversation_id;// channel-specific conversation identifier
    hu_json_t *metadata;        // channel-specific metadata (optional)
} hu_incoming_message_t;
```

### Agent -> Provider

```c
// Agent assembles context and invokes the provider
typedef struct {
    const char *system_prompt;  // assembled from persona + context
    hu_message_t *messages;     // conversation history
    size_t message_count;
    hu_tool_spec_t *tools;      // available tool specifications
    size_t tool_count;
    float temperature;          // 0.0-1.0
    int max_tokens;             // response token limit
} hu_chat_request_t;
```

### Provider -> Agent (tool call)

```c
// Provider returns a tool call request
typedef struct {
    const char *tool_name;
    const char *arguments_json; // validated JSON string
    const char *call_id;        // provider-assigned correlation ID
} hu_tool_call_t;
```

### Agent -> Tool

```c
// Agent dispatches tool execution
typedef struct {
    const char *input_json;     // validated, sanitized arguments
    hu_security_policy_t *policy; // active security policy
    bool is_test;               // HU_IS_TEST guard
} hu_tool_input_t;

// Tool returns a result
typedef struct {
    hu_error_t error;           // HU_OK or specific error code
    const char *output;         // result text or JSON
    bool needs_confirmation;    // tool requests human approval
} hu_tool_result_t;
```

---

## Orchestration Patterns

### Sequential Turn (Default)

```
1. Receive message via channel
2. Retrieve relevant memory (hu_memory_t.search)
3. Build system prompt (persona + overlay + memory context)
4. Call provider (hu_provider_t.chat)
5. If tool calls returned:
   a. Validate tool call against security policy
   b. Execute tool (hu_tool_t.execute)
   c. Append tool result to context
   d. Re-invoke provider with tool results
   e. Repeat until no more tool calls (max depth: configurable, default 10)
6. Deliver response via channel
7. Store turn in memory
```

### Retry and Fallback

| Scenario                            | Action                                            | Max Retries    |
| ----------------------------------- | ------------------------------------------------- | -------------- |
| Provider returns malformed response | Re-invoke with same context                       | 2              |
| Provider timeout                    | Retry with exponential backoff                    | 2              |
| Provider returns empty response     | Fall back to next configured provider             | 1 per fallback |
| Tool execution fails                | Return error description to provider for recovery | 1              |
| All providers exhausted             | Return graceful error message to user             | 0              |

### Fallback Hierarchy

```
1. Primary provider (user-configured)
2. Fallback provider (if configured in config.json)
3. Graceful error message ("I'm having trouble responding right now.")
```

Never fail silently. If the pipeline degrades, the user receives a response at the appropriate quality level or a clear error message.

---

## Temperature and Sampling Guidelines

| Use Case                        | Recommended Temperature | Rationale                               |
| ------------------------------- | ----------------------- | --------------------------------------- |
| Factual Q&A, tool dispatch      | 0.0-0.1                 | Deterministic; accuracy over creativity |
| General conversation            | 0.3-0.5                 | Natural variation within persona voice  |
| Creative writing, brainstorming | 0.5-0.8                 | Diversity of output desired             |
| Code generation                 | 0.0-0.2                 | Precision required                      |
| Classification, routing         | 0.0                     | Must be consistent                      |

Persona overlays may specify temperature preferences per channel. The agent loop applies the most specific setting available: tool-specific > channel overlay > persona default > global default.

---

## Adding a New Component

When adding a new vtable implementation:

1. **Define the responsibility** in a single sentence. If it needs "and," consider splitting.
2. **Implement the vtable struct** with all required function pointers.
3. **Register in the factory** (`src/*/factory.c`).
4. **Add `HU_IS_TEST` guards** for any side effects (network, filesystem, hardware).
5. **Add tests** for vtable wiring, error paths, and graceful degradation.
6. **Update this document** if the component changes the pipeline flow.

---

## Monitoring and Observability

### Per-Invocation Metrics (via hu_observer_t)

| Metric                        | Collection Point                | Alert Threshold          |
| ----------------------------- | ------------------------------- | ------------------------ |
| Provider latency (ms)         | Every `hu_provider_t.chat` call | P95 > 2x expected        |
| Token usage (input + output)  | Every provider call             | > 150% of context window |
| Error rate                    | Every provider call             | > 5% over 1-hour window  |
| Tool execution count per turn | Every turn                      | > 10 (possible loop)     |
| Fallback activations          | Every provider fallback         | > 10% of requests        |
| Memory retrieval latency      | Every `hu_memory_t.search` call | P95 > 500ms              |

---

## Anti-Patterns

```
WRONG -- Tools call providers directly to "enhance" their output
RIGHT -- Tools execute their action and return a result; the agent loop handles all provider interaction

WRONG -- Provider implementation reads from hu_memory_t
RIGHT -- The agent loop retrieves memory and includes it in the prompt; providers are stateless

WRONG -- Channel implementation processes AI logic or modifies messages semantically
RIGHT -- Channels transport messages; the agent loop handles all AI logic

WRONG -- Retry indefinitely when a provider fails
RIGHT -- Max 2 retries per provider, then fall back; max depth for the full fallback chain

WRONG -- Same temperature for all use cases
RIGHT -- Match temperature to the task: low for factual/tool work, higher for creative/conversational

WRONG -- Add a component without typed contracts and tests
RIGHT -- Every new component comes with a vtable struct, factory registration, and test coverage
```
