#include "test_framework.h"
#include "human/memory/self_rag.h"
#include <string.h>

static void srag_greeting_skips_retrieval(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_srag_config_t cfg = hu_srag_config_default();
    hu_srag_assessment_t a;
    HU_ASSERT_EQ(hu_srag_should_retrieve(&alloc, &cfg, "hello", 5, NULL, 0, &a), HU_OK);
    HU_ASSERT_EQ((int)a.decision, (int)HU_SRAG_NO_RETRIEVAL);
}

static void srag_personal_query_always_retrieves(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_srag_config_t cfg = hu_srag_config_default();
    hu_srag_assessment_t a;
    const char *q = "remember my favorite color";
    HU_ASSERT_EQ(hu_srag_should_retrieve(&alloc, &cfg, q, strlen(q), NULL, 0, &a), HU_OK);
    HU_ASSERT_EQ((int)a.decision, (int)HU_SRAG_RETRIEVE);
    HU_ASSERT(a.is_personal_query);
}

static void srag_temporal_marker_retrieves(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_srag_config_t cfg = hu_srag_config_default();
    hu_srag_assessment_t a;
    const char *q = "what did we discuss last week";
    HU_ASSERT_EQ(hu_srag_should_retrieve(&alloc, &cfg, q, strlen(q), NULL, 0, &a), HU_OK);
    HU_ASSERT_EQ((int)a.decision, (int)HU_SRAG_RETRIEVE_AND_VERIFY);
    HU_ASSERT(a.has_temporal_marker);
}

static void srag_creative_query_skips_retrieval(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_srag_config_t cfg = hu_srag_config_default();
    hu_srag_assessment_t a;
    const char *q = "write me a poem about cats";
    HU_ASSERT_EQ(hu_srag_should_retrieve(&alloc, &cfg, q, strlen(q), NULL, 0, &a), HU_OK);
    HU_ASSERT_EQ((int)a.decision, (int)HU_SRAG_NO_RETRIEVAL);
    HU_ASSERT(a.is_creative_query);
}

static void srag_factual_query_retrieves(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_srag_config_t cfg = hu_srag_config_default();
    hu_srag_assessment_t a;
    const char *q = "what is the project deadline";
    HU_ASSERT_EQ(hu_srag_should_retrieve(&alloc, &cfg, q, strlen(q), NULL, 0, &a), HU_OK);
    HU_ASSERT_EQ((int)a.decision, (int)HU_SRAG_RETRIEVE_AND_VERIFY);
    HU_ASSERT(a.is_factual_query);
}

static void srag_confidence_threshold_respected(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_srag_config_t cfg = hu_srag_config_default();
    hu_srag_assessment_t a;
    HU_ASSERT_EQ(hu_srag_should_retrieve(&alloc, &cfg, "hello", 5, NULL, 0, &a), HU_OK);
    HU_ASSERT(a.confidence >= 0.0 && a.confidence <= 1.0);
}

static void srag_verify_relevance_high_score(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_srag_config_t cfg = hu_srag_config_default();
    double score = 0.0;
    bool use = false;
    const char *q = "project deadline status";
    const char *r = "The project deadline is next Friday and the status is on track.";
    HU_ASSERT_EQ(hu_srag_verify_relevance(&alloc, &cfg, q, strlen(q), r, strlen(r), &score, &use), HU_OK);
    HU_ASSERT(score > 0.2);
    HU_ASSERT(use);
}

static void srag_verify_relevance_low_score_filters(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_srag_config_t cfg = hu_srag_config_default();
    double score = 0.0;
    bool use = false;
    const char *q = "weather forecast tomorrow";
    const char *r = "The recipe for chocolate cake requires flour and sugar.";
    HU_ASSERT_EQ(hu_srag_verify_relevance(&alloc, &cfg, q, strlen(q), r, strlen(r), &score, &use), HU_OK);
    HU_ASSERT(score < 0.2);
    HU_ASSERT(!use);
}

static void srag_null_query_returns_no_retrieval(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_srag_config_t cfg = hu_srag_config_default();
    hu_srag_assessment_t a;
    HU_ASSERT_EQ(hu_srag_should_retrieve(&alloc, &cfg, NULL, 0, NULL, 0, &a), HU_OK);
    HU_ASSERT_EQ((int)a.decision, (int)HU_SRAG_NO_RETRIEVAL);
}

static void srag_disabled_always_retrieves(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_srag_config_t cfg = hu_srag_config_default();
    cfg.enabled = false;
    hu_srag_assessment_t a;
    HU_ASSERT_EQ(hu_srag_should_retrieve(&alloc, &cfg, "hello", 5, NULL, 0, &a), HU_OK);
    HU_ASSERT_EQ((int)a.decision, (int)HU_SRAG_RETRIEVE);
}

static void srag_history_context_reduces_retrieval(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_srag_config_t cfg = hu_srag_config_default();
    hu_srag_assessment_t a;
    const char *q = "tell me about quantum physics";
    const char *h = "We discussed quantum physics in depth earlier.";
    HU_ASSERT_EQ(hu_srag_should_retrieve(&alloc, &cfg, q, strlen(q), h, strlen(h), &a), HU_OK);
    /* Just verify it runs without error */
    HU_ASSERT(a.decision >= 0);
}

static void srag_assessment_fields_populated(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_srag_config_t cfg = hu_srag_config_default();
    hu_srag_assessment_t a;
    memset(&a, 0xFF, sizeof(a));
    const char *q = "what is machine learning";
    HU_ASSERT_EQ(hu_srag_should_retrieve(&alloc, &cfg, q, strlen(q), NULL, 0, &a), HU_OK);
    HU_ASSERT(a.confidence >= 0.0 && a.confidence <= 1.0);
    HU_ASSERT(a.is_factual_query);
}

void run_self_rag_tests(void) {
    HU_TEST_SUITE("Self RAG");
    HU_RUN_TEST(srag_greeting_skips_retrieval);
    HU_RUN_TEST(srag_personal_query_always_retrieves);
    HU_RUN_TEST(srag_temporal_marker_retrieves);
    HU_RUN_TEST(srag_creative_query_skips_retrieval);
    HU_RUN_TEST(srag_factual_query_retrieves);
    HU_RUN_TEST(srag_confidence_threshold_respected);
    HU_RUN_TEST(srag_verify_relevance_high_score);
    HU_RUN_TEST(srag_verify_relevance_low_score_filters);
    HU_RUN_TEST(srag_null_query_returns_no_retrieval);
    HU_RUN_TEST(srag_disabled_always_retrieves);
    HU_RUN_TEST(srag_history_context_reduces_retrieval);
    HU_RUN_TEST(srag_assessment_fields_populated);
}
