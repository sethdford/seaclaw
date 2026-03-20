---
name: code-review
description: Automated code review with style checks and security scanning
---

# Code Review

Apply a structured review before any merge or ship decision. Treat security and correctness as non-negotiable; style and clarity as improvements that should not block fixes for critical issues.

Prioritize findings by severity (security, data loss, crashes, incorrect behavior) then maintainability (tests, naming, duplication). Tie each comment to a concrete line or symbol when possible.

## When to Use
- Pull requests, diff review, pre-commit audits, or when the user asks for feedback on code quality
- After substantive refactors or new features touching auth, parsing, or concurrency

## Workflow
1. Map the change: intent, public API surface, data flow, and trust boundaries (network, filesystem, user input).
2. Run or infer project checks: linters, tests, and static patterns already used in the repo (read `AGENTS.md` / CI config when available).
3. Scan for common failure classes: injection, unsafe deserialization, missing validation, TOCTOU, secret logging, error swallowing, resource leaks.
4. Evaluate tests: happy path only vs edge cases; property or table-driven coverage where appropriate.
5. Summarize with **must-fix**, **should-fix**, and **nice-to-have** sections; suggest minimal patches or pseudocode, not vague advice.

## Examples
**Example 1:** A new HTTP handler: verify input schema, status codes on errors, timeouts, and that errors never echo stack traces or secrets to clients.

**Example 2:** A parser change: require tests for empty input, malformed tokens, oversized payloads, and round-trip behavior if applicable.
