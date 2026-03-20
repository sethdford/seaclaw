---
name: test-generator
description: Generate unit tests and integration tests from source code
---

# Test Generator

Generate tests that are deterministic, fast, and aligned with the project’s test harness. Prefer testing public behavior and contracts; use fakes over real network or hardware unless the project already integrates them in CI.

Follow existing patterns in the repo (naming, fixtures, mocks). Never introduce flaky timing or dependence on external services without `HU_IS_TEST`-style guards where applicable.

## When to Use
- New modules without coverage, bug fixes that need regression tests, or refactors that must preserve behavior

## Workflow
1. Identify the unit under test and its observable outputs or side effects (pure function vs I/O).
2. List equivalence classes and boundaries: null/empty, max size, invalid encoding, error paths.
3. Implement minimal tests first (happy path + one failure mode), then expand to edge cases.
4. Run the narrowest test target locally; fix leaks and nondeterminism before claiming done.

## Examples
**Example 1:** JSON parser: tests for truncated input, duplicate keys policy, and UTF-8 edge cases.

**Example 2:** State machine: table-driven tests for each transition and illegal transition errors.
