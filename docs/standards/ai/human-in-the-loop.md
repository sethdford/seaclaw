---
title: Human-in-the-Loop
---

# Human-in-the-Loop

When agent actions require human approval, and how the approval workflow operates. The goal is maximum autonomy for safe operations with targeted human oversight where the stakes justify it.

**Cross-references:** [agent-architecture.md](agent-architecture.md), [evaluation.md](evaluation.md), [conversation-design.md](conversation-design.md), [../security/threat-model.md](../security/threat-model.md)

---

## Core Principle

**Automate the common case. Escalate the edge case.** Most agent actions are safe to execute without human review. A small percentage carry risks that justify the latency cost of asking for confirmation. The system must identify which actions fall into which category.

---

## Approval Tiers

### Tier 1: Automatic Execution (No Confirmation)

| Action                  | Condition                       | Rationale                                           |
| ----------------------- | ------------------------------- | --------------------------------------------------- |
| Respond to a message    | Standard conversation           | Core function; low stakes; correctable in next turn |
| Retrieve memory         | Any memory search               | Read-only; no side effects                          |
| Web search              | General information lookup      | Read-only; no side effects                          |
| Read a file             | Path allowed by security policy | Read-only                                           |
| List directory contents | Path allowed by security policy | Read-only                                           |

### Tier 2: Configurable Confirmation (Policy-Driven)

These actions may or may not require confirmation depending on the security policy in `hu_security_policy_t`:

| Action                               | Default Policy                      | Configurable Via           |
| ------------------------------------ | ----------------------------------- | -------------------------- |
| Execute shell command                | Confirm                             | `config.json` tool policy  |
| Write or modify a file               | Confirm                             | `config.json` tool policy  |
| Send a message on behalf of the user | Confirm                             | Per-channel policy         |
| Make an HTTP request                 | Confirm for non-allowlisted domains | Domain allowlist in config |
| Access peripheral hardware           | Confirm                             | Per-peripheral policy      |
| Delete data                          | Always confirm                      | Not configurable (safety)  |

### Tier 3: Always Confirm (Non-Overridable)

| Action                            | Rationale                                                         |
| --------------------------------- | ----------------------------------------------------------------- |
| Delete files or data              | Destructive; not easily reversible                                |
| Access credentials or secrets     | High-security surface                                             |
| Modify system configuration       | Changes agent behavior for all future interactions                |
| Execute code in a runtime sandbox | Arbitrary code execution; even sandboxed, confirmation is prudent |
| Interact with financial APIs      | Real-world consequences                                           |

---

## Confirmation Flow

### Standard Confirmation

```
1. Agent determines a tool call is needed
2. Security policy check: does this action require confirmation?
   -> No: execute immediately (Tier 1)
   -> Yes: present confirmation prompt to user
3. User receives: "[Action description]. Proceed? (yes/no)"
4. User confirms:
   -> Yes: execute the tool; include result in response
   -> No: acknowledge; suggest alternative approach
   -> Modify: user adjusts parameters; re-confirm
5. Result flows back to the agent loop normally
```

### Batch Confirmation

When multiple confirmable actions arise in a single turn:

```
WRONG -- Confirm each action one at a time (tedious for the user)
RIGHT -- Group related actions: "I'd like to: 1) Create file X, 2) Modify file Y,
  3) Run the test suite. Proceed with all, or want to pick?"
```

### Timeout Behavior

If the user doesn't respond to a confirmation prompt:

- Channel-dependent timeout (e.g., CLI waits indefinitely; Telegram times out after configurable period)
- On timeout: cancel the action, inform the agent, continue conversation
- Never execute a confirmable action without explicit approval

---

## Security Policy Integration

The confirmation system is driven by `hu_security_policy_t`, which is the single source of truth for what requires approval:

| Policy Setting      | Effect                                                                               |
| ------------------- | ------------------------------------------------------------------------------------ |
| `tool_allowlist`    | Only listed tools are available; all others blocked                                  |
| `tool_confirmation` | Per-tool override: "auto" (no confirm), "confirm" (always confirm), "deny" (blocked) |
| `domain_allowlist`  | HTTP requests to listed domains skip confirmation                                    |
| `path_allowlist`    | File operations within listed paths skip confirmation                                |
| `sandbox_mode`      | When enabled, all side-effecting tools require confirmation                          |

See `src/security/policy.c` and `include/human/security.h` for the policy implementation.

---

## Feedback Loop

User responses to confirmation prompts feed back into the system:

| Signal                                     | Action                                                             |
| ------------------------------------------ | ------------------------------------------------------------------ |
| User confirms most actions of a type       | Consider relaxing policy for that tool (inform user of the option) |
| User denies a specific action repeatedly   | Flag the pattern; the agent should learn to avoid proposing it     |
| User modifies parameters before confirming | Log the modification; inform prompt tuning                         |
| User never responds (timeout)              | Track abandonment; consider the action was unwanted                |

### Learning from Denials

When a user denies a confirmation:

```
WRONG -- Immediately retry the same action
RIGHT -- Acknowledge the denial, ask what the user wants instead

WRONG -- Silently stop trying (leaving the user's original request unaddressed)
RIGHT -- "Got it. Would you like me to try a different approach to [goal]?"
```

---

## Escalation Triggers

Beyond tool confirmation, these scenarios trigger escalation beyond the normal agent flow:

| Trigger                                             | Escalation Action                                                         |
| --------------------------------------------------- | ------------------------------------------------------------------------- |
| User reports factual error                          | Log the error; flag for memory correction; add to evaluation golden set   |
| User expresses frustration after 2+ failed attempts | Offer to reset approach or hand off to alternative assistance             |
| Safety-sensitive topic detected                     | Redirect per conversation-design.md; log for review                       |
| Security policy violation attempt                   | Block action; log via observer; alert if pattern suggests adversarial use |
| Repeated tool failures                              | Stop retrying; inform the user of the limitation; log for investigation   |

---

## Metrics

| Metric                                          | Target                                 | Measurement                                         |
| ----------------------------------------------- | -------------------------------------- | --------------------------------------------------- |
| Confirmation approval rate                      | > 90% (high = good policy calibration) | % of confirmations approved by user                 |
| Confirmation latency (user response time)       | < 30s for interactive channels         | Time from prompt to user response                   |
| False positive rate (unnecessary confirmations) | < 10%                                  | User always approves -> policy may be too strict    |
| False negative rate (missed confirmations)      | 0%                                     | Actions that should have been confirmed but weren't |
| Escalation rate                                 | Informational; no fixed target         | % of conversations requiring escalation             |

---

## Anti-Patterns

```
WRONG -- Require confirmation for every action (defeats the purpose of an autonomous agent)
RIGHT -- Define clear tiers; auto-execute safe actions, confirm risky ones

WRONG -- Execute a confirmable action because the user "probably" wants it
RIGHT -- Never assume consent. Explicit confirmation or don't execute.

WRONG -- Confirmation prompt exposes internal details: "Execute hu_tool_shell with args..."
RIGHT -- Natural language: "I'd like to run `ls -la` in your project directory. OK?"

WRONG -- After denial, do nothing and wait silently
RIGHT -- Acknowledge the denial and offer an alternative path

WRONG -- Hard-code confirmation logic in each tool implementation
RIGHT -- Confirmation is a pipeline concern handled by the security policy, not individual tools

WRONG -- No feedback mechanism for confirmation patterns
RIGHT -- Track approval/denial patterns to calibrate policy over time
```
