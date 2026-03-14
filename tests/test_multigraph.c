/*
 * Multigraph retrieval (MAGMA-style) tests.
 */
#include "human/core/allocator.h"
#include "human/memory/graph.h"
#include "human/memory/retrieval_policy.h"
#include "test_framework.h"
#include <math.h>
#include <string.h>

static void default_policy_balanced(void) {
    hu_retrieval_policy_t p = hu_retrieval_policy_default();
    HU_ASSERT_EQ(p.active_count, 5);
    HU_ASSERT_EQ(p.total_budget_chars, 2000);
    for (size_t i = 0; i < 5; i++) {
        HU_ASSERT_FLOAT_EQ(p.dims[i].weight, 0.2f, 0.001f);
    }
}

static void intent_policy_factual(void) {
    hu_retrieval_policy_t p = hu_retrieval_policy_for_intent(HU_INTENT_FACTUAL);
    float max_w = 0.0f;
    for (size_t i = 0; i < p.active_count; i++) {
        if (p.dims[i].dim == HU_GRAPH_DIM_SEMANTIC)
            max_w = p.dims[i].weight;
    }
    HU_ASSERT_FLOAT_EQ(max_w, 0.4f, 0.001f);
}

static void intent_policy_temporal(void) {
    hu_retrieval_policy_t p = hu_retrieval_policy_for_intent(HU_INTENT_TEMPORAL);
    float max_w = 0.0f;
    for (size_t i = 0; i < p.active_count; i++) {
        if (p.dims[i].dim == HU_GRAPH_DIM_TEMPORAL)
            max_w = p.dims[i].weight;
    }
    HU_ASSERT_FLOAT_EQ(max_w, 0.5f, 0.001f);
}

static void intent_policy_causal(void) {
    hu_retrieval_policy_t p = hu_retrieval_policy_for_intent(HU_INTENT_CAUSAL);
    float max_w = 0.0f;
    for (size_t i = 0; i < p.active_count; i++) {
        if (p.dims[i].dim == HU_GRAPH_DIM_CAUSAL)
            max_w = p.dims[i].weight;
    }
    HU_ASSERT_FLOAT_EQ(max_w, 0.4f, 0.001f);
}

static void classify_temporal_query(void) {
    hu_query_intent_t intent = hu_query_classify_intent("When did we last meet?", 23);
    HU_ASSERT_EQ(intent, HU_INTENT_TEMPORAL);
}

static void classify_causal_query(void) {
    hu_query_intent_t intent = hu_query_classify_intent("Why did the server crash?", 26);
    HU_ASSERT_EQ(intent, HU_INTENT_CAUSAL);
}

static void classify_relational_query(void) {
    hu_query_intent_t intent = hu_query_classify_intent("Who knows about this?", 21);
    HU_ASSERT_EQ(intent, HU_INTENT_RELATIONAL);
}

static void classify_factual_query(void) {
    hu_query_intent_t intent = hu_query_classify_intent("What is the API endpoint?", 26);
    HU_ASSERT_EQ(intent, HU_INTENT_FACTUAL);
}

static void classify_exploratory_query(void) {
    hu_query_intent_t intent = hu_query_classify_intent("Tell me about the project", 25);
    HU_ASSERT_EQ(intent, HU_INTENT_EXPLORATORY);
}

static void retrieve_null_graph(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_retrieval_policy_t policy = hu_retrieval_policy_default();
    char *out = NULL;
    size_t out_len = 0;
    float score = -1.0f;

    hu_error_t err = hu_multigraph_retrieve(&alloc, NULL, "query", 5, &policy, &out, &out_len,
                                            &score);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NULL(out);
    HU_ASSERT_EQ(out_len, 0);
    HU_ASSERT_FLOAT_EQ(score, 0.0f, 0.001f);
}

static void retrieve_empty_policy(void) {
#ifdef HU_ENABLE_SQLITE
    hu_allocator_t alloc = hu_system_allocator();
    hu_graph_t *g = NULL;
    HU_ASSERT_EQ(hu_graph_open(&alloc, ":memory:", 8, &g), HU_OK);

    hu_retrieval_policy_t policy = {0};
    policy.active_count = 0;
    policy.total_budget_chars = 2000;

    char *out = NULL;
    size_t out_len = 0;
    float score = -1.0f;

    hu_error_t err =
        hu_multigraph_retrieve(&alloc, g, "query", 5, &policy, &out, &out_len, &score);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NULL(out);
    HU_ASSERT_EQ(out_len, 0);

    hu_graph_close(g, &alloc);
#else
    (void)0;
#endif
}

void run_multigraph_tests(void) {
    HU_TEST_SUITE("Multigraph Retrieval");
    HU_RUN_TEST(default_policy_balanced);
    HU_RUN_TEST(intent_policy_factual);
    HU_RUN_TEST(intent_policy_temporal);
    HU_RUN_TEST(intent_policy_causal);
    HU_RUN_TEST(classify_temporal_query);
    HU_RUN_TEST(classify_causal_query);
    HU_RUN_TEST(classify_relational_query);
    HU_RUN_TEST(classify_factual_query);
    HU_RUN_TEST(classify_exploratory_query);
    HU_RUN_TEST(retrieve_null_graph);
    HU_RUN_TEST(retrieve_empty_policy);
}
