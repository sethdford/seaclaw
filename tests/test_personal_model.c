#include "human/core/error.h"
#include "human/memory/fact_extract.h"
#include "human/memory/personal_model.h"
#include "test_framework.h"
#include <string.h>

static void personal_model_init_sets_defaults(void) {
    hu_personal_model_t m;
    hu_personal_model_init(&m);
    HU_ASSERT_EQ((long)m.version, 1L);
    HU_ASSERT_EQ((long)m.created_at, 0L);
    HU_ASSERT_EQ((long)m.fact_count, 0L);
    HU_ASSERT_EQ((long)m.topic_count, 0L);
    HU_ASSERT_EQ((long)m.goal_count, 0L);
    HU_ASSERT_EQ((unsigned)m.style.sample_count, 0U);
}

static void personal_model_ingest_extracts_facts(void) {
    hu_personal_model_t m;
    hu_personal_model_init(&m);
    const char *text = "I like hiking, I live in Portland";
    HU_ASSERT_EQ(hu_personal_model_ingest(&m, text, strlen(text), true, 1700000000LL), HU_OK);
    HU_ASSERT_TRUE(m.fact_count >= 2U);
}

static void personal_model_merge_facts_deduplicates(void) {
    hu_personal_model_t m;
    hu_personal_model_init(&m);
    m.updated_at = 1;

    hu_fact_extract_result_t batch;
    memset(&batch, 0, sizeof(batch));
    strncpy(batch.facts[0].subject, "user", sizeof(batch.facts[0].subject) - 1);
    strncpy(batch.facts[0].predicate, "i like", sizeof(batch.facts[0].predicate) - 1);
    strncpy(batch.facts[0].object, "tea", sizeof(batch.facts[0].object) - 1);
    batch.facts[0].type = HU_KNOWLEDGE_PROPOSITIONAL;
    batch.facts[0].confidence = 0.7f;
    batch.fact_count = 1;

    HU_ASSERT_EQ(hu_personal_model_merge_facts(&m, &batch), HU_OK);
    HU_ASSERT_EQ((long)m.fact_count, 1L);
    HU_ASSERT_EQ(hu_personal_model_merge_facts(&m, &batch), HU_OK);
    HU_ASSERT_EQ((long)m.fact_count, 1L);
}

static void personal_model_build_prompt_non_empty(void) {
    hu_personal_model_t m;
    hu_personal_model_init(&m);
    char buf[2048];
    size_t n = hu_personal_model_build_prompt(&m, buf, sizeof(buf));
    HU_ASSERT_GT((long)n, 0L);
    HU_ASSERT_TRUE(strstr(buf, "[Personal Context]") != NULL);
}

static void personal_model_query_preference_finds_match(void) {
    hu_personal_model_t m;
    hu_personal_model_init(&m);
    const char *text = "I prefer dark mode for coding";
    HU_ASSERT_EQ(hu_personal_model_ingest(&m, text, strlen(text), true, 0), HU_OK);
    const hu_heuristic_fact_t *f = hu_personal_model_query_preference(&m, "dark", 4);
    HU_ASSERT_NOT_NULL(f);
}

static void personal_model_ingest_updates_style_metrics(void) {
    hu_personal_model_t m;
    hu_personal_model_init(&m);
    const char *text = "Hello there";
    size_t len = strlen(text);
    HU_ASSERT_EQ(hu_personal_model_ingest(&m, text, len, true, 0), HU_OK);
    HU_ASSERT_EQ((unsigned)m.style.sample_count, 1U);
    HU_ASSERT_EQ((unsigned)m.style.avg_message_length, (unsigned)len);
}

void run_personal_model_tests(void) {
    HU_TEST_SUITE("PersonalModel");
    HU_RUN_TEST(personal_model_init_sets_defaults);
    HU_RUN_TEST(personal_model_ingest_extracts_facts);
    HU_RUN_TEST(personal_model_merge_facts_deduplicates);
    HU_RUN_TEST(personal_model_build_prompt_non_empty);
    HU_RUN_TEST(personal_model_query_preference_finds_match);
    HU_RUN_TEST(personal_model_ingest_updates_style_metrics);
}
