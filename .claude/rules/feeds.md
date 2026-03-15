---
paths:
  - src/feeds/**
  - include/human/feeds/**
---

# Feeds Subsystem Rules

## Architecture

- Feed processor (`hu_feed_processor_t`) with interest-based relevance filtering.
- Sources: social, google, apple, research, file ingest, gmail, imessage, twitter.
- Read `src/feeds/CLAUDE.md` for module architecture.

## Standards

- Read `docs/standards/security/data-privacy.md` — feeds process user content.
- Read `docs/standards/security/compliance.md` — PII handling in feed data.
- Read `docs/standards/engineering/testing.md` before adding tests.

## Rules

- All feed sources must use `HU_IS_TEST` guards — no real network in tests.
- Feed content must be sanitized before storage in memory.
- Relevance filtering must be deterministic for test reproducibility.
- Test files: `tests/test_feeds.c`, `tests/test_feed_processor.c`, `tests/test_research_feeds.c`.
