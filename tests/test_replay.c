#include "seaclaw/core/allocator.h"
#include "seaclaw/persona/replay.h"
#include "test_framework.h"
#include <string.h>

static sc_channel_history_entry_t make_entry(bool from_me, const char *text, const char *ts) {
    sc_channel_history_entry_t e;
    memset(&e, 0, sizeof(e));
    e.from_me = from_me;
    size_t tl = strlen(text);
    if (tl >= sizeof(e.text))
        tl = sizeof(e.text) - 1;
    memcpy(e.text, text, tl);
    e.text[tl] = '\0';
    size_t tsl = strlen(ts);
    if (tsl >= sizeof(e.timestamp))
        tsl = sizeof(e.timestamp) - 1;
    memcpy(e.timestamp, ts, tsl);
    e.timestamp[tsl] = '\0';
    return e;
}

static void replay_detects_positive_signal(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_channel_history_entry_t entries[2] = {
        make_entry(true, "that's so funny", "12:00"),
        make_entry(false, "haha exactly!", "12:01"),
    };
    sc_replay_result_t result = {0};
    sc_error_t err = sc_replay_analyze(&alloc, entries, 2, 300, &result);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_TRUE(result.insight_count >= 1);
    bool found_positive = false;
    for (size_t i = 0; i < result.insight_count; i++) {
        if (result.insights[i].score_delta > 0) {
            found_positive = true;
            SC_ASSERT_TRUE(result.insights[i].observation != NULL);
            SC_ASSERT_TRUE(strstr(result.insights[i].observation, "enthusiastic") != NULL);
            break;
        }
    }
    SC_ASSERT_TRUE(found_positive);
    sc_replay_result_deinit(&result, &alloc);
}

static void replay_detects_robotic_tell(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_channel_history_entry_t entries[2] = {
        make_entry(false, "I've been really stressed about work", "12:00"),
        make_entry(true, "I understand how you feel. That must be difficult.", "12:01"),
    };
    sc_replay_result_t result = {0};
    sc_error_t err = sc_replay_analyze(&alloc, entries, 2, 300, &result);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_TRUE(result.insight_count >= 1);
    bool found_negative = false;
    for (size_t i = 0; i < result.insight_count; i++) {
        if (result.insights[i].score_delta < 0) {
            found_negative = true;
            SC_ASSERT_TRUE(result.insights[i].observation != NULL);
            SC_ASSERT_TRUE(strstr(result.insights[i].observation, "robotic") != NULL ||
                          strstr(result.insights[i].observation, "formulaic") != NULL);
            break;
        }
    }
    SC_ASSERT_TRUE(found_negative);
    sc_replay_result_deinit(&result, &alloc);
}

static void replay_empty_history(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_replay_result_t result = {0};
    sc_error_t err = sc_replay_analyze(&alloc, NULL, 0, 300, &result);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(result.insight_count, 0u);
    SC_ASSERT_EQ(result.overall_score, 0);
}

static void replay_null_args(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_channel_history_entry_t entries[1] = {make_entry(true, "hi", "12:00")};
    sc_replay_result_t result = {0};

    sc_error_t err = sc_replay_analyze(NULL, entries, 1, 300, &result);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);

    err = sc_replay_analyze(&alloc, entries, 1, 300, NULL);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);

    err = sc_replay_analyze(&alloc, NULL, 1, 300, &result);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
}

static void replay_build_context_with_insights(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_channel_history_entry_t entries[2] = {
        make_entry(true, "that's awesome", "12:00"),
        make_entry(false, "yes!!", "12:01"),
    };
    sc_replay_result_t result = {0};
    sc_error_t err = sc_replay_analyze(&alloc, entries, 2, 300, &result);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_TRUE(result.insight_count >= 1);

    size_t len = 0;
    char *ctx = sc_replay_build_context(&alloc, &result, &len);
    SC_ASSERT_NOT_NULL(ctx);
    SC_ASSERT_TRUE(len > 0);
    SC_ASSERT_TRUE(strstr(ctx, "Session Replay") != NULL);
    alloc.free(alloc.ctx, ctx, len + 1);
    sc_replay_result_deinit(&result, &alloc);
}

static void replay_build_context_empty_returns_null(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_replay_result_t result = {0};
    result.insight_count = 0;
    size_t len = 0;
    char *ctx = sc_replay_build_context(&alloc, &result, &len);
    SC_ASSERT_NULL(ctx);
    SC_ASSERT_EQ(len, 0u);
}

void run_replay_tests(void) {
    SC_TEST_SUITE("replay");
    SC_RUN_TEST(replay_detects_positive_signal);
    SC_RUN_TEST(replay_detects_robotic_tell);
    SC_RUN_TEST(replay_empty_history);
    SC_RUN_TEST(replay_null_args);
    SC_RUN_TEST(replay_build_context_with_insights);
    SC_RUN_TEST(replay_build_context_empty_returns_null);
}
