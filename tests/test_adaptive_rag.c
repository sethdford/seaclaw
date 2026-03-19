#include "test_framework.h"
#include "human/memory/adaptive_rag.h"
#include <string.h>

static void adaptive_rag_keyword_for_short_query(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_adaptive_rag_t rag;
    HU_ASSERT_EQ(hu_adaptive_rag_create(&alloc, NULL, &rag), HU_OK);
    hu_rag_strategy_t s = hu_adaptive_rag_select(&rag, "hello", 5);
    HU_ASSERT_EQ((int)s, (int)HU_RAG_KEYWORD);
    hu_adaptive_rag_deinit(&rag);
}

static void adaptive_rag_semantic_for_conceptual_query(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_adaptive_rag_t rag;
    HU_ASSERT_EQ(hu_adaptive_rag_create(&alloc, NULL, &rag), HU_OK);
    const char *q = "explain quantum entanglement theory";
    hu_rag_strategy_t s = hu_adaptive_rag_select(&rag, q, strlen(q));
    /* "explain" triggers is_factual, word_count=4 so not >=5, falls through */
    HU_ASSERT(s == HU_RAG_SEMANTIC || s == HU_RAG_CORRECTIVE || s == HU_RAG_HYBRID);
    hu_adaptive_rag_deinit(&rag);
}

static void adaptive_rag_graph_for_relationship_query(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_adaptive_rag_t rag;
    HU_ASSERT_EQ(hu_adaptive_rag_create(&alloc, NULL, &rag), HU_OK);
    const char *q = "who knows John and is related to the project";
    hu_rag_strategy_t s = hu_adaptive_rag_select(&rag, q, strlen(q));
    HU_ASSERT_EQ((int)s, (int)HU_RAG_GRAPH);
    hu_adaptive_rag_deinit(&rag);
}

static void adaptive_rag_hybrid_for_complex_query(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_adaptive_rag_t rag;
    HU_ASSERT_EQ(hu_adaptive_rag_create(&alloc, NULL, &rag), HU_OK);
    const char *q = "I want to understand my previous conversations about machine learning and the recommendations you made";
    hu_rag_strategy_t s = hu_adaptive_rag_select(&rag, q, strlen(q));
    /* Long query with personal markers → HYBRID */
    HU_ASSERT(s == HU_RAG_HYBRID || s == HU_RAG_SEMANTIC);
    hu_adaptive_rag_deinit(&rag);
}

static void adaptive_rag_corrective_for_factual_query(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_adaptive_rag_t rag;
    HU_ASSERT_EQ(hu_adaptive_rag_create(&alloc, NULL, &rag), HU_OK);
    const char *q = "what is the capital of France according to our records";
    hu_rag_strategy_t s = hu_adaptive_rag_select(&rag, q, strlen(q));
    HU_ASSERT_EQ((int)s, (int)HU_RAG_CORRECTIVE);
    hu_adaptive_rag_deinit(&rag);
}

static void adaptive_rag_features_extraction(void) {
    hu_rag_query_features_t f;
    const char *q = "what is the status of Project Alpha recently";
    HU_ASSERT_EQ(hu_adaptive_rag_extract_features(q, strlen(q), &f), HU_OK);
    HU_ASSERT_EQ((int)f.word_count, 8);
    HU_ASSERT(f.has_entity_names); /* "Project" and "Alpha" capitalized */
    HU_ASSERT(f.has_temporal_marker); /* "recently" */
    HU_ASSERT(f.is_factual); /* "what is" */
}

static void adaptive_rag_record_outcome_updates_weights(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_adaptive_rag_t rag;
    HU_ASSERT_EQ(hu_adaptive_rag_create(&alloc, NULL, &rag), HU_OK);
    double orig = rag.strategy_weights[HU_RAG_SEMANTIC];
    HU_ASSERT_EQ(hu_adaptive_rag_record_outcome(&rag, HU_RAG_SEMANTIC, 0.0), HU_OK);
    HU_ASSERT(rag.strategy_weights[HU_RAG_SEMANTIC] < orig);
    hu_adaptive_rag_deinit(&rag);
}

static void adaptive_rag_learned_weights_influence_selection(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_adaptive_rag_t rag;
    HU_ASSERT_EQ(hu_adaptive_rag_create(&alloc, NULL, &rag), HU_OK);
    /* Force KEYWORD weight very low */
    rag.strategy_weights[HU_RAG_KEYWORD] = 0.1;
    hu_rag_strategy_t s = hu_adaptive_rag_select(&rag, "test", 4);
    /* Would normally be KEYWORD, but weight is below 0.3 so fallback */
    HU_ASSERT_NEQ((int)s, (int)HU_RAG_KEYWORD);
    hu_adaptive_rag_deinit(&rag);
}

static void adaptive_rag_null_query_returns_keyword(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_adaptive_rag_t rag;
    HU_ASSERT_EQ(hu_adaptive_rag_create(&alloc, NULL, &rag), HU_OK);
    HU_ASSERT_EQ((int)hu_adaptive_rag_select(&rag, NULL, 0), (int)HU_RAG_KEYWORD);
    hu_adaptive_rag_deinit(&rag);
}

static void adaptive_rag_create_without_db_uses_heuristics(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_adaptive_rag_t rag;
    HU_ASSERT_EQ(hu_adaptive_rag_create(&alloc, NULL, &rag), HU_OK);
    HU_ASSERT(rag.db == NULL);
    const char *q = "explain neural networks in detail please";
    hu_rag_strategy_t s = hu_adaptive_rag_select(&rag, q, strlen(q));
    HU_ASSERT(s != HU_RAG_NONE);
    hu_adaptive_rag_deinit(&rag);
}

static void adaptive_rag_strategy_count_tracking(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_adaptive_rag_t rag;
    HU_ASSERT_EQ(hu_adaptive_rag_create(&alloc, NULL, &rag), HU_OK);
    hu_adaptive_rag_select(&rag, "hello", 5);
    hu_adaptive_rag_select(&rag, "hello", 5);
    HU_ASSERT_EQ((int)rag.strategy_uses[HU_RAG_KEYWORD], 2);
    hu_adaptive_rag_deinit(&rag);
}

static void adaptive_rag_deinit_cleans_up(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_adaptive_rag_t rag;
    HU_ASSERT_EQ(hu_adaptive_rag_create(&alloc, NULL, &rag), HU_OK);
    hu_adaptive_rag_deinit(&rag);
    HU_ASSERT(rag.alloc == NULL);
}

void run_adaptive_rag_tests(void) {
    HU_TEST_SUITE("Adaptive RAG");
    HU_RUN_TEST(adaptive_rag_keyword_for_short_query);
    HU_RUN_TEST(adaptive_rag_semantic_for_conceptual_query);
    HU_RUN_TEST(adaptive_rag_graph_for_relationship_query);
    HU_RUN_TEST(adaptive_rag_hybrid_for_complex_query);
    HU_RUN_TEST(adaptive_rag_corrective_for_factual_query);
    HU_RUN_TEST(adaptive_rag_features_extraction);
    HU_RUN_TEST(adaptive_rag_record_outcome_updates_weights);
    HU_RUN_TEST(adaptive_rag_learned_weights_influence_selection);
    HU_RUN_TEST(adaptive_rag_null_query_returns_keyword);
    HU_RUN_TEST(adaptive_rag_create_without_db_uses_heuristics);
    HU_RUN_TEST(adaptive_rag_strategy_count_tracking);
    HU_RUN_TEST(adaptive_rag_deinit_cleans_up);
}
