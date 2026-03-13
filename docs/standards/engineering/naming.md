# Naming Conventions

Mandatory naming rules for all identifiers in the human runtime.

**Cross-references:** [principles.md](principles.md), [anti-patterns.md](anti-patterns.md)

---

## Identifiers

| Category                      | Convention                     | Example                                         |
| ----------------------------- | ------------------------------ | ----------------------------------------------- |
| Functions, variables, fields  | `snake_case`                   | `hu_provider_create`, `message_count`           |
| Modules, files                | `snake_case`                   | `src/providers/openai.c`, `memory.h`            |
| Types, structs, enums, unions | `hu_<name>_t`                  | `hu_provider_t`, `hu_channel_t`, `hu_error_t`   |
| Constants and macros          | `HU_SCREAMING_SNAKE_CASE`      | `HU_OK`, `HU_ERR_NOT_SUPPORTED`, `HU_MAX_TOOLS` |
| Public functions              | `hu_<module>_<action>`         | `hu_provider_create`, `hu_channel_send`         |
| Factory registration keys     | Stable, lowercase, user-facing | `"openai"`, `"telegram"`, `"shell"`             |
| Test functions                | `subject_expected_behavior`    | `provider_returns_error_on_null_input`          |
| Test fixtures                 | Neutral, impersonal names      | `"test-key"`, `"example.com"`, `"user_a"`       |

---

## Right vs. Wrong

```c
// WRONG -- camelCase for C identifiers
int messageCount;
void createProvider();

// RIGHT -- snake_case
int message_count;
void hu_provider_create();
```

```c
// WRONG -- type without hu_ prefix and _t suffix
typedef struct provider { ... } provider;

// RIGHT -- canonical type naming
typedef struct hu_provider_t { ... } hu_provider_t;
```

```c
// WRONG -- magic numbers
if (status == 0)

// RIGHT -- named constants
if (status == HU_OK)
```

```c
// WRONG -- test with personal data
test_sends_email_to_john_doe()

// RIGHT -- neutral fixture
test_sends_message_to_user_a()
```
