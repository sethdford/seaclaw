#include "test_framework.h"
#include "human/memory/fact_extract.h"
#include "human/core/allocator.h"
#include <string.h>

/* ── Extraction ──────────────────────────────────────────────────── */

static void extract_propositional_fact(void) {
    hu_fact_extract_result_t result = {0};
    const char *text = "I like hiking in the mountains.";
    HU_ASSERT(hu_fact_extract(text, strlen(text), &result) == HU_OK);
    HU_ASSERT(result.fact_count >= 1);
    HU_ASSERT(result.propositional_count >= 1);
    HU_ASSERT(result.facts[0].type == HU_KNOWLEDGE_PROPOSITIONAL);
    HU_ASSERT(strcmp(result.facts[0].subject, "user") == 0);
}

static void extract_prescriptive_fact(void) {
    hu_fact_extract_result_t result = {0};
    const char *text = "I usually go for a walk when stressed.";
    HU_ASSERT(hu_fact_extract(text, strlen(text), &result) == HU_OK);
    HU_ASSERT(result.fact_count >= 1);
    HU_ASSERT(result.prescriptive_count >= 1);
    HU_ASSERT(result.facts[0].type == HU_KNOWLEDGE_PRESCRIPTIVE);
}

static void extract_multiple_facts(void) {
    hu_fact_extract_result_t result = {0};
    const char *text = "I like coffee. I work at a startup. I live in Austin.";
    HU_ASSERT(hu_fact_extract(text, strlen(text), &result) == HU_OK);
    HU_ASSERT(result.fact_count >= 3);
}

static void extract_no_facts_from_plain(void) {
    hu_fact_extract_result_t result = {0};
    const char *text = "The weather is nice today.";
    HU_ASSERT(hu_fact_extract(text, strlen(text), &result) == HU_OK);
    HU_ASSERT(result.fact_count == 0);
}

static void extract_case_insensitive(void) {
    hu_fact_extract_result_t result = {0};
    const char *text = "I Love cooking Italian food.";
    HU_ASSERT(hu_fact_extract(text, strlen(text), &result) == HU_OK);
    HU_ASSERT(result.fact_count >= 1);
}

static void extract_null_fails(void) {
    hu_fact_extract_result_t result = {0};
    HU_ASSERT(hu_fact_extract(NULL, 0, &result) == HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT(hu_fact_extract("hi", 2, NULL) == HU_ERR_INVALID_ARGUMENT);
}

/* ── Dedup ───────────────────────────────────────────────────────── */

static void dedup_removes_duplicates(void) {
    hu_fact_extract_result_t result = {0};
    const char *text = "I like hiking.";
    HU_ASSERT(hu_fact_extract(text, strlen(text), &result) == HU_OK);

    hu_heuristic_fact_t existing[1];
    memset(existing, 0, sizeof(existing));
    strncpy(existing[0].subject, "user", sizeof(existing[0].subject) - 1);
    strncpy(existing[0].predicate, "i like", sizeof(existing[0].predicate) - 1);

    size_t novel = hu_fact_dedup(&result, existing, 1);
    HU_ASSERT(novel == 0);
}

static void dedup_keeps_novel(void) {
    hu_fact_extract_result_t result = {0};
    const char *text = "I like hiking.";
    HU_ASSERT(hu_fact_extract(text, strlen(text), &result) == HU_OK);

    hu_heuristic_fact_t existing[1];
    memset(existing, 0, sizeof(existing));
    strncpy(existing[0].subject, "user", sizeof(existing[0].subject) - 1);
    strncpy(existing[0].predicate, "i hate", sizeof(existing[0].predicate) - 1);

    size_t novel = hu_fact_dedup(&result, existing, 1);
    HU_ASSERT(novel >= 1);
}

/* ── Format for store ────────────────────────────────────────────── */

static void format_propositional_fact(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_heuristic_fact_t fact = {0};
    fact.type = HU_KNOWLEDGE_PROPOSITIONAL;
    strncpy(fact.subject, "user", sizeof(fact.subject) - 1);
    strncpy(fact.predicate, "likes", sizeof(fact.predicate) - 1);
    strncpy(fact.object, "hiking", sizeof(fact.object) - 1);
    fact.confidence = 0.8f;

    char *key = NULL, *value = NULL;
    size_t key_len = 0, value_len = 0;
    HU_ASSERT(hu_fact_format_for_store(&alloc, &fact, &key, &key_len, &value, &value_len) == HU_OK);
    HU_ASSERT(key != NULL);
    HU_ASSERT(strstr(key, "fact:") != NULL);
    HU_ASSERT(value != NULL);
    alloc.free(alloc.ctx, key, key_len + 1);
    alloc.free(alloc.ctx, value, value_len + 1);
}

static void format_prescriptive_fact(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_heuristic_fact_t fact = {0};
    fact.type = HU_KNOWLEDGE_PRESCRIPTIVE;
    strncpy(fact.subject, "user", sizeof(fact.subject) - 1);
    strncpy(fact.predicate, "usually walks", sizeof(fact.predicate) - 1);
    strncpy(fact.object, "when stressed", sizeof(fact.object) - 1);
    fact.confidence = 0.7f;

    char *key = NULL, *value = NULL;
    size_t key_len = 0, value_len = 0;
    HU_ASSERT(hu_fact_format_for_store(&alloc, &fact, &key, &key_len, &value, &value_len) == HU_OK);
    HU_ASSERT(strstr(key, "skill:") != NULL);
    alloc.free(alloc.ctx, key, key_len + 1);
    alloc.free(alloc.ctx, value, value_len + 1);
}

static void format_null_fails(void) {
    char *k = NULL, *v = NULL;
    size_t kl = 0, vl = 0;
    HU_ASSERT(hu_fact_format_for_store(NULL, NULL, &k, &kl, &v, &vl) == HU_ERR_INVALID_ARGUMENT);
}

/* ── Runner ──────────────────────────────────────────────────────── */

void run_fact_extract_tests(void) {
    HU_TEST_SUITE("Fact Extraction");

    HU_RUN_TEST(extract_propositional_fact);
    HU_RUN_TEST(extract_prescriptive_fact);
    HU_RUN_TEST(extract_multiple_facts);
    HU_RUN_TEST(extract_no_facts_from_plain);
    HU_RUN_TEST(extract_case_insensitive);
    HU_RUN_TEST(extract_null_fails);

    HU_RUN_TEST(dedup_removes_duplicates);
    HU_RUN_TEST(dedup_keeps_novel);

    HU_RUN_TEST(format_propositional_fact);
    HU_RUN_TEST(format_prescriptive_fact);
    HU_RUN_TEST(format_null_fails);
}
