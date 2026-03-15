---
title: AI Safety
---

# AI Safety

Security standards specific to AI agent behavior: prompt injection prevention, output validation, and safe tool execution.

**Cross-references:** [threat-model.md](threat-model.md), [../ai/hallucination-prevention.md](../ai/hallucination-prevention.md), [../ai/human-in-the-loop.md](../ai/human-in-the-loop.md)

---

## Prompt Injection Prevention

### Structured Separation

User input must never be interpolated directly into system prompts. The prompt builder (`src/persona/`) enforces this by keeping system instructions and user content in separate message roles.

```c
// WRONG -- user input mixed into system prompt
snprintf(prompt, sizeof(prompt), "You are an assistant. User says: %s", user_input);

// RIGHT -- structured message array with role separation
messages[0] = (hu_message_t){ .role = "system", .content = system_prompt };
messages[1] = (hu_message_t){ .role = "user",   .content = user_input };
```

### Input Sanitization

- Strip or escape control characters from user input before including in prompts
- Validate that tool arguments match expected schemas before execution
- Never allow user input to modify system prompt structure or tool specifications
- Log suspicious patterns (attempts to override system instructions) via `hu_observer_t`

### System Prompt Protection

- The agent must never reveal its system prompt when asked
- System prompt contents are not included in responses
- Attempts to extract the system prompt ("ignore previous instructions and...") are handled by the constraint block in the prompt, not by code inspection

---

## AI Output Validation

### Never Trust AI Output as Code

- AI-generated strings must never be passed to shell or process APIs without explicit user confirmation
- Tool dispatch validates the tool name against the registered `hu_tool_t` factory -- unknown tool names are rejected
- Tool arguments are validated against the tool's parameter schema before execution
- The security policy (`hu_security_policy_t`) gates tool execution independent of AI intent

### Structured Output Validation

When AI returns structured data (JSON, tool calls):

| Check                          | Action on Failure                                                    |
| ------------------------------ | -------------------------------------------------------------------- |
| Valid JSON                     | Re-invoke provider with stricter schema instructions (max 2 retries) |
| Expected fields present        | Fall back to unstructured response handling                          |
| Field values in expected range | Log anomaly; reject out-of-range values                              |
| Tool name in registered set    | Reject; respond with "I can't do that"                               |
| Tool arguments match schema    | Reject; log the malformed call for investigation                     |

### Error Transformation

- Never expose raw AI provider errors to the user
- Never include model-generated error explanations verbatim (may contain prompt leakage)
- Transform errors to human-friendly messages per `docs/standards/ai/conversation-design.md`
- Log full error details via `hu_observer_t` at error level for debugging

---

## Safe Tool Execution

### Security Policy Enforcement

All tool execution passes through `hu_security_policy_t` checks. The policy is the single authority on what actions are permitted:

| Policy Check                  | Enforcement                                                 |
| ----------------------------- | ----------------------------------------------------------- |
| Tool in allowlist             | Only explicitly allowed tools are available                 |
| Tool confirmation required    | Per-tool: "auto", "confirm", or "deny"                      |
| Domain allowlist (HTTP tools) | Non-allowlisted domains require confirmation                |
| Path allowlist (file tools)   | Operations outside allowed paths are blocked                |
| Sandbox mode                  | When enabled, all side-effecting tools require confirmation |

### Sandbox Isolation

When running in sandbox mode:

- File access restricted to configured paths
- Network access restricted to allowlisted domains
- Shell tool restricted or blocked entirely
- Process spawning disabled
- Hardware peripheral access gated per device

See `docs/standards/security/sandbox.md` for sandbox backend details.

### Tool Execution Logging

Every tool execution logs via `hu_observer_t`:

- Tool name
- Sanitized argument summary (never log secrets or full file contents)
- Result status (success/failure)
- Duration
- Whether confirmation was required and the user's response

---

## Data Protection in AI Context

### What Never Enters a Prompt

- API keys, tokens, passwords, or secrets
- Raw credentials from `hu_secrets_t`
- Other users' private data (multi-user scenarios)
- Full file contents of sensitive files (`.env`, credentials)

### What Gets Redacted in Logs

- Full user messages at production log levels (use hash or truncated preview)
- Full AI responses at info level (debug only)
- Tool arguments containing file paths outside the sandbox
- Personal identifiers (names, emails, phone numbers)

### Memory Isolation

- `hu_memory_t` stores per-persona, per-user data
- Memory retrieval queries are scoped to the active persona and user
- Cross-persona memory access is not permitted without explicit configuration
- Stored secrets are encrypted at rest via `hu_secrets_t`

---

## Anti-Patterns

```
WRONG -- Interpolate user input directly into system prompts
RIGHT -- Structured message roles with system/user separation

WRONG -- Trust AI tool call output without validation
RIGHT -- Validate tool name, argument schema, and security policy before execution

WRONG -- Pass AI-generated strings to shell APIs without confirmation
RIGHT -- AI-generated commands require explicit user confirmation and sandbox constraints

WRONG -- Log full user messages and AI responses in production
RIGHT -- Log identifiers and hashes; full content at debug level only

WRONG -- Allow the AI to decide its own permissions
RIGHT -- Security policy is the authority; AI intent does not override policy

WRONG -- Expose raw provider errors to users
RIGHT -- Transform to friendly messages; log full details for debugging
```

## Normative References

| ID              | Source                               | Version          | Relevance                                                         |
| --------------- | ------------------------------------ | ---------------- | ----------------------------------------------------------------- |
| [OWASP-LLM]     | OWASP Top 10 for LLM Applications    | 1.1 (2023-10)    | LLM vulnerability taxonomy (prompt injection, data leakage, etc.) |
| [Anthropic-RSP] | Anthropic Responsible Scaling Policy | v1.0 (2023-09)   | AI capability evaluation and safety thresholds                    |
| [NIST-AI-RMF]   | NIST AI Risk Management Framework    | 1.0 (2023-01-26) | AI risk identification and mitigation                             |
| [Google-SAIF]   | Google Secure AI Framework           | 1.0 (2023-06)    | AI system security principles                                     |
