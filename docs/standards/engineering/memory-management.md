---
title: Memory Management
---

# Memory Management

Standards for allocation, ownership, and cleanup across the human runtime.

**Cross-references:** [principles.md](principles.md), [anti-patterns.md](anti-patterns.md), [testing.md](testing.md)

---

## Allocator Types

- **`hu_allocator_t` vtable:** `alloc`, `realloc`, `free` with ctx pointer and size tracking
- **`hu_system_allocator()`** -- default, backed by libc malloc/free
- **`hu_tracking_allocator_t`** -- wraps any allocator, counts allocs/frees, reports leaks via `hu_tracking_allocator_leaks()`
- **`hu_arena_t`** -- bulk ephemeral allocator: `hu_arena_create` -> `hu_arena_allocator` -> use -> `hu_arena_reset`/`hu_arena_destroy`

## When to Use Each

| Allocator | Use When                                                | Lifetime                          | Example                                       |
| --------- | ------------------------------------------------------- | --------------------------------- | --------------------------------------------- |
| System    | long-lived objects, config, vtable structs              | process lifetime or explicit free | provider creation, config parsing             |
| Arena     | per-turn ephemeral data, JSON assembly, prompt building | function/turn scope, bulk-freed   | agent_turn.c context assembly, config_merge.c |
| Tracking  | tests only, leak detection                              | test scope                        | test_memory.c, test_tools.c                   |

## Ownership Rules

1. **Creator owns** -- whoever allocates is responsible for freeing
2. **Transfer via `_owned` flags** -- `hu_tool_result_t.output_owned = true` means caller must free; `false` means borrowed/static
3. **Vtable `deinit`/`destroy`** -- every vtable type has a cleanup function; always call it
4. **"Caller owns" / "Caller frees"** -- when a function returns allocated memory, the header comment says who frees
5. **Never return pointers to stack locals or temporaries** -- dangling pointer

## Cleanup Patterns

- **Goto-cleanup pattern:** single cleanup label at end of function, `goto cleanup` on error
- **Early-return only** when no allocations have been made yet
- **Always free in reverse allocation order**
- **Arena bulk free:** `hu_arena_destroy()` frees everything at once

## ASan Integration

- Dev builds enable AddressSanitizer by default
- Zero ASan errors required -- leaks, overflows, use-after-free are all build failures
- Use tracking allocator in tests to assert `hu_tracking_allocator_leaks(ta) == 0`

---

## Anti-Patterns

```c
// WRONG -- return pointer to stack local
const char *get_name(void) {
    char buf[64];
    snprintf(buf, sizeof(buf), "provider-%d", id);
    return buf;  /* dangling pointer */
}

// RIGHT -- allocate via allocator, document caller-frees
/* Caller owns; free with hu_string_free. */
char *get_name(hu_allocator_t *a) {
    char *buf = hu_allocator_alloc(a, 64);
    if (!buf) return NULL;
    snprintf(buf, 64, "provider-%d", id);
    return buf;
}
```

```c
// WRONG -- forget to free on error path
hu_error_t do_work(void) {
    char *buf = malloc(256);
    if (parse(buf) != HU_OK) return HU_ERR_PARSE;  /* leak */
    use(buf);
    free(buf);
    return HU_OK;
}

// RIGHT -- goto-cleanup pattern
hu_error_t do_work(void) {
    char *buf = NULL;
    hu_error_t err = HU_OK;
    buf = malloc(256);
    if (!buf) { err = HU_ERR_NO_MEM; goto cleanup; }
    if (parse(buf) != HU_OK) { err = HU_ERR_PARSE; goto cleanup; }
    use(buf);
cleanup:
    free(buf);
    return err;
}
```

```c
// WRONG -- use arena for long-lived data
hu_arena_t arena;
hu_arena_create(&arena, sys_alloc, 4096);
hu_provider_t *p = hu_arena_alloc(&arena, sizeof(hu_provider_t));
/* ... p outlives arena_destroy ... */
hu_arena_destroy(&arena);  /* p now dangling */

// RIGHT -- use system allocator for data that outlives the turn
hu_provider_t *p = hu_allocator_alloc(sys_alloc, sizeof(hu_provider_t));
/* ... p lives until explicit hu_provider_destroy ... */
hu_provider_destroy(p);
hu_allocator_free(sys_alloc, p);
```

```c
// WRONG -- skip tracking allocator in tests
void test_tool_execute(void) {
    hu_tool_t tool;
    hu_tool_create(&tool, sys_alloc, "shell");
    hu_tool_result_t r;
    hu_tool_execute(&tool, "echo hi", &r);
    /* no leak check */
}

// RIGHT -- every test uses tracking allocator and asserts zero leaks
void test_tool_execute(void) {
    hu_tracking_allocator_t ta;
    hu_tracking_allocator_init(&ta, hu_system_allocator());
    hu_tool_t tool;
    hu_tool_create(&tool, hu_tracking_allocator_get(&ta), "shell");
    hu_tool_result_t r;
    hu_tool_execute(&tool, "echo hi", &r);
    hu_tool_destroy(&tool);
    hu_tool_result_free(&r, hu_tracking_allocator_get(&ta));
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(&ta), 0);
    hu_tracking_allocator_deinit(&ta);
}
```

```c
// WRONG -- free arena-allocated pointers individually
hu_arena_t arena;
hu_arena_create(&arena, sys_alloc, 4096);
char *a = hu_arena_alloc(&arena, 64);
char *b = hu_arena_alloc(&arena, 64);
free(a);  /* undefined -- arena owns these */
free(b);
hu_arena_destroy(&arena);

// RIGHT -- let arena_destroy handle bulk free
hu_arena_t arena;
hu_arena_create(&arena, sys_alloc, 4096);
char *a = hu_arena_alloc(&arena, 64);
char *b = hu_arena_alloc(&arena, 64);
/* use a, b */
hu_arena_destroy(&arena);  /* frees everything */
```
