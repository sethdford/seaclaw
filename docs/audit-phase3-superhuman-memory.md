# Phase 3 Superhuman Memory — Code Audit Report

**Date:** 2026-03-10  
**Scope:** All Phase 3 "Superhuman Memory" code paths  
**Test status:** 3802/3802 passing

---

## Critical (must fix)

### 1. Wrong free size in `hu_superhuman_memory_build_context` — allocator mismatch

**File:** `src/memory/superhuman.c`  
**Lines:** 1275–1277

**Description:** The function returns `*out_len = buf_len` (string length) but the buffer was allocated with `buf_cap`. Callers free with `memory_sh_len + 1`, which can be smaller than the actual allocation. This violates the allocator contract (free must receive the exact allocated size) and can cause allocator corruption or undefined behavior.

**Evidence:** `micro_moment_list`, `avoidance_list`, `topic_absence_list`, etc. return `*out_len = buf_cap` with the comment "allocated size for caller to free". `hu_superhuman_memory_build_context` is inconsistent and returns `buf_len`.

**Suggested fix:**

```c
*out = buf;
*out_len = buf_cap;  /* allocated size for caller to free */
return HU_OK;
```

Then update all callers to free with `*out_len` (not `*out_len + 1`):

- `src/agent/agent_turn.c` lines 396, 400, 407, 429–430
- `tests/test_superhuman.c` line 554

---

### 2. Potential NULL dereference in `hu_conversation_detect_topic_change`

**File:** `src/context/conversation.c`  
**Lines:** 2152–2153

**Description:** `strlen(msg_after->text)` and `strlen(msg_before->text)` are called without checking if `text` is NULL. `hu_channel_history_entry_t` defines `char text[512]`, so `text` is never NULL in normal use. However, if callers ever pass entries with uninitialized or partially filled structs, this could be unsafe. The struct is fixed-size, so this is low risk but worth a defensive check for robustness.

**Suggested fix:** Add null checks if the struct contract allows NULL text (e.g. from `load_conversation_history`). If the struct guarantees non-null, document it. Current struct uses `char text[512]` (array, not pointer), so no NULL — **downgrade to nice-to-have**.

---

## Important (should fix)

### 3. Inconsistent `*out_len` semantics across superhuman list functions

**File:** `src/memory/superhuman.c`

**Description:**

- `hu_superhuman_micro_moment_list`, `hu_superhuman_avoidance_list`, `hu_superhuman_topic_absence_list`, `hu_superhuman_growth_list_recent`, `hu_superhuman_pattern_list` return `*out_len = buf_cap` (allocated size).
- `hu_superhuman_memory_build_context` returns `*out_len = buf_len` (string length).

Callers must know which convention applies. The header or implementation should document this clearly.

**Suggested fix:** Standardize on "allocated size for free" and update `hu_superhuman_memory_build_context` as in #1.

---

### 4. Dead code in daemon avoidance injection

**File:** `src/daemon.c`  
**Lines:** 3523–3525

**Description:** `} else if (avoid_str) {` is unreachable. We are inside the `else` of `if (avoid_str)`, so `avoid_str` is NULL. The block never runs.

**Suggested fix:** Remove the dead `else if (avoid_str)` block.

---

### 5. `hu_proactive_check_curiosity` passes wrong allocator order

**File:** `src/agent/proactive.c`  
**Lines:** 448–449

**Description:** The call is `hu_superhuman_micro_moment_list(memory, alloc, ...)`. The superhuman API expects `(sqlite_ctx, alloc, ...)`. `memory` is `hu_memory_t*`; `get_db` casts it to `hu_memory_t*` and calls `hu_sqlite_memory_get_db`. If `memory` is not SQLite-backed, `get_db` returns NULL and the function returns early. **No bug** — behavior is correct.

---

### 6. Missing `#ifdef HU_ENABLE_SQLITE` around daemon superhuman calls

**File:** `src/daemon.c`

**Description:** Superhuman calls are already wrapped in `#ifdef HU_ENABLE_SQLITE` (e.g. lines 983, 1029, 1129, 2371, 2832, 3393). **No change needed.**

---

### 7. `hu_conversation_detect_inside_joke` with NULL entries

**File:** `src/memory/superhuman.c`  
**Lines:** 848–856 (inside `hu_superhuman_extract_and_store`)

**Description:** `hu_conversation_detect_inside_joke(user_msg, user_len, NULL, 0)` is called with `entries=NULL`. The function handles this: `if (entries && count > 0)` guards the shared-phrase loop. When entries is NULL, only keyword heuristics run. **No bug.**

---

### 8. `hu_superhuman_temporal_record` division when `old_count` is 0

**File:** `src/memory/superhuman.c`  
**Lines:** 347–349

**Description:** `new_avg = (old_avg * (int64_t)old_count + response_time_ms) / (int64_t)new_count`. When `old_count` is 0, `new_count` is 1, so no division by zero. **No bug.**

---

## Nice to have

### 9. Test coverage gaps

- `hu_conversation_parse_deadline` — no dedicated tests for "in X days/hours", "tonight", "this weekend".
- `hu_conversation_detect_topic_change` — no direct test.
- `hu_superhuman_temporal_record` / `hu_superhuman_temporal_get_quiet_hours` — no Phase 3 tests.
- `hu_superhuman_delayed_followup_*` — no Phase 3 tests.
- `hu_proactive_check_curiosity` / `hu_proactive_check_callbacks` — tests exist but rely on `HU_IS_TEST` for determinism.

---

### 10. Documentation

- `include/human/memory/superhuman.h`: Document that `*out_len` from list functions is the allocated size for `free()`.
- `hu_superhuman_extract_and_store`: Document that `history` is currently unused (Task 18 placeholder).

---

### 11. Logic: `hu_conversation_parse_deadline` "in 1 day" vs "in 1 days"

**File:** `src/context/conversation.c`  
**Lines:** 1480–1487

**Description:** Loop checks both "in %u day" and "in %u days". Covers "in 1 day" and "in 2 days", etc. **No bug.**

---

## Summary

| Severity  | Count |
| --------- | ----- |
| Critical  | 1     |
| Important | 2     |
| Nice      | 3     |

**Recommended immediate fix:** #1 (wrong free size in `hu_superhuman_memory_build_context`).

---

## Fixes Applied (2026-03-10)

1. **#1 Critical:** `hu_superhuman_memory_build_context` now returns `buf_cap` (allocated size) instead of `buf_len`. Callers updated to free with `*out_len`.
2. **#1 Related:** `hu_superhuman_build_context` (agent) now returns `cap` instead of `len` for consistency.
3. **#4:** Removed dead `else if (avoid_str)` block in daemon.c.
4. **Hint block fix:** Quiet-hours hint merge now uses `strlen(superhuman_ctx)` for copy length (was incorrectly using allocated size, which could include garbage and overwrite the null terminator).
