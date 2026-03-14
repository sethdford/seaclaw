---
title: Engineering Principles
---

# Engineering Principles

Mandatory engineering principles for all code in the human runtime. These are implementation constraints, not suggestions.

**Cross-references:** [naming.md](naming.md), [anti-patterns.md](anti-patterns.md), [../quality/governance.md](../quality/governance.md)

---

## KISS

- Prefer straightforward control flow over meta-programming.
- Prefer explicit `#ifdef` branches and typed structs over hidden dynamic behavior.
- Keep error paths obvious and localized.

## YAGNI

- Do not add config keys, vtable methods, or feature flags without a concrete caller.
- Do not introduce speculative abstractions.
- Keep unsupported paths explicit (`return HU_ERR_NOT_SUPPORTED`) rather than silent no-ops.

## DRY + Rule of Three

- Duplicate small local logic when it preserves clarity.
- Extract shared helpers only after repeated, stable patterns (rule-of-three).
- When extracting, preserve module boundaries and avoid hidden coupling.

## Fail Fast + Explicit Errors

- Prefer explicit errors for unsupported or unsafe states.
- Never silently broaden permissions or capabilities.
- In tests: `HU_IS_TEST` guards are acceptable to skip side effects (e.g., spawning browsers), but the guard must be explicit and documented.

## Secure by Default + Least Privilege

- Deny-by-default for access and exposure boundaries.
- Never log secrets, raw tokens, or sensitive payloads.
- All outbound URLs must be HTTPS. HTTP is rejected at the tool layer.
- Keep network/filesystem/shell scope as narrow as possible.

## Determinism + No Flaky Tests

- Tests must not spawn real network connections, open browsers, or depend on system state.
- Use `HU_IS_TEST` to bypass side effects (spawning, opening URLs, real hardware I/O).
- Tests must be reproducible across macOS and Linux.

---

## Architecture Boundaries

- Extend capabilities by adding vtable implementations + factory wiring first.
- Keep dependency direction inward to contracts: concrete implementations depend on vtable/config/util, not on each other.
- Avoid cross-subsystem coupling (provider code importing channel internals, tool code mutating gateway policy).
- Keep module responsibilities single-purpose: orchestration in `agent/`, transport in `channels/`, model I/O in `providers/`, policy in `security/`, execution in `tools/`.
