#include "human/memory/policy.h"
#include "test_framework.h"
#include <string.h>

static void test_mem_policy_init(void) {
    hu_mem_policy_t p;
    hu_mem_policy_init(&p);
    HU_ASSERT_TRUE(p.recency_weight > 0.0);
    HU_ASSERT_TRUE(p.relevance_weight > 0.0);
    HU_ASSERT_EQ((int)p.total_actions, 0);
}

static void test_mem_action_type_names(void) {
    HU_ASSERT_STR_EQ(hu_mem_action_type_name(HU_MEM_STORE), "store");
    HU_ASSERT_STR_EQ(hu_mem_action_type_name(HU_MEM_RETRIEVE), "retrieve");
    HU_ASSERT_STR_EQ(hu_mem_action_type_name(HU_MEM_UPDATE), "update");
    HU_ASSERT_STR_EQ(hu_mem_action_type_name(HU_MEM_SUMMARIZE), "summarize");
    HU_ASSERT_STR_EQ(hu_mem_action_type_name(HU_MEM_DISCARD), "discard");
}

static void test_mem_policy_score(void) {
    hu_mem_policy_t p;
    hu_mem_policy_init(&p);
    double score = hu_mem_policy_score(&p, 0.8, 0.9, 0.5);
    HU_ASSERT_TRUE(score > 0.5);
    HU_ASSERT_TRUE(score < 1.0);
}

static void test_mem_policy_decide_store(void) {
    hu_mem_policy_t p;
    hu_mem_policy_init(&p);
    hu_mem_state_t state = {.total_memories = 10,
                            .context_tokens = 1000,
                            .stm_entries = 5,
                            .ltm_entries = 5,
                            .avg_relevance = 0.7,
                            .context_pressure = 0.3};
    hu_mem_action_type_t action = hu_mem_policy_decide(&p, &state, "important new information", 24);
    HU_ASSERT_EQ((int)action, (int)HU_MEM_STORE);
}

static void test_mem_policy_decide_discard_high_pressure(void) {
    hu_mem_policy_t p;
    hu_mem_policy_init(&p);
    hu_mem_state_t state = {.total_memories = 500,
                            .context_tokens = 100000,
                            .stm_entries = 30,
                            .ltm_entries = 200,
                            .avg_relevance = 0.3,
                            .context_pressure = 0.95};
    hu_mem_action_type_t action = hu_mem_policy_decide(&p, &state, "data", 4);
    HU_ASSERT_EQ((int)action, (int)HU_MEM_DISCARD);
}

static void test_mem_policy_decide_summarize(void) {
    hu_mem_policy_t p;
    hu_mem_policy_init(&p);
    hu_mem_state_t state = {.total_memories = 100,
                            .context_tokens = 50000,
                            .stm_entries = 25,
                            .ltm_entries = 50,
                            .avg_relevance = 0.5,
                            .context_pressure = 0.85};
    hu_mem_action_type_t action = hu_mem_policy_decide(&p, &state, "data", 4);
    HU_ASSERT_EQ((int)action, (int)HU_MEM_SUMMARIZE);
}

static void test_mem_policy_decide_retrieve_empty(void) {
    hu_mem_policy_t p;
    hu_mem_policy_init(&p);
    hu_mem_state_t state = {0};
    hu_mem_action_type_t action = hu_mem_policy_decide(&p, &state, NULL, 0);
    HU_ASSERT_EQ((int)action, (int)HU_MEM_RETRIEVE);
}

static void test_mem_policy_record(void) {
    hu_mem_policy_t p;
    hu_mem_policy_init(&p);
    hu_mem_experience_t exp = {
        .state = {0},
        .action = {.type = HU_MEM_STORE},
        .reward = 0.8,
        .next_state = {0},
    };
    HU_ASSERT_EQ(hu_mem_policy_record(&p, &exp), HU_OK);
    HU_ASSERT_EQ((int)p.total_actions, 1);
    HU_ASSERT_EQ((int)p.store_count, 1);
    HU_ASSERT_EQ((int)p.buffer_count, 1);
}

static void test_mem_policy_record_null(void) {
    HU_ASSERT_EQ(hu_mem_policy_record(NULL, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void test_mem_policy_report(void) {
    hu_mem_policy_t p;
    hu_mem_policy_init(&p);
    p.total_actions = 42;
    p.store_count = 20;
    p.retrieve_count = 15;
    p.discard_count = 7;
    char buf[512];
    size_t len = hu_mem_policy_report(&p, buf, sizeof(buf));
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_STR_CONTAINS(buf, "Total actions: 42");
    HU_ASSERT_STR_CONTAINS(buf, "Store: 20");
}

void run_mem_policy_tests(void) {
    HU_TEST_SUITE("Memory Policy");
    HU_RUN_TEST(test_mem_policy_init);
    HU_RUN_TEST(test_mem_action_type_names);
    HU_RUN_TEST(test_mem_policy_score);
    HU_RUN_TEST(test_mem_policy_decide_store);
    HU_RUN_TEST(test_mem_policy_decide_discard_high_pressure);
    HU_RUN_TEST(test_mem_policy_decide_summarize);
    HU_RUN_TEST(test_mem_policy_decide_retrieve_empty);
    HU_RUN_TEST(test_mem_policy_record);
    HU_RUN_TEST(test_mem_policy_record_null);
    HU_RUN_TEST(test_mem_policy_report);
}
