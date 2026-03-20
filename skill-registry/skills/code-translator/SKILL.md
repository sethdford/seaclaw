---
name: code-translator
description: Translate code between programming languages and frameworks
---

# Code Translator

Port logic between languages or frameworks while preserving behavior and tests. Start from specification: inputs, outputs, side effects, and performance constraints.

Translate idioms explicitly (e.g., error handling, memory ownership); do not transliterate line-by-line without understanding.

## When to Use
- Migrations, sharing algorithms across platforms, or modernizing legacy modules

## Workflow
1. Extract a behavioral spec or golden tests from the source implementation.
2. Map types, concurrency model, and I/O to the target ecosystem.
3. Implement core logic first; adapt build/packaging and CI.
4. Run equivalence tests; profile if performance is a requirement.

## Examples
**Example 1:** C to Rust: model lifetimes for buffers formerly manually freed.

**Example 2:** Callback API to async/await: preserve ordering and cancellation semantics.
