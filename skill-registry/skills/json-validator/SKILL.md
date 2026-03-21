---
name: json-validator
description: Validate JSON against schemas and fix formatting
---

# Json Validator

Validate JSON documents against schemas and normalize formatting. Report paths to invalid nodes; auto-fix only safe issues (trailing commas policy per project).

Large files: validate streaming if possible to reduce memory.

## When to Use
- Config CI gates, API contract checks, or cleaning hand-edited JSON

## Workflow
1. Load schema (JSON Schema, OpenAPI fragment, or custom rules).
2. Validate instance; collect all errors, not just first failure, when tooling allows.
3. Pretty-print with stable key order if project requires deterministic diffs.
4. Add regression fixtures for each invalid case caught in prod.

## Examples
**Example 1:** Reject unknown keys in strict config mode to catch typos.

**Example 2:** Fix: normalize Unicode NFC; reject ambiguous duplicate keys.
