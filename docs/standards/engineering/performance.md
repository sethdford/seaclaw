---
title: Performance Budgets
---

# Performance Budgets

Hard limits and measurement practices for the human runtime. These metrics define the product identity.

**Cross-references:** [../operations/monitoring.md](../operations/monitoring.md), [../quality/ceremonies.md](../quality/ceremonies.md)

---

## Hard Limits (the identity metrics)

| Metric                       | Target         | Max Allowed       | Measured On   |
| ---------------------------- | -------------- | ----------------- | ------------- |
| Binary size (MinSizeRel+LTO) | ~1696 KB       | 1800 KB           | macOS aarch64 |
| Text section                 | 480 KB         | 550 KB            | macOS aarch64 |
| Cold start (`--version`)     | <30 ms         | 100 ms            | macOS aarch64 |
| Peak RSS (`--version`)       | ~5.7 MB        | 8 MB              | macOS aarch64 |
| Peak RSS (test suite)        | ~6.0 MB        | 8 MB              | macOS aarch64 |
| Test throughput              | 700+ tests/sec | 500 tests/sec min | macOS aarch64 |

## Measurement Commands

```bash
# Binary size
ls -la build-release/human | awk '{print $5}'
size build-release/human  # text section

# Cold start
hyperfine --warmup 3 './build-release/human --version'

# Peak RSS
/usr/bin/time -l ./build-release/human --version 2>&1 | grep 'maximum resident'

# Test throughput
time ./build/human_tests  # divide test count by wall time
```

## CI Enforcement

- `benchmark.yml` checks binary size, startup time, RSS on every push
- Regressions >5% from baseline trigger warnings
- Regressions >10% fail the build
- Baseline values stored in CI and updated on release tags

## Per-Operation Budgets

| Operation                  | Budget                 |
| -------------------------- | ---------------------- |
| Config parse + load        | <5 ms                  |
| Provider factory lookup    | <1 ms                  |
| Tool dispatch (no network) | <2 ms                  |
| Memory retrieval (SQLite)  | <10 ms for 100 results |
| Channel send (mock)        | <1 ms                  |

## Dependency Cost Awareness

- Every new `#include` adds to binary size
- Every new dependency must justify its size cost
- Prefer compile-time polymorphism (vtables) over runtime dispatch tables
- Use `HU_ENABLE_*` flags to exclude unused subsystems

---

## Anti-Patterns

```c
// WRONG -- add a heavy dependency "because we might need it"
#include <openssl/ssl.h>  /* +200 KB for optional feature */

// RIGHT -- use HU_ENABLE_* and justify; prefer libc-only paths
#if HU_ENABLE_CURL
#include <curl/curl.h>
#endif
```

```c
// WRONG -- allocate large buffers on hot path
void process_turn(void) {
    char buf[1024 * 1024];  /* 1 MB stack -- may blow RSS */
    ...
}

// RIGHT -- use arena or bounded buffers; reuse across turns
void process_turn(hu_arena_t *arena) {
    char *buf = hu_arena_alloc(arena, 4096);  /* bounded */
    ...
}
```

```
WRONG -- ignore binary size when adding features
RIGHT -- run size check after any change; regression >5% needs justification

WRONG -- measure performance only on release builds
RIGHT -- dev builds for iteration; release builds for official baselines

WRONG -- add synchronous network calls in startup path
RIGHT -- defer network to first request; startup must stay <30 ms

WRONG -- assume "it's fast enough" without measuring
RIGHT -- use hyperfine for startup; /usr/bin/time -l for RSS; benchmark before/after
```
