#include "seaclaw/core/allocator.h"
#include "seaclaw/memory/emotional_graph.h"
#include "seaclaw/memory/stm.h"
#include "test_framework.h"
#include <stdio.h>
#include <string.h>

static void egraph_record_and_query(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_emotional_graph_t g;
    sc_egraph_init(&g, alloc);

    sc_error_t err = sc_egraph_record(&g, "work", 4, SC_EMOTION_JOY, 0.8);
    SC_ASSERT_EQ(err, SC_OK);

    double avg = 0.0;
    sc_emotion_tag_t tag = sc_egraph_query(&g, "work", 4, &avg);
    SC_ASSERT_EQ(tag, SC_EMOTION_JOY);
    SC_ASSERT_EQ(avg, 0.8);

    sc_egraph_deinit(&g);
}

static void egraph_running_average(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_emotional_graph_t g;
    sc_egraph_init(&g, alloc);

    SC_ASSERT_EQ(sc_egraph_record(&g, "work", 4, SC_EMOTION_JOY, 0.6), SC_OK);
    SC_ASSERT_EQ(sc_egraph_record(&g, "work", 4, SC_EMOTION_JOY, 0.8), SC_OK);

    double avg = 0.0;
    sc_emotion_tag_t tag = sc_egraph_query(&g, "work", 4, &avg);
    SC_ASSERT_EQ(tag, SC_EMOTION_JOY);
    SC_ASSERT_EQ(avg, 0.7);

    sc_egraph_deinit(&g);
}

static void egraph_multiple_emotions(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_emotional_graph_t g;
    sc_egraph_init(&g, alloc);

    SC_ASSERT_EQ(sc_egraph_record(&g, "work", 4, SC_EMOTION_JOY, 0.3), SC_OK);
    SC_ASSERT_EQ(sc_egraph_record(&g, "work", 4, SC_EMOTION_ANXIETY, 0.9), SC_OK);

    double avg = 0.0;
    sc_emotion_tag_t tag = sc_egraph_query(&g, "work", 4, &avg);
    SC_ASSERT_EQ(tag, SC_EMOTION_ANXIETY);
    SC_ASSERT_EQ(avg, 0.9);

    sc_egraph_deinit(&g);
}

static void egraph_build_context_with_data(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_emotional_graph_t g;
    sc_egraph_init(&g, alloc);

    SC_ASSERT_EQ(sc_egraph_record(&g, "work", 4, SC_EMOTION_ANXIETY, 0.85), SC_OK);
    SC_ASSERT_EQ(sc_egraph_record(&g, "cooking", 7, SC_EMOTION_JOY, 0.5), SC_OK);

    size_t len = 0;
    char *ctx = sc_egraph_build_context(&alloc, &g, &len);
    SC_ASSERT_NOT_NULL(ctx);
    SC_ASSERT_TRUE(len > 0);
    SC_ASSERT_TRUE(strstr(ctx, "### Emotional Topic Map") != NULL);
    SC_ASSERT_TRUE(strstr(ctx, "work") != NULL);
    SC_ASSERT_TRUE(strstr(ctx, "cooking") != NULL);
    SC_ASSERT_TRUE(strstr(ctx, "anxiety") != NULL);
    SC_ASSERT_TRUE(strstr(ctx, "joy") != NULL);
    SC_ASSERT_TRUE(strstr(ctx, "high") != NULL);
    SC_ASSERT_TRUE(strstr(ctx, "moderate") != NULL);

    alloc.free(alloc.ctx, ctx, len + 1);
    sc_egraph_deinit(&g);
}

static void egraph_build_context_empty(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_emotional_graph_t g;
    sc_egraph_init(&g, alloc);

    size_t len = 0;
    char *ctx = sc_egraph_build_context(&alloc, &g, &len);
    SC_ASSERT_NULL(ctx);
    SC_ASSERT_EQ(len, 0);

    sc_egraph_deinit(&g);
}

static void egraph_populate_from_stm(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_stm_buffer_t buf;
    sc_stm_init(&buf, alloc, "sess", 4);
    sc_stm_record_turn(&buf, "user", 4, "Work is stressing me out", 24, 1000);
    sc_stm_turn_set_primary_topic(&buf, 0, "work", 4);
    sc_stm_turn_add_emotion(&buf, 0, SC_EMOTION_ANXIETY, 0.7);

    sc_emotional_graph_t g;
    sc_egraph_init(&g, alloc);
    sc_error_t err = sc_egraph_populate_from_stm(&g, &buf);
    SC_ASSERT_EQ(err, SC_OK);

    double avg = 0.0;
    sc_emotion_tag_t tag = sc_egraph_query(&g, "work", 4, &avg);
    SC_ASSERT_EQ(tag, SC_EMOTION_ANXIETY);
    SC_ASSERT_EQ(avg, 0.7);

    sc_egraph_deinit(&g);
    sc_stm_deinit(&buf);
}

static void egraph_case_insensitive(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_emotional_graph_t g;
    sc_egraph_init(&g, alloc);

    SC_ASSERT_EQ(sc_egraph_record(&g, "Work", 4, SC_EMOTION_JOY, 0.8), SC_OK);

    double avg = 0.0;
    sc_emotion_tag_t tag = sc_egraph_query(&g, "work", 4, &avg);
    SC_ASSERT_EQ(tag, SC_EMOTION_JOY);
    SC_ASSERT_EQ(avg, 0.8);

    SC_ASSERT_EQ(sc_egraph_record(&g, "work", 4, SC_EMOTION_JOY, 0.6), SC_OK);
    tag = sc_egraph_query(&g, "WORK", 4, &avg);
    SC_ASSERT_EQ(tag, SC_EMOTION_JOY);
    SC_ASSERT_EQ(avg, 0.7);

    sc_egraph_deinit(&g);
}

static void egraph_populate_from_stm_topic_entities(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_stm_buffer_t buf;
    sc_stm_init(&buf, alloc, "sess", 4);
    sc_stm_record_turn(&buf, "user", 4, "Cooking makes me happy", 22, 1000);
    sc_stm_turn_add_entity(&buf, 0, "cooking", 7, "topic", 5, 1);
    sc_stm_turn_add_emotion(&buf, 0, SC_EMOTION_JOY, 0.9);

    sc_emotional_graph_t g;
    sc_egraph_init(&g, alloc);
    sc_error_t err = sc_egraph_populate_from_stm(&g, &buf);
    SC_ASSERT_EQ(err, SC_OK);

    double avg = 0.0;
    sc_emotion_tag_t tag = sc_egraph_query(&g, "cooking", 7, &avg);
    SC_ASSERT_EQ(tag, SC_EMOTION_JOY);
    SC_ASSERT_EQ(avg, 0.9);

    sc_egraph_deinit(&g);
    sc_stm_deinit(&buf);
}

static void egraph_query_unknown_topic(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_emotional_graph_t g;
    sc_egraph_init(&g, alloc);

    double avg = 0.0;
    sc_emotion_tag_t tag = sc_egraph_query(&g, "never_recorded", 14, &avg);
    SC_ASSERT_EQ(tag, SC_EMOTION_NEUTRAL);
    SC_ASSERT_EQ(avg, 0.0);

    sc_egraph_deinit(&g);
}

static void egraph_populate_from_stm_empty(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_stm_buffer_t buf;
    sc_stm_init(&buf, alloc, "sess", 4);

    sc_emotional_graph_t g;
    sc_egraph_init(&g, alloc);
    sc_error_t err = sc_egraph_populate_from_stm(&g, &buf);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(g.edge_count, 0);

    sc_egraph_deinit(&g);
    sc_stm_deinit(&buf);
}

static void egraph_init_null(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_error_t err = sc_egraph_init(NULL, alloc);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
}

static void egraph_record_null_topic(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_emotional_graph_t g;
    sc_egraph_init(&g, alloc);

    sc_error_t err = sc_egraph_record(&g, NULL, 0, SC_EMOTION_JOY, 0.5);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);

    sc_egraph_deinit(&g);
}

static void egraph_at_capacity(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_emotional_graph_t g;
    sc_egraph_init(&g, alloc);

    char topic[16];
    for (size_t i = 0; i < SC_EGRAPH_MAX_NODES; i++) {
        int n = snprintf(topic, sizeof(topic), "topic_%zu", i);
        SC_ASSERT_TRUE(n > 0 && (size_t)n < sizeof(topic));
        sc_error_t err = sc_egraph_record(&g, topic, (size_t)n, SC_EMOTION_JOY, 0.5);
        SC_ASSERT_EQ(err, SC_OK);
    }

    sc_error_t err = sc_egraph_record(&g, "extra_topic", 11, SC_EMOTION_JOY, 0.5);
    SC_ASSERT_EQ(err, SC_ERR_OUT_OF_MEMORY);

    sc_egraph_deinit(&g);
}

static void egraph_deinit_twice(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_emotional_graph_t g;
    sc_egraph_init(&g, alloc);
    (void)sc_egraph_record(&g, "work", 4, SC_EMOTION_JOY, 0.8);

    sc_egraph_deinit(&g);
    sc_egraph_deinit(&g);
}

void run_emotional_graph_tests(void) {
    SC_TEST_SUITE("emotional_graph");
    SC_RUN_TEST(egraph_record_and_query);
    SC_RUN_TEST(egraph_running_average);
    SC_RUN_TEST(egraph_multiple_emotions);
    SC_RUN_TEST(egraph_build_context_with_data);
    SC_RUN_TEST(egraph_build_context_empty);
    SC_RUN_TEST(egraph_populate_from_stm);
    SC_RUN_TEST(egraph_case_insensitive);
    SC_RUN_TEST(egraph_populate_from_stm_topic_entities);
    SC_RUN_TEST(egraph_query_unknown_topic);
    SC_RUN_TEST(egraph_populate_from_stm_empty);
    SC_RUN_TEST(egraph_init_null);
    SC_RUN_TEST(egraph_record_null_topic);
    SC_RUN_TEST(egraph_at_capacity);
    SC_RUN_TEST(egraph_deinit_twice);
}
