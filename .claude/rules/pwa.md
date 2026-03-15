---
paths:
  - src/pwa/**
  - include/human/pwa/**
---

# PWA Subsystem Rules

## Architecture
- Progressive web app bridge: context building, entity extraction, app drivers.
- Read `src/pwa/CLAUDE.md` for module architecture.

## Standards
- Read `docs/standards/security/ai-safety.md` — PWA context feeds into agent prompts.
- Read `docs/standards/security/data-privacy.md` — PWA accesses local app data.
- Read `docs/standards/engineering/error-handling.md` for error propagation patterns.

## Rules
- PWA context building must use `HU_IS_TEST` guards — no real app access in tests.
- Entity extraction must sanitize PII before including in agent context.
- CDP (Chrome DevTools Protocol) operations are security-sensitive — validate all inputs.
- Test file: `tests/test_pwa.c`.
