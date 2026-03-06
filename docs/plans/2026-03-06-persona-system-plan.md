# Persona System Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a data-driven persona system that lets seaclaw adopt a user's real personality by analyzing their message history, with per-agent personas and per-channel style overlays.

**Architecture:** Structured persona profiles (`~/.seaclaw/personas/*.json`) with curated example banks. A creation pipeline ingests messages from iMessage/Gmail/Facebook, analyzes them via the AI provider, and generates the profile + examples. At runtime, the persona is loaded and injected into the system prompt alongside channel-matched few-shot examples.

**Tech Stack:** C11, SQLite (iMessage DB), libcurl (Gmail), JSON parsing via existing `sc_json_*` utilities.

**Design doc:** `docs/plans/2026-03-06-persona-system-design.md`

---

## Task 1: Core Types and Header

Define the public persona types and function signatures.

**Files:**

- Create: `include/seaclaw/persona.h`
- Test: `tests/test_persona.c`

**Step 1: Write the failing test**

Create `tests/test_persona.c`:

```c
#include "test_helpers.h"
#include "seaclaw/persona.h"

static void test_persona_types_exist(void) {
    sc_persona_overlay_t overlay = {0};
    SC_ASSERT(overlay.channel == NULL);

    sc_persona_example_t example = {0};
    SC_ASSERT(example.context == NULL);

    sc_persona_t persona = {0};
    SC_ASSERT(persona.name == NULL);
    SC_ASSERT(persona.overlays_count == 0);
    SC_PASS();
}

void run_persona_tests(void) {
    SC_TEST(test_persona_types_exist);
}
```

**Step 2: Run test to verify it fails**

Run: `cd build && cmake .. -DSC_ENABLE_ALL_CHANNELS=ON && cmake --build . -j$(nproc) && ./seaclaw_tests`
Expected: FAIL — `seaclaw/persona.h` not found.

**Step 3: Write minimal implementation**

Create `include/seaclaw/persona.h`:

```c
#ifndef SC_PERSONA_H
#define SC_PERSONA_H

#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct sc_persona_overlay {
    char *channel;
    char *formality;       /* "casual", "professional", "very formal" */
    char *avg_length;      /* "short", "medium", "long" */
    char *emoji_usage;     /* "none", "minimal", "moderate", "heavy" */
    char **style_notes;
    size_t style_notes_count;
} sc_persona_overlay_t;

typedef struct sc_persona_example {
    char *context;
    char *incoming;
    char *response;
} sc_persona_example_t;

typedef struct sc_persona_example_bank {
    char *channel;
    sc_persona_example_t *examples;
    size_t examples_count;
} sc_persona_example_bank_t;

typedef struct sc_persona {
    char *name;
    size_t name_len;

    /* Core identity */
    char *identity;
    char **traits;
    size_t traits_count;
    char **preferred_vocab;
    size_t preferred_vocab_count;
    char **avoided_vocab;
    size_t avoided_vocab_count;
    char **slang;
    size_t slang_count;
    char **communication_rules;
    size_t communication_rules_count;
    char **values;
    size_t values_count;
    char *decision_style;

    /* Channel overlays */
    sc_persona_overlay_t *overlays;
    size_t overlays_count;

    /* Example banks (one per channel) */
    sc_persona_example_bank_t *example_banks;
    size_t example_banks_count;
} sc_persona_t;

/* Load a persona from ~/.seaclaw/personas/<name>.json */
sc_error_t sc_persona_load(sc_allocator_t *alloc, const char *name, size_t name_len,
                           sc_persona_t *out);

/* Free all owned memory in a persona */
void sc_persona_deinit(sc_allocator_t *alloc, sc_persona_t *persona);

/* Build a persona prompt string for injection into the system prompt.
   If channel is non-NULL, includes the matching overlay. */
sc_error_t sc_persona_build_prompt(sc_allocator_t *alloc, const sc_persona_t *persona,
                                   const char *channel, size_t channel_len,
                                   char **out, size_t *out_len);

/* Select up to max_examples from the example bank matching channel.
   Returns pointers into the persona's example bank (not copies). */
sc_error_t sc_persona_select_examples(const sc_persona_t *persona,
                                      const char *channel, size_t channel_len,
                                      const char *topic, size_t topic_len,
                                      const sc_persona_example_t **out,
                                      size_t *out_count, size_t max_examples);

/* Find overlay by channel name. Returns NULL if not found. */
const sc_persona_overlay_t *sc_persona_find_overlay(const sc_persona_t *persona,
                                                    const char *channel, size_t channel_len);

#endif /* SC_PERSONA_H */
```

**Step 4: Wire into build**

Add `tests/test_persona.c` to `SC_TEST_SOURCES` in `CMakeLists.txt` (near the other test files around line 914).

Add `void run_persona_tests(void);` declaration and `run_persona_tests();` call in `tests/test_main.c`.

**Step 5: Run test to verify it passes**

Run: `cd build && cmake .. -DSC_ENABLE_ALL_CHANNELS=ON && cmake --build . -j$(nproc) && ./seaclaw_tests`
Expected: PASS

**Step 6: Commit**

```bash
git add include/seaclaw/persona.h tests/test_persona.c CMakeLists.txt tests/test_main.c
git commit -m "feat(persona): add core persona types and header"
```

---

## Task 2: Persona Deinit and Overlay Lookup

Implement memory cleanup and overlay lookup — the simplest functions first.

**Files:**

- Create: `src/persona/persona.c`
- Modify: `tests/test_persona.c`
- Modify: `CMakeLists.txt` (add `src/persona/persona.c` to `SC_CORE_SOURCES` near line 413)

**Step 1: Write the failing tests**

Add to `tests/test_persona.c`:

```c
static void test_persona_find_overlay_found(void) {
    sc_persona_overlay_t overlays[] = {
        {.channel = "imessage", .formality = "casual"},
        {.channel = "gmail", .formality = "professional"},
    };
    sc_persona_t p = {.overlays = overlays, .overlays_count = 2};
    const sc_persona_overlay_t *o = sc_persona_find_overlay(&p, "gmail", 5);
    SC_ASSERT(o != NULL);
    SC_ASSERT(strcmp(o->formality, "professional") == 0);
    SC_PASS();
}

static void test_persona_find_overlay_not_found(void) {
    sc_persona_overlay_t overlays[] = {
        {.channel = "imessage", .formality = "casual"},
    };
    sc_persona_t p = {.overlays = overlays, .overlays_count = 1};
    const sc_persona_overlay_t *o = sc_persona_find_overlay(&p, "slack", 5);
    SC_ASSERT(o == NULL);
    SC_PASS();
}

static void test_persona_deinit_null_safe(void) {
    sc_allocator_t alloc = sc_default_allocator();
    sc_persona_t p = {0};
    sc_persona_deinit(&alloc, &p); /* must not crash */
    SC_PASS();
}
```

Register in `run_persona_tests`:

```c
void run_persona_tests(void) {
    SC_TEST(test_persona_types_exist);
    SC_TEST(test_persona_find_overlay_found);
    SC_TEST(test_persona_find_overlay_not_found);
    SC_TEST(test_persona_deinit_null_safe);
}
```

**Step 2: Run test to verify it fails**

Run: `cd build && cmake --build . -j$(nproc) && ./seaclaw_tests`
Expected: FAIL — linker errors for `sc_persona_find_overlay`, `sc_persona_deinit`.

**Step 3: Write minimal implementation**

Create `src/persona/persona.c`:

```c
#include "seaclaw/persona.h"
#include "seaclaw/core/allocator.h"

#include <string.h>

const sc_persona_overlay_t *sc_persona_find_overlay(const sc_persona_t *persona,
                                                    const char *channel, size_t channel_len) {
    if (!persona || !channel)
        return NULL;
    for (size_t i = 0; i < persona->overlays_count; i++) {
        const sc_persona_overlay_t *o = &persona->overlays[i];
        if (o->channel && strlen(o->channel) == channel_len &&
            memcmp(o->channel, channel, channel_len) == 0)
            return o;
    }
    return NULL;
}

static void free_string_array(sc_allocator_t *a, char **arr, size_t count) {
    if (!arr)
        return;
    for (size_t i = 0; i < count; i++) {
        if (arr[i])
            a->free(a->ctx, arr[i], strlen(arr[i]) + 1);
    }
    a->free(a->ctx, arr, count * sizeof(char *));
}

static void deinit_overlay(sc_allocator_t *a, sc_persona_overlay_t *o) {
    if (o->channel) a->free(a->ctx, o->channel, strlen(o->channel) + 1);
    if (o->formality) a->free(a->ctx, o->formality, strlen(o->formality) + 1);
    if (o->avg_length) a->free(a->ctx, o->avg_length, strlen(o->avg_length) + 1);
    if (o->emoji_usage) a->free(a->ctx, o->emoji_usage, strlen(o->emoji_usage) + 1);
    free_string_array(a, o->style_notes, o->style_notes_count);
}

static void deinit_example_bank(sc_allocator_t *a, sc_persona_example_bank_t *bank) {
    if (bank->channel) a->free(a->ctx, bank->channel, strlen(bank->channel) + 1);
    for (size_t i = 0; i < bank->examples_count; i++) {
        sc_persona_example_t *e = &bank->examples[i];
        if (e->context) a->free(a->ctx, e->context, strlen(e->context) + 1);
        if (e->incoming) a->free(a->ctx, e->incoming, strlen(e->incoming) + 1);
        if (e->response) a->free(a->ctx, e->response, strlen(e->response) + 1);
    }
    if (bank->examples)
        a->free(a->ctx, bank->examples, bank->examples_count * sizeof(sc_persona_example_t));
}

void sc_persona_deinit(sc_allocator_t *alloc, sc_persona_t *p) {
    if (!p)
        return;
    if (p->name) alloc->free(alloc->ctx, p->name, p->name_len + 1);
    if (p->identity) alloc->free(alloc->ctx, p->identity, strlen(p->identity) + 1);
    free_string_array(alloc, p->traits, p->traits_count);
    free_string_array(alloc, p->preferred_vocab, p->preferred_vocab_count);
    free_string_array(alloc, p->avoided_vocab, p->avoided_vocab_count);
    free_string_array(alloc, p->slang, p->slang_count);
    free_string_array(alloc, p->communication_rules, p->communication_rules_count);
    free_string_array(alloc, p->values, p->values_count);
    if (p->decision_style) alloc->free(alloc->ctx, p->decision_style, strlen(p->decision_style) + 1);
    for (size_t i = 0; i < p->overlays_count; i++)
        deinit_overlay(alloc, &p->overlays[i]);
    if (p->overlays)
        alloc->free(alloc->ctx, p->overlays, p->overlays_count * sizeof(sc_persona_overlay_t));
    for (size_t i = 0; i < p->example_banks_count; i++)
        deinit_example_bank(alloc, &p->example_banks[i]);
    if (p->example_banks)
        alloc->free(alloc->ctx, p->example_banks, p->example_banks_count * sizeof(sc_persona_example_bank_t));
    memset(p, 0, sizeof(*p));
}
```

**Step 4: Run test to verify it passes**

Run: `cd build && cmake --build . -j$(nproc) && ./seaclaw_tests`
Expected: PASS, 0 ASan errors.

**Step 5: Commit**

```bash
git add src/persona/persona.c tests/test_persona.c CMakeLists.txt
git commit -m "feat(persona): implement deinit and overlay lookup"
```

---

## Task 3: Persona JSON Loading

Parse `persona.json` files into `sc_persona_t`.

**Files:**

- Modify: `src/persona/persona.c`
- Modify: `tests/test_persona.c`

**Step 1: Write the failing test**

Add to `tests/test_persona.c`:

```c
static void test_persona_load_from_json(void) {
    sc_allocator_t alloc = sc_default_allocator();
    const char *json =
        "{"
        "  \"version\": 1,"
        "  \"name\": \"test-user\","
        "  \"core\": {"
        "    \"identity\": \"A test persona\","
        "    \"traits\": [\"direct\", \"curious\"],"
        "    \"vocabulary\": {"
        "      \"preferred\": [\"solid\", \"cool\"],"
        "      \"avoided\": [\"synergy\"],"
        "      \"slang\": [\"ngl\"]"
        "    },"
        "    \"communication_rules\": [\"Keep it short\"],"
        "    \"values\": [\"honesty\"],"
        "    \"decision_style\": \"Decides fast\""
        "  },"
        "  \"channel_overlays\": {"
        "    \"imessage\": {"
        "      \"formality\": \"casual\","
        "      \"avg_length\": \"short\","
        "      \"emoji_usage\": \"minimal\","
        "      \"style_notes\": [\"drops punctuation\"]"
        "    }"
        "  }"
        "}";

    sc_persona_t p = {0};
    sc_error_t err = sc_persona_load_json(&alloc, json, strlen(json), &p);
    SC_ASSERT(err == SC_OK);
    SC_ASSERT(strcmp(p.name, "test-user") == 0);
    SC_ASSERT(p.traits_count == 2);
    SC_ASSERT(strcmp(p.traits[0], "direct") == 0);
    SC_ASSERT(strcmp(p.identity, "A test persona") == 0);
    SC_ASSERT(p.preferred_vocab_count == 2);
    SC_ASSERT(p.avoided_vocab_count == 1);
    SC_ASSERT(p.slang_count == 1);
    SC_ASSERT(p.communication_rules_count == 1);
    SC_ASSERT(p.values_count == 1);
    SC_ASSERT(strcmp(p.decision_style, "Decides fast") == 0);
    SC_ASSERT(p.overlays_count == 1);
    SC_ASSERT(strcmp(p.overlays[0].channel, "imessage") == 0);
    SC_ASSERT(strcmp(p.overlays[0].formality, "casual") == 0);
    SC_ASSERT(p.overlays[0].style_notes_count == 1);
    sc_persona_deinit(&alloc, &p);
    SC_PASS();
}
```

Also add the declaration to `include/seaclaw/persona.h`:

```c
/* Parse persona from a JSON string (used by sc_persona_load and tests) */
sc_error_t sc_persona_load_json(sc_allocator_t *alloc, const char *json, size_t json_len,
                                sc_persona_t *out);
```

**Step 2: Run test to verify it fails**

Run: `cd build && cmake --build . -j$(nproc) && ./seaclaw_tests`
Expected: FAIL — `sc_persona_load_json` undefined.

**Step 3: Write minimal implementation**

Add to `src/persona/persona.c` — a JSON parser that uses the existing `sc_json_*` API to populate `sc_persona_t`. Pattern: parse root object, extract `core` sub-object, iterate `channel_overlays` keys.

Refer to existing JSON parsing in `src/config.c:877-924` (`parse_agent`) for the idiomatic pattern of using `sc_json_get_string`, `sc_json_get_bool`, etc. Use `sc_strdup` for all owned strings. Use `sc_json_object_get`, `sc_json_array_get`, `sc_json_array_length` for arrays. Refer to `include/seaclaw/core/json.h` for available JSON functions.

**Step 4: Run test to verify it passes**

Run: `cd build && cmake --build . -j$(nproc) && ./seaclaw_tests`
Expected: PASS, 0 ASan errors.

**Step 5: Commit**

```bash
git add src/persona/persona.c include/seaclaw/persona.h tests/test_persona.c
git commit -m "feat(persona): implement JSON loading for persona profiles"
```

---

## Task 4: Persona File Loading (`sc_persona_load`)

Load persona from `~/.seaclaw/personas/<name>.json` on disk.

**Files:**

- Modify: `src/persona/persona.c`
- Modify: `tests/test_persona.c`

**Step 1: Write the failing test**

```c
static void test_persona_load_file_not_found(void) {
    sc_allocator_t alloc = sc_default_allocator();
    sc_persona_t p = {0};
    sc_error_t err = sc_persona_load(&alloc, "nonexistent-persona-xyz", 23, &p);
    SC_ASSERT(err != SC_OK);
    SC_PASS();
}
```

**Step 2: Run test to verify it fails**

Expected: FAIL — `sc_persona_load` undefined.

**Step 3: Implement `sc_persona_load`**

In `src/persona/persona.c`:

1. Build path: `$HOME/.seaclaw/personas/<name>.json`.
2. Read file contents via `fopen`/`fread`.
3. Call `sc_persona_load_json` on the contents.
4. Free the file buffer.
5. Return error if file not found or parse fails.

**Step 4: Run test to verify it passes**

Run: `cd build && cmake --build . -j$(nproc) && ./seaclaw_tests`
Expected: PASS, 0 ASan errors.

**Step 5: Commit**

```bash
git add src/persona/persona.c tests/test_persona.c
git commit -m "feat(persona): implement file-based persona loading"
```

---

## Task 5: Example Bank Loading

Load example banks from `~/.seaclaw/personas/examples/<persona>/<channel>/examples.json`.

**Files:**

- Create: `src/persona/examples.c`
- Modify: `tests/test_persona.c`

**Step 1: Write the failing test**

```c
static void test_persona_load_examples_from_json(void) {
    sc_allocator_t alloc = sc_default_allocator();
    const char *json =
        "{\"examples\":["
        "  {\"context\":\"casual\",\"incoming\":\"hey\",\"response\":\"yo\"},"
        "  {\"context\":\"work\",\"incoming\":\"meeting?\",\"response\":\"sure 3pm\"}"
        "]}";
    sc_persona_example_bank_t bank = {0};
    sc_error_t err = sc_persona_examples_load_json(&alloc, "imessage", 8, json, strlen(json), &bank);
    SC_ASSERT(err == SC_OK);
    SC_ASSERT(bank.examples_count == 2);
    SC_ASSERT(strcmp(bank.examples[0].context, "casual") == 0);
    SC_ASSERT(strcmp(bank.examples[1].response, "sure 3pm") == 0);
    /* cleanup handled by deinit_example_bank via sc_persona_deinit */
    if (bank.channel) alloc.free(alloc.ctx, bank.channel, strlen(bank.channel) + 1);
    for (size_t i = 0; i < bank.examples_count; i++) {
        sc_persona_example_t *e = &bank.examples[i];
        if (e->context) alloc.free(alloc.ctx, e->context, strlen(e->context) + 1);
        if (e->incoming) alloc.free(alloc.ctx, e->incoming, strlen(e->incoming) + 1);
        if (e->response) alloc.free(alloc.ctx, e->response, strlen(e->response) + 1);
    }
    alloc.free(alloc.ctx, bank.examples, bank.examples_count * sizeof(sc_persona_example_t));
    SC_PASS();
}
```

**Step 2: Run test to verify it fails**

Expected: FAIL — `sc_persona_examples_load_json` undefined.

**Step 3: Implement**

Create `src/persona/examples.c` with `sc_persona_examples_load_json`. Add declaration to `include/seaclaw/persona.h`.

Add `src/persona/examples.c` to `SC_CORE_SOURCES` in `CMakeLists.txt`.

**Step 4: Run test to verify it passes**

Expected: PASS, 0 ASan errors.

**Step 5: Commit**

```bash
git add src/persona/examples.c include/seaclaw/persona.h tests/test_persona.c CMakeLists.txt
git commit -m "feat(persona): implement example bank JSON loading"
```

---

## Task 6: Example Selection

Implement `sc_persona_select_examples` with keyword-biased sampling.

**Files:**

- Modify: `src/persona/examples.c`
- Modify: `tests/test_persona.c`

**Step 1: Write the failing test**

```c
static void test_persona_select_examples_by_channel(void) {
    sc_persona_example_t imsg_examples[] = {
        {.context = "casual greeting", .incoming = "hey", .response = "yo"},
        {.context = "making plans", .incoming = "dinner?", .response = "down"},
        {.context = "tech question", .incoming = "what lang?", .response = "C obviously"},
    };
    sc_persona_example_bank_t banks[] = {
        {.channel = "imessage", .examples = imsg_examples, .examples_count = 3},
    };
    sc_persona_t p = {.example_banks = banks, .example_banks_count = 1};

    const sc_persona_example_t *selected[2];
    size_t count = 0;
    sc_error_t err = sc_persona_select_examples(&p, "imessage", 8, "plans dinner", 12,
                                                 selected, &count, 2);
    SC_ASSERT(err == SC_OK);
    SC_ASSERT(count <= 2);
    SC_ASSERT(count > 0);
    SC_PASS();
}

static void test_persona_select_examples_no_channel(void) {
    sc_persona_t p = {0};
    const sc_persona_example_t *selected[2];
    size_t count = 99;
    sc_error_t err = sc_persona_select_examples(&p, "slack", 5, NULL, 0,
                                                 selected, &count, 2);
    SC_ASSERT(err == SC_OK);
    SC_ASSERT(count == 0);
    SC_PASS();
}
```

**Step 2: Run test to verify it fails**

Expected: FAIL — `sc_persona_select_examples` undefined.

**Step 3: Implement**

In `src/persona/examples.c`:

1. Find matching example bank by channel name.
2. If no match, set `*out_count = 0` and return `SC_OK`.
3. Score each example: count keyword overlaps between `topic` and `example.context`.
4. Sort by score descending, take top `max_examples`.
5. Set output pointers (not copies).

**Step 4: Run test to verify it passes**

Expected: PASS, 0 ASan errors.

**Step 5: Commit**

```bash
git add src/persona/examples.c tests/test_persona.c
git commit -m "feat(persona): implement keyword-biased example selection"
```

---

## Task 7: Persona Prompt Builder

Build the prompt string from persona + overlay + examples.

**Files:**

- Modify: `src/persona/persona.c`
- Modify: `tests/test_persona.c`

**Step 1: Write the failing test**

```c
static void test_persona_build_prompt_core_only(void) {
    sc_allocator_t alloc = sc_default_allocator();
    char *traits[] = {"direct", "curious"};
    char *rules[] = {"Keep it short"};
    char *values[] = {"honesty"};
    sc_persona_t p = {
        .name = "testuser", .name_len = 8,
        .identity = "A test persona",
        .traits = traits, .traits_count = 2,
        .communication_rules = rules, .communication_rules_count = 1,
        .values = values, .values_count = 1,
        .decision_style = "Decides fast",
    };

    char *out = NULL;
    size_t out_len = 0;
    sc_error_t err = sc_persona_build_prompt(&alloc, &p, NULL, 0, &out, &out_len);
    SC_ASSERT(err == SC_OK);
    SC_ASSERT(out != NULL);
    SC_ASSERT(out_len > 0);
    SC_ASSERT(strstr(out, "testuser") != NULL);
    SC_ASSERT(strstr(out, "direct") != NULL);
    SC_ASSERT(strstr(out, "Keep it short") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
    SC_PASS();
}

static void test_persona_build_prompt_with_overlay(void) {
    sc_allocator_t alloc = sc_default_allocator();
    char *notes[] = {"drops punctuation"};
    sc_persona_overlay_t overlays[] = {
        {.channel = "imessage", .formality = "casual", .avg_length = "short",
         .emoji_usage = "minimal", .style_notes = notes, .style_notes_count = 1},
    };
    sc_persona_t p = {
        .name = "testuser", .name_len = 8,
        .identity = "A test persona",
        .overlays = overlays, .overlays_count = 1,
    };

    char *out = NULL;
    size_t out_len = 0;
    sc_error_t err = sc_persona_build_prompt(&alloc, &p, "imessage", 8, &out, &out_len);
    SC_ASSERT(err == SC_OK);
    SC_ASSERT(strstr(out, "imessage") != NULL);
    SC_ASSERT(strstr(out, "casual") != NULL);
    SC_ASSERT(strstr(out, "drops punctuation") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
    SC_PASS();
}
```

**Step 2: Run test to verify it fails**

Expected: FAIL — `sc_persona_build_prompt` undefined.

**Step 3: Implement**

In `src/persona/persona.c`, implement `sc_persona_build_prompt`:

1. Start with identity header: `"You are acting as <name>. <identity>\n\n"`
2. Append traits: `"Personality traits: direct, curious\n"`
3. Append vocabulary if present: preferred, avoided, slang.
4. Append communication rules.
5. Append values and decision style.
6. Append: `"Match this style naturally. Don't exaggerate traits — aim for authenticity, not caricature.\n\n"`
7. If `channel` is provided, find overlay and append channel-specific section.
8. Use the same `append` buffer pattern as `src/agent/prompt.c:27-120`.

**Step 4: Run test to verify it passes**

Expected: PASS, 0 ASan errors.

**Step 5: Commit**

```bash
git add src/persona/persona.c tests/test_persona.c
git commit -m "feat(persona): implement persona prompt builder with overlay support"
```

---

## Task 8: Config Integration

Add `persona` field to agent config and parse it.

**Files:**

- Modify: `include/seaclaw/config.h` (add `persona` field to `sc_agent_config_t`, around line 73-90)
- Modify: `src/config.c` (add parsing in `parse_agent`, around line 877-924)
- Modify: `tests/test_persona.c`

**Step 1: Write the failing test**

```c
static void test_config_persona_field(void) {
    sc_allocator_t alloc = sc_default_allocator();
    const char *json = "{\"agent\":{\"persona\":\"seth\"}}";
    sc_config_t cfg = {0};
    sc_error_t err = sc_config_parse(&alloc, json, strlen(json), &cfg);
    SC_ASSERT(err == SC_OK);
    SC_ASSERT(cfg.agent.persona != NULL);
    SC_ASSERT(strcmp(cfg.agent.persona, "seth") == 0);
    sc_config_deinit(&alloc, &cfg);
    SC_PASS();
}
```

**Step 2: Run test to verify it fails**

Expected: FAIL — `cfg.agent.persona` does not exist.

**Step 3: Implement**

In `include/seaclaw/config.h`, add to `sc_agent_config_t`:

```c
char *persona; /* persona name to load from ~/.seaclaw/personas/<name>.json */
```

In `src/config.c`, in `parse_agent` (around line 920, before `return SC_OK`):

```c
const char *persona = sc_json_get_string(obj, "persona");
if (persona) {
    if (cfg->agent.persona)
        a->free(a->ctx, cfg->agent.persona, strlen(cfg->agent.persona) + 1);
    cfg->agent.persona = sc_strdup(a, persona);
}
```

In `sc_config_deinit`, free `cfg->agent.persona` if set.

**Step 4: Run test to verify it passes**

Expected: PASS, 0 ASan errors.

**Step 5: Commit**

```bash
git add include/seaclaw/config.h src/config.c tests/test_persona.c
git commit -m "feat(persona): add persona field to agent config schema"
```

---

## Task 9: Agent Integration

Wire persona loading into the agent lifecycle.

**Files:**

- Modify: `include/seaclaw/agent.h` (already has `persona_prompt` at line 129-130; add `sc_persona_t *persona`)
- Modify: `src/agent/agent.c` (load persona in `sc_agent_from_config`, wire into turn)
- Modify: `tests/test_persona.c`

**Step 1: Write the failing test**

```c
static void test_agent_persona_prompt_injected(void) {
    /* This test verifies that when persona_prompt is set on the agent,
       sc_prompt_build_system uses it instead of the default identity.
       The infrastructure already exists — we just need to verify the
       full path from agent.persona → persona_prompt → system prompt. */
    sc_allocator_t alloc = sc_default_allocator();
    sc_prompt_config_t cfg = {
        .persona_prompt = "You are acting as TestUser. Personality: direct.",
        .persona_prompt_len = 48,
    };
    char *out = NULL;
    size_t out_len = 0;
    sc_error_t err = sc_prompt_build_system(&alloc, &cfg, &out, &out_len);
    SC_ASSERT(err == SC_OK);
    SC_ASSERT(strstr(out, "TestUser") != NULL);
    SC_ASSERT(strstr(out, "SeaClaw") == NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
    SC_PASS();
}
```

**Step 2: Run test to verify it passes** (this one should already pass since `sc_prompt_build_system` already handles `persona_prompt`)

Expected: PASS — this validates the existing path works.

**Step 3: Wire persona loading into agent**

In `include/seaclaw/agent.h`, add field near line 129:

```c
sc_persona_t *persona;          /* loaded persona; owned; NULL if none */
char *persona_name;             /* persona name from config; owned */
size_t persona_name_len;
```

In `src/agent/agent.c`, in `sc_agent_from_config` (after line 274):

```c
/* Load persona if configured */
if (config->persona && config->persona[0]) {
    out->persona_name = sc_strdup(alloc, config->persona);
    out->persona_name_len = strlen(config->persona);
    out->persona = alloc->alloc(alloc->ctx, sizeof(sc_persona_t));
    if (out->persona) {
        memset(out->persona, 0, sizeof(sc_persona_t));
        sc_error_t perr = sc_persona_load(alloc, config->persona,
                                           strlen(config->persona), out->persona);
        if (perr != SC_OK) {
            alloc->free(alloc->ctx, out->persona, sizeof(sc_persona_t));
            out->persona = NULL;
            /* Log warning but don't fail agent creation */
        }
    }
}
```

In `sc_agent_turn` (around line 766), before building the prompt config, if `agent->persona` is loaded and `agent->persona_prompt` is NULL, build the persona prompt:

```c
if (agent->persona && !agent->persona_prompt) {
    char *pp = NULL;
    size_t pp_len = 0;
    const char *ch = NULL; /* TODO: get current channel name */
    size_t ch_len = 0;
    sc_error_t perr = sc_persona_build_prompt(agent->alloc, agent->persona,
                                               ch, ch_len, &pp, &pp_len);
    if (perr == SC_OK) {
        agent->persona_prompt = pp;
        agent->persona_prompt_len = pp_len;
    }
}
```

In `sc_agent_deinit`, free persona:

```c
if (agent->persona) {
    sc_persona_deinit(agent->alloc, agent->persona);
    agent->alloc->free(agent->alloc->ctx, agent->persona, sizeof(sc_persona_t));
}
if (agent->persona_name)
    agent->alloc->free(agent->alloc->ctx, agent->persona_name, agent->persona_name_len + 1);
```

**Step 4: Run test to verify it passes**

Run: `cd build && cmake --build . -j$(nproc) && ./seaclaw_tests`
Expected: PASS, 0 ASan errors.

**Step 5: Commit**

```bash
git add include/seaclaw/agent.h src/agent/agent.c tests/test_persona.c
git commit -m "feat(persona): wire persona loading into agent lifecycle"
```

---

## Task 10: Spawn Persona Inheritance

Pass persona name through spawn config so child agents inherit it.

**Files:**

- Modify: `include/seaclaw/agent/spawn.h` (add `persona_name` to `sc_spawn_config_t`, around line 20-41)
- Modify: `src/agent/spawn.c` (copy persona name into slot, around line 330-356)
- Modify: `tests/test_persona.c`

**Step 1: Write the failing test**

```c
static void test_spawn_config_has_persona(void) {
    sc_spawn_config_t cfg = {
        .persona_name = "seth",
        .persona_name_len = 4,
    };
    SC_ASSERT(cfg.persona_name != NULL);
    SC_ASSERT(cfg.persona_name_len == 4);
    SC_PASS();
}
```

**Step 2: Run test to verify it fails**

Expected: FAIL — `persona_name` field doesn't exist on `sc_spawn_config_t`.

**Step 3: Implement**

In `include/seaclaw/agent/spawn.h`, add to `sc_spawn_config_t`:

```c
const char *persona_name;
size_t persona_name_len;
```

In `src/agent/spawn.c`:

1. Add `char *persona_name;` to `sc_pool_slot_t` (around line 19-44).
2. In `sc_agent_pool_spawn` (around line 330), add: `s->persona_name = dup_opt(a, cfg->persona_name, cfg->persona_name_len);`
3. In `spawn_thread` (around line 138), set persona on the spawned agent config.
4. In slot cleanup, free `persona_name`.

**Step 4: Run test to verify it passes**

Expected: PASS, 0 ASan errors.

**Step 5: Commit**

```bash
git add include/seaclaw/agent/spawn.h src/agent/spawn.c tests/test_persona.c
git commit -m "feat(persona): add persona inheritance to spawn config"
```

---

## Task 11: Message Sampler

Sample messages from data sources for the creation pipeline.

**Files:**

- Create: `src/persona/sampler.c`
- Modify: `include/seaclaw/persona.h`
- Modify: `tests/test_persona.c`

**Step 1: Write the failing test**

```c
static void test_sampler_imessage_query(void) {
    /* Verify the iMessage query builder produces valid SQL */
    char query[512];
    size_t query_len = 0;
    sc_error_t err = sc_persona_sampler_imessage_query(query, sizeof(query), &query_len, 500);
    SC_ASSERT(err == SC_OK);
    SC_ASSERT(query_len > 0);
    SC_ASSERT(strstr(query, "message") != NULL);
    SC_ASSERT(strstr(query, "LIMIT") != NULL);
    SC_PASS();
}
```

**Step 2: Run test to verify it fails**

Expected: FAIL — `sc_persona_sampler_imessage_query` undefined.

**Step 3: Implement**

Create `src/persona/sampler.c`:

- `sc_persona_sampler_imessage_query`: builds a SQL query to sample messages from `~/Library/Messages/chat.db`. Selects the user's outgoing messages with varied contacts and recency bias. Columns: `text`, `handle_id`, `date`, `is_from_me`. Limits to N samples.
- `sc_persona_sampler_facebook_parse`: parses Facebook's `message_*.json` export format. Extracts the user's messages.
- These are pure functions — they build queries or parse data, not open databases. The CLI command handles I/O.

Add `src/persona/sampler.c` to `SC_CORE_SOURCES` in `CMakeLists.txt`.

**Step 4: Run test to verify it passes**

Expected: PASS, 0 ASan errors.

**Step 5: Commit**

```bash
git add src/persona/sampler.c include/seaclaw/persona.h tests/test_persona.c CMakeLists.txt
git commit -m "feat(persona): implement message sampler for iMessage and Facebook"
```

---

## Task 12: Provider Analyzer

Send message batches to the AI provider for personality extraction.

**Files:**

- Create: `src/persona/analyzer.c`
- Modify: `include/seaclaw/persona.h`
- Modify: `tests/test_persona.c`

**Step 1: Write the failing test**

```c
static void test_analyzer_builds_extraction_prompt(void) {
    const char *messages[] = {"hey whats up", "down. where at", "thursday works"};
    char prompt[2048];
    size_t prompt_len = 0;
    sc_error_t err = sc_persona_analyzer_build_prompt(messages, 3, "imessage",
                                                       prompt, sizeof(prompt), &prompt_len);
    SC_ASSERT(err == SC_OK);
    SC_ASSERT(prompt_len > 0);
    SC_ASSERT(strstr(prompt, "personality") != NULL || strstr(prompt, "traits") != NULL);
    SC_ASSERT(strstr(prompt, "hey whats up") != NULL);
    SC_PASS();
}

static void test_analyzer_parses_extraction_response(void) {
    sc_allocator_t alloc = sc_default_allocator();
    /* Simulated provider response */
    const char *response =
        "{\"traits\":[\"direct\",\"casual\"],"
        "\"vocabulary\":{\"preferred\":[\"down\",\"works\"],\"avoided\":[],\"slang\":[]},"
        "\"communication_rules\":[\"Keeps messages very short\"],"
        "\"formality\":\"casual\",\"avg_length\":\"short\","
        "\"emoji_usage\":\"none\"}";

    sc_persona_t partial = {0};
    sc_error_t err = sc_persona_analyzer_parse_response(&alloc, response, strlen(response),
                                                         "imessage", 8, &partial);
    SC_ASSERT(err == SC_OK);
    SC_ASSERT(partial.traits_count >= 1);
    SC_ASSERT(partial.overlays_count == 1);
    sc_persona_deinit(&alloc, &partial);
    SC_PASS();
}
```

**Step 2: Run test to verify they fail**

Expected: FAIL — functions undefined.

**Step 3: Implement**

Create `src/persona/analyzer.c`:

- `sc_persona_analyzer_build_prompt`: builds the extraction prompt containing message samples and structured extraction instructions. Asks the provider to return JSON with traits, vocabulary, communication_rules, and channel-specific style.
- `sc_persona_analyzer_parse_response`: parses the provider's JSON response into a partial `sc_persona_t` (traits + one overlay for the source channel).
- Uses `SC_IS_TEST` guard: in test mode, does not make real provider calls.

Add `src/persona/analyzer.c` to `SC_CORE_SOURCES` in `CMakeLists.txt`.

**Step 4: Run test to verify it passes**

Expected: PASS, 0 ASan errors.

**Step 5: Commit**

```bash
git add src/persona/analyzer.c include/seaclaw/persona.h tests/test_persona.c CMakeLists.txt
git commit -m "feat(persona): implement provider-based personality analyzer"
```

---

## Task 13: Creator Pipeline

Orchestrate the full creation flow: ingest → sample → analyze → synthesize → write.

**Files:**

- Create: `src/persona/creator.c`
- Modify: `include/seaclaw/persona.h`
- Modify: `tests/test_persona.c`

**Step 1: Write the failing test**

```c
static void test_creator_synthesize_merges_partials(void) {
    sc_allocator_t alloc = sc_default_allocator();
    char *traits1[] = {"direct", "casual"};
    char *traits2[] = {"curious", "direct"};
    sc_persona_t partials[] = {
        {.traits = traits1, .traits_count = 2},
        {.traits = traits2, .traits_count = 2},
    };
    sc_persona_t merged = {0};
    sc_error_t err = sc_persona_creator_synthesize(&alloc, partials, 2, "testuser", 8, &merged);
    SC_ASSERT(err == SC_OK);
    SC_ASSERT(strcmp(merged.name, "testuser") == 0);
    /* "direct" should be deduplicated */
    SC_ASSERT(merged.traits_count == 3); /* direct, casual, curious */
    sc_persona_deinit(&alloc, &merged);
    SC_PASS();
}
```

**Step 2: Run test to verify it fails**

Expected: FAIL — `sc_persona_creator_synthesize` undefined.

**Step 3: Implement**

Create `src/persona/creator.c`:

- `sc_persona_creator_synthesize`: merges partial persona extractions (from different sources/batches). Deduplicates traits, merges vocabulary, combines overlays, picks majority values for decision_style etc.
- `sc_persona_creator_write`: writes `sc_persona_t` to `~/.seaclaw/personas/<name>.json` and example bank files. Creates directories if needed.
- `sc_persona_creator_run`: full pipeline orchestrator (called by CLI command and tool). Takes source configs, provider, and name. Returns the created persona path.

Add `src/persona/creator.c` to `SC_CORE_SOURCES` in `CMakeLists.txt`.

**Step 4: Run test to verify it passes**

Expected: PASS, 0 ASan errors.

**Step 5: Commit**

```bash
git add src/persona/creator.c include/seaclaw/persona.h tests/test_persona.c CMakeLists.txt
git commit -m "feat(persona): implement creation pipeline with merge and write"
```

---

## Task 14: CLI Command

Add `seaclaw persona` command.

**Files:**

- Create: `src/persona/cli.c`
- Modify: `src/main.c` (add command entry around line 137-164)
- Modify: `CMakeLists.txt`
- Modify: `tests/test_persona.c`

**Step 1: Write the failing test**

```c
static void test_persona_cli_parse_args(void) {
    const char *argv[] = {"seaclaw", "persona", "create", "seth", "--from-imessage"};
    sc_persona_cli_args_t args = {0};
    sc_error_t err = sc_persona_cli_parse(5, argv, &args);
    SC_ASSERT(err == SC_OK);
    SC_ASSERT(args.action == SC_PERSONA_ACTION_CREATE);
    SC_ASSERT(strcmp(args.name, "seth") == 0);
    SC_ASSERT(args.from_imessage == true);
    SC_ASSERT(args.from_gmail == false);
    SC_PASS();
}
```

**Step 2: Run test to verify it fails**

Expected: FAIL — types and function undefined.

**Step 3: Implement**

Create `src/persona/cli.c`:

- Define `sc_persona_cli_args_t` with `action` (create/update/show/list/delete), `name`, source flags (`from_imessage`, `from_gmail`, `from_facebook`), and `interactive` flag.
- `sc_persona_cli_parse`: parse argc/argv into args struct.
- `cmd_persona`: the command handler registered in `src/main.c`. Dispatches to creator pipeline, show, list, or delete based on action.

In `src/main.c`, add forward declaration and command entry:

```c
static int cmd_persona(sc_allocator_t *alloc, int argc, char **argv);

/* In commands[] array: */
{"persona", "Create and manage persona profiles", cmd_persona},
```

The `cmd_persona` implementation in `main.c` delegates to `sc_persona_cli_run` from `src/persona/cli.c`.

Add `src/persona/cli.c` to `SC_CORE_SOURCES` in `CMakeLists.txt`.

**Step 4: Run test to verify it passes**

Expected: PASS, 0 ASan errors.

**Step 5: Commit**

```bash
git add src/persona/cli.c src/main.c include/seaclaw/persona.h tests/test_persona.c CMakeLists.txt
git commit -m "feat(persona): add CLI command for persona management"
```

---

## Task 15: Persona Tool

Register as `sc_tool_t` for in-conversation persona management.

**Files:**

- Create: `src/tools/persona.c`
- Modify: `src/tools/factory.c` (register around line 363)
- Modify: `CMakeLists.txt`
- Modify: `tests/test_persona.c`

**Step 1: Write the failing test**

```c
static void test_persona_tool_create(void) {
    sc_allocator_t alloc = sc_default_allocator();
    sc_tool_t tool = {0};
    sc_error_t err = sc_persona_tool_create(&alloc, &tool);
    SC_ASSERT(err == SC_OK);
    SC_ASSERT(tool.vtable != NULL);
    SC_ASSERT(strcmp(tool.vtable->name(tool.ctx), "persona") == 0);
    SC_ASSERT(tool.vtable->description(tool.ctx) != NULL);
    SC_ASSERT(tool.vtable->parameters_json(tool.ctx) != NULL);
    if (tool.vtable->deinit) tool.vtable->deinit(tool.ctx);
    SC_PASS();
}
```

**Step 2: Run test to verify it fails**

Expected: FAIL — `sc_persona_tool_create` undefined.

**Step 3: Implement**

Create `src/tools/persona.c` implementing the `sc_tool_t` vtable:

- `name`: returns `"persona"`
- `description`: returns `"Create, update, and manage persona profiles for personality cloning"`
- `parameters_json`: returns JSON schema with `action` (create/update/update_overlay/show/list/delete), `name`, `source`, `channel`, `patch` parameters.
- `execute`: dispatches based on action. Uses `SC_IS_TEST` guard for operations that touch the filesystem or provider.

Follow the pattern from existing tools (e.g., `src/tools/agent_spawn.c`).

In `src/tools/factory.c`, add after `sc_agent_spawn_tool_create` (around line 363):

```c
err = sc_persona_tool_create(alloc, &tools[idx]);
if (err != SC_OK)
    goto fail;
idx++;
```

Update `SC_TOOLS_COUNT_BASE` (line 86) to include the new tool.

Add `src/tools/persona.c` to `SC_CORE_SOURCES` in `CMakeLists.txt`.

**Step 4: Run test to verify it passes**

Expected: PASS, 0 ASan errors.

**Step 5: Commit**

```bash
git add src/tools/persona.c src/tools/factory.c include/seaclaw/persona.h tests/test_persona.c CMakeLists.txt
git commit -m "feat(persona): add persona tool for in-conversation management"
```

---

## Task 16: Integration Test — Full Round Trip

End-to-end test: create persona from mock data, load it, build prompt, verify output.

**Files:**

- Modify: `tests/test_persona.c`

**Step 1: Write the integration test**

```c
static void test_persona_full_round_trip(void) {
    sc_allocator_t alloc = sc_default_allocator();

    /* 1. Parse a persona from JSON */
    const char *json =
        "{"
        "  \"version\": 1, \"name\": \"roundtrip\","
        "  \"core\": {"
        "    \"identity\": \"Integration test persona\","
        "    \"traits\": [\"direct\"],"
        "    \"vocabulary\": {\"preferred\": [\"solid\"], \"avoided\": [], \"slang\": []},"
        "    \"communication_rules\": [\"Be brief\"],"
        "    \"values\": [\"speed\"],"
        "    \"decision_style\": \"Fast\""
        "  },"
        "  \"channel_overlays\": {"
        "    \"imessage\": {"
        "      \"formality\": \"casual\", \"avg_length\": \"short\","
        "      \"emoji_usage\": \"none\", \"style_notes\": [\"no caps\"]"
        "    }"
        "  }"
        "}";

    sc_persona_t p = {0};
    sc_error_t err = sc_persona_load_json(&alloc, json, strlen(json), &p);
    SC_ASSERT(err == SC_OK);

    /* 2. Build prompt with channel overlay */
    char *prompt = NULL;
    size_t prompt_len = 0;
    err = sc_persona_build_prompt(&alloc, &p, "imessage", 8, &prompt, &prompt_len);
    SC_ASSERT(err == SC_OK);
    SC_ASSERT(strstr(prompt, "roundtrip") != NULL);
    SC_ASSERT(strstr(prompt, "casual") != NULL);
    SC_ASSERT(strstr(prompt, "no caps") != NULL);

    /* 3. Inject into prompt config and build system prompt */
    sc_prompt_config_t cfg = {
        .persona_prompt = prompt,
        .persona_prompt_len = prompt_len,
    };
    char *sys = NULL;
    size_t sys_len = 0;
    err = sc_prompt_build_system(&alloc, &cfg, &sys, &sys_len);
    SC_ASSERT(err == SC_OK);
    SC_ASSERT(strstr(sys, "roundtrip") != NULL);
    SC_ASSERT(strstr(sys, "SeaClaw") == NULL);

    alloc.free(alloc.ctx, sys, sys_len + 1);
    alloc.free(alloc.ctx, prompt, prompt_len + 1);
    sc_persona_deinit(&alloc, &p);
    SC_PASS();
}
```

**Step 2: Run test to verify it passes**

Run: `cd build && cmake --build . -j$(nproc) && ./seaclaw_tests`
Expected: PASS, 0 ASan errors. All 2,493+ existing tests still pass.

**Step 3: Commit**

```bash
git add tests/test_persona.c
git commit -m "test(persona): add full round-trip integration test"
```

---

## Task 17: Final Validation

**Step 1: Full test suite**

```bash
cd build && cmake .. -DSC_ENABLE_ALL_CHANNELS=ON && cmake --build . -j$(nproc) && ./seaclaw_tests
```

Expected: All tests pass, 0 ASan errors.

**Step 2: Release build**

```bash
cd build && cmake .. -DCMAKE_BUILD_TYPE=MinSizeRel -DSC_ENABLE_LTO=ON && cmake --build . -j$(nproc)
```

Expected: Clean compile. Binary size increase < 15 KB.

**Step 3: Verify CLI**

```bash
./build/seaclaw persona --help
```

Expected: Shows persona subcommands (create, update, show, list, delete).

**Step 4: Commit any remaining fixes**

```bash
git add -A
git commit -m "chore(persona): final validation and cleanup"
```
