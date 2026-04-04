#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/memory/fact_extract.h"
#include "test_framework.h"
#include <string.h>

static void fact_extract_personal_statement_finds_facts(void) {
    const char *text = "My name is Alice and I live in NYC.";
    hu_fact_extract_result_t result;
    HU_ASSERT_EQ(hu_fact_extract(text, strlen(text), &result), HU_OK);
    HU_ASSERT_GT((long)result.fact_count, 0L);
}

static void fact_extract_empty_text_zero_facts(void) {
    hu_fact_extract_result_t result;
    HU_ASSERT_EQ(hu_fact_extract("", 0, &result), HU_OK);
    HU_ASSERT_EQ((long)result.fact_count, 0L);
}

static void fact_extract_null_returns_error(void) {
    hu_fact_extract_result_t result;
    HU_ASSERT_EQ(hu_fact_extract(NULL, 0, &result), HU_ERR_INVALID_ARGUMENT);
}

static void fact_dedup_removes_duplicates(void) {
    const char *text = "My name is Alice and I live in NYC.";
    hu_fact_extract_result_t first;
    hu_fact_extract_result_t second;
    HU_ASSERT_EQ(hu_fact_extract(text, strlen(text), &first), HU_OK);
    HU_ASSERT_EQ(hu_fact_extract(text, strlen(text), &second), HU_OK);
    HU_ASSERT_GT((long)first.fact_count, 0L);

    size_t novel = hu_fact_dedup(&second, first.facts, first.fact_count);
    HU_ASSERT_EQ((long)novel, 0L);
}

static void fact_format_for_store_produces_key_value(void) {
    hu_heuristic_fact_t fact;
    memset(&fact, 0, sizeof(fact));
    fact.type = HU_KNOWLEDGE_PROPOSITIONAL;
    strncpy(fact.subject, "user", sizeof(fact.subject) - 1);
    strncpy(fact.predicate, "prefers", sizeof(fact.predicate) - 1);
    strncpy(fact.object, "tea", sizeof(fact.object) - 1);
    fact.confidence = 0.75f;

    hu_allocator_t alloc = hu_system_allocator();
    char *key = NULL;
    size_t key_len = 0;
    char *value = NULL;
    size_t value_len = 0;
    HU_ASSERT_EQ(hu_fact_format_for_store(&alloc, &fact, &key, &key_len, &value, &value_len),
                 HU_OK);
    HU_ASSERT_NOT_NULL(key);
    HU_ASSERT_NOT_NULL(value);
    HU_ASSERT_GT((long)key_len, 0L);
    HU_ASSERT_GT((long)value_len, 0L);

    alloc.free(alloc.ctx, key, key_len + 1);
    alloc.free(alloc.ctx, value, value_len + 1);
}

static void fact_format_null_fact_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *key = NULL;
    size_t key_len = 0;
    char *value = NULL;
    size_t value_len = 0;
    HU_ASSERT_EQ(hu_fact_format_for_store(&alloc, NULL, &key, &key_len, &value, &value_len),
                 HU_ERR_INVALID_ARGUMENT);
}

void run_fact_extract_tests(void) {
    HU_TEST_SUITE("fact_extract");
    HU_RUN_TEST(fact_extract_personal_statement_finds_facts);
    HU_RUN_TEST(fact_extract_empty_text_zero_facts);
    HU_RUN_TEST(fact_extract_null_returns_error);
    HU_RUN_TEST(fact_dedup_removes_duplicates);
    HU_RUN_TEST(fact_format_for_store_produces_key_value);
    HU_RUN_TEST(fact_format_null_fact_returns_error);
}
