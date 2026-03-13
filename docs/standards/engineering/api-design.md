# C API Design

Standards for the public C API surface, stability tiers, and versioning.

**Cross-references:** [naming.md](naming.md), [principles.md](principles.md), [memory-management.md](memory-management.md)

---

## API Surface

- **Public API:** everything in `include/human/` -- these headers are the contract
- **Internal API:** `src/` internal headers -- not part of the public contract, may change freely
- **Plugin API:** `include/human/plugin.h` with `HU_PLUGIN_API_VERSION` for versioned extension

## Stability Tiers

| Tier     | Headers                                                                                                 | Promise                                                                     |
| -------- | ------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------- |
| Stable   | core/ (allocator, error, json, slice, string, arena), channel.h, tool.h, provider.h, memory.h, config.h | backward-compatible within major version                                    |
| Plugin   | plugin.h                                                                                                | versioned via HU_PLUGIN_API_VERSION; breaking changes increment the version |
| Evolving | everything else in include/human/                                                                       | may change between minor versions with notice                               |
| Internal | src/ headers                                                                                            | no stability promise                                                        |

## Versioning Policy

- **Semantic versioning:** MAJOR.MINOR.PATCH via `hu_version_string()`
- **Major bump:** breaking changes to Stable tier headers
- **Minor bump:** new features, Evolving tier changes
- **Patch bump:** bug fixes only
- **Plugin API version:** integer, incremented independently of semver when plugin ABI changes

## API Design Rules

1. Every public function returns `hu_error_t` or a result struct -- never raw ints or bools for fallible operations
2. Size parameters always accompany pointers: `(const char *data, size_t data_len)` -- never rely on null-termination alone
3. Output parameters: `foo *out` as last parameter; caller allocates, function fills
4. Ownership transfer documented in header comments: "Caller owns", "Caller frees with hu\_\*\_destroy"
5. Vtable interfaces: `{ void *ctx; const struct vtable *vtable; }` pattern
6. Factory functions: `hu_<type>_create(allocator, ...)` -> error code
7. Cleanup functions: `hu_<type>_destroy(type *)` -- always safe to call with NULL

## Deprecation Process

1. Mark deprecated functions with comment `/* DEPRECATED: use hu_foo_bar instead. Remove in vX.Y.0 */`
2. Add to deprecation list in release notes
3. Keep deprecated function working for at least one minor version
4. Remove in the version specified

---

## Anti-Patterns

```c
// WRONG -- return raw int for fallible operation
int hu_provider_chat(hu_provider_t *p, const char *msg);

// RIGHT -- return hu_error_t
hu_error_t hu_provider_chat(hu_provider_t *p, const char *msg, size_t msg_len, hu_chat_result_t *out);
```

```c
// WRONG -- rely on null-termination; no size
void hu_config_parse(const char *json);

// RIGHT -- size parameters accompany pointers
hu_error_t hu_config_parse(const char *json, size_t json_len, hu_config_t *out);
```

```c
// WRONG -- return allocated memory without documenting ownership
char *hu_tool_get_output(hu_tool_result_t *r);

// RIGHT -- document in header; caller owns
/* Caller owns; free with hu_string_free. */
char *hu_tool_get_output(hu_tool_result_t *r, hu_allocator_t *a);
```

```c
// WRONG -- destroy crashes on NULL
void hu_provider_destroy(hu_provider_t *p) {
    free(p->name);  /* crash if p == NULL */
    free(p);
}

// RIGHT -- always safe to call with NULL
void hu_provider_destroy(hu_provider_t *p) {
    if (!p) return;
    hu_string_free(p->name);
    hu_allocator_free(p->alloc, p);
}
```

```c
// WRONG -- vtable pointing to stack temporary
hu_provider_t get_provider(void) {
    hu_provider_t p = { .ctx = &(struct { int x; }){ 42 }, .vtable = &impl };
    return p;  /* ctx points to temporary; dangling after return */
}

// RIGHT -- caller allocates; vtable points to owned struct
void use_provider(void) {
    struct provider_ctx ctx = { .x = 42 };
    hu_provider_t p = { .ctx = &ctx, .vtable = &impl };
    hu_provider_chat(&p, ...);
}
```

```
WRONG -- add output parameter in the middle of the argument list
RIGHT -- output parameters last: hu_foo(input1, input2, output)

WRONG -- remove deprecated function in a patch release
RIGHT -- remove only in the version specified in the DEPRECATED comment (minor or major)

WRONG -- change Stable tier struct layout without major bump
RIGHT -- add new fields at end; never remove or reorder; major bump for breaking changes
```
