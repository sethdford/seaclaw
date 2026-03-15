---
title: Error Handling
updated: 2026-03-13
---

# Error Handling

Patterns and rules for error propagation, reporting, and recovery in the human runtime.

**Cross-references:** [principles.md](principles.md), [anti-patterns.md](anti-patterns.md), [../../error-codes.md](../../error-codes.md)

---

## 1. Error Type System

human uses `hu_error_t` (integer return codes) as the universal error type. Every function that can fail returns `hu_error_t`.

```c
hu_error_t hu_config_parse(const char *json, hu_config_t *out);
// Returns HU_OK (0) on success, HU_ERR_* on failure
```

### Error Code Taxonomy

| Range   | Category           | Examples                                             |
| ------- | ------------------ | ---------------------------------------------------- |
| 0       | Success            | `HU_OK`                                              |
| 1-99    | General errors     | `HU_ERR_INVALID_ARGUMENT`, `HU_ERR_OUT_OF_MEMORY`    |
| 100-199 | I/O errors         | `HU_ERR_FILE_NOT_FOUND`, `HU_ERR_NETWORK`            |
| 200-299 | Security errors    | `HU_ERR_PERMISSION_DENIED`, `HU_ERR_UNAUTHORIZED`    |
| 300-399 | Provider errors    | `HU_ERR_PROVIDER_UNAVAILABLE`, `HU_ERR_RATE_LIMITED` |
| 400-499 | Tool errors        | `HU_ERR_TOOL_NOT_FOUND`, `HU_ERR_TOOL_TIMEOUT`       |
| 500+    | Subsystem-specific | See `docs/error-codes.md`                            |

Full error code catalog: [`docs/error-codes.md`](../../error-codes.md)

## 2. Propagation Rules

### Rule 1: Check every return value

```c
// GOOD
hu_error_t err = hu_config_parse(json, &config);
if (err != HU_OK) return err;

// BAD — silent failure
hu_config_parse(json, &config);
```

### Rule 2: Clean up before returning errors

```c
hu_error_t err = hu_memory_store(mem, &entry);
if (err != HU_OK) {
    free(buffer);  // Clean up allocations
    return err;    // Then propagate
}
```

### Rule 3: Never use errno for internal errors

`errno` is for libc interop only. All internal errors use `hu_error_t`.

### Rule 4: Out-parameters for rich error context

When callers need more than an error code:

```c
hu_error_t hu_tool_execute(hu_tool_t *tool, const char *params,
                           hu_tool_result_t *result);
// result->error_msg and result->error_msg_len provide context
```

## 3. Error Reporting

### Internal errors (logs)

Use structured logging via `hu_observer_t`. Never include secrets, PII, or raw user input in error messages.

```c
hu_log(HU_LOG_ERROR, "config_parse: invalid JSON at offset %zu", offset);
```

### User-facing errors (channels)

- Use natural language, not error codes
- Never expose internal state or file paths
- Suggest actionable next steps when possible

### Test errors

Use `HU_ASSERT_EQ(err, HU_OK)` for expected success. Use `HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT)` for expected failures.

## 4. Recovery Patterns

### Retry with backoff

For transient errors (network, rate limits):

```c
for (int attempt = 0; attempt < max_retries; attempt++) {
    err = hu_provider_chat(provider, &req, &resp);
    if (err == HU_OK) break;
    if (!hu_error_is_transient(err)) return err;
    hu_sleep_ms(backoff_ms << attempt);
}
```

### Circuit breaker

For provider failover, use the provider router's built-in fallback chain. Do not implement ad-hoc circuit breakers.

### Graceful degradation

When optional features fail (embedding provider down, MCP server unreachable), log and continue with reduced functionality. Never crash the runtime for optional feature failures.

## 5. Anti-Patterns

| Anti-Pattern                         | Why It's Wrong          | Do Instead                     |
| ------------------------------------ | ----------------------- | ------------------------------ |
| Ignoring return values               | Silent data corruption  | Always check and propagate     |
| `assert()` in production paths       | Crashes the runtime     | Return `hu_error_t`            |
| Generic "something went wrong"       | Unhelpful for debugging | Include error code and context |
| Logging secrets in error messages    | Security violation      | Redact before logging          |
| Returning `HU_OK` on partial failure | Caller assumes success  | Return the first error         |

## Normative References

| ID              | Source                     | Version | Relevance                                      |
| --------------- | -------------------------- | ------- | ---------------------------------------------- |
| [C11]           | ISO/IEC 9899:2011          | C11     | Error handling semantics (errno, return codes) |
| [C11-K]         | ISO/IEC 9899:2011 Annex K  | C11     | Bounds-checking interfaces pattern             |
| [CERT-C]        | SEI CERT C Coding Standard | 2016    | ERR rules (ERR30-C through ERR34-C)            |
| [Abseil-Status] | Google Abseil Status Guide | 2024    | Status-or-value error handling pattern         |
