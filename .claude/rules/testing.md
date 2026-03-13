---
paths: tests/**
---

# Testing Standards

## C Tests

- Every new function needs at least one test
- Test names follow `subject_expected_behavior`
- Use `HU_IS_TEST` guards for side effects — no real network, no process spawning
- Free all allocations — ASan catches leaks
- Never assert `HU_ASSERT_TRUE(1)` or tautological conditions
- Use `tests/CLAUDE.md` for framework reference (`HU_TEST_SUITE`, `HU_RUN_TEST`, `HU_ASSERT_*`)

## Running Tests

```bash
./build/human_tests                    # all tests
./build/human_tests --suite=json       # one suite
./build/human_tests --filter=parse     # filter by name
```

Use `scripts/what-to-test.sh` to map changed files to relevant suites.

## UI Tests

- Every `hu-*` component must have tests
- Test custom element registration, property reflection, default state, ARIA attributes

## E2E Tests (Playwright)

- Include axe-core accessibility checks for every page/view
- LitElement Shadow DOM: scope selectors to host component first, then inner class
- Prefer `data-testid` attributes for E2E anchors
