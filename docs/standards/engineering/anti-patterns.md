# Anti-Patterns

Patterns explicitly prohibited in the human runtime. Each anti-pattern has a concrete reason.

**Cross-references:** [principles.md](principles.md), [naming.md](naming.md), [../quality/governance.md](../quality/governance.md)

---

## Code Anti-Patterns

| Anti-Pattern                                     | Why                                           | Do This Instead                                                           |
| ------------------------------------------------ | --------------------------------------------- | ------------------------------------------------------------------------- |
| Add C dependencies without strong justification  | Binary size impact; every dep has a cost      | Use libc; optional SQLite and libcurl only                                |
| Return vtable interfaces pointing to temporaries | Dangling pointer; callers must own the struct | Callers allocate (local var or heap); never return pointers to stack vars |
| Use `SQLITE_TRANSIENT`                           | Unnecessary copy overhead                     | Use `SQLITE_STATIC` (null) -- the binding lifetime is managed             |
| Skip `free()` on any allocation                  | ASan will catch it; leaks are bugs            | Every `malloc`/`calloc`/`strdup` has a corresponding `free`               |
| Use `-Werror` exceptions                         | Masks real issues                             | Fix warnings at the source                                                |
| Use `fprintf(stderr, ...)` for logging           | Bypasses observer system                      | Use `hu_observer_t` for all runtime logging                               |

## Architecture Anti-Patterns

| Anti-Pattern                          | Why                                                   | Do This Instead                                                                       |
| ------------------------------------- | ----------------------------------------------------- | ------------------------------------------------------------------------------------- |
| Cross-subsystem coupling              | Breaks module boundaries; creates hidden dependencies | Provider code never imports channel internals; tool code never mutates gateway policy |
| Speculative config/feature flags      | YAGNI; increases maintenance burden                   | Add flags only when a concrete caller exists                                          |
| Silently weaken security policy       | Undermines deny-by-default design                     | Keep security as strict as necessary; weaken only with explicit justification         |
| Modify unrelated modules "while here" | Scope creep; makes reviews harder                     | One concern per change                                                                |

## Test Anti-Patterns

| Anti-Pattern                                          | Why                                          | Do This Instead                                         |
| ----------------------------------------------------- | -------------------------------------------- | ------------------------------------------------------- |
| Tests that spawn real network connections             | Non-deterministic; depends on external state | Use `HU_IS_TEST` guards; mock at boundaries             |
| Tests that open browsers or spawn processes           | Side effects; unreliable in CI               | Guard with `HU_IS_TEST`                                 |
| Tests with personal/sensitive data in fixtures        | Privacy risk; makes tests non-neutral        | Use neutral placeholders: `"test-key"`, `"example.com"` |
| Tests that depend on system state (clock, filesystem) | Flaky across environments                    | Mock time; use temp directories; clean up               |

## Documentation Anti-Patterns

| Anti-Pattern                                              | Why                                              | Do This Instead                                                      |
| --------------------------------------------------------- | ------------------------------------------------ | -------------------------------------------------------------------- |
| Duplicate standards across multiple docs                  | Drift is inevitable; creates conflicting sources | One source of truth in `docs/standards/`; reference from agent files |
| Include personal identity or sensitive info in tests/docs | Privacy and security risk                        | Use neutral, impersonal examples                                     |
| Leave orphaned `.md` files                                | False confidence; stale docs mislead             | Index every doc; remove or update stale ones                         |
