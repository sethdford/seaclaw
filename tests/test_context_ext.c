#include "human/context/context_ext.h"
#include "human/core/allocator.h"
#include "human/core/string.h"
#include "test_framework.h"
#include <stdint.h>
#include <string.h>

static void test_forwarding_create_table_valid(void) {
    char buf[1024];
    size_t len = 0;
    hu_error_t err = hu_forwarding_create_table_sql(buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_TRUE(strstr(buf, "CREATE TABLE") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "shareable_content") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "shared_with") != NULL);
}

static void test_forwarding_insert_sql_valid(void) {
    hu_shareable_content_t c = {
        .content = "Check out this article",
        .content_len = 22,
        .source = "web",
        .source_len = 3,
        .topic = "tech",
        .topic_len = 4,
        .received_at = 1000,
        .share_score = 0.8,
    };
    char buf[512];
    size_t len = 0;
    hu_error_t err = hu_forwarding_insert_sql(&c, buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_TRUE(strstr(buf, "INSERT INTO") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "shareable_content") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "Check out this article") != NULL);
}

static void test_forwarding_query_sql_valid(void) {
    char buf[512];
    size_t len = 0;
    hu_error_t err = hu_forwarding_query_for_contact_sql("alice", 5, buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_TRUE(strstr(buf, "SELECT") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "alice") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "ORDER BY share_score") != NULL);
}

static void test_forwarding_score_topic_match_high(void) {
    double s = hu_forwarding_score(true, 0.9, 24, false);
    HU_ASSERT_TRUE(s > 0.8);
    HU_ASSERT_TRUE(s <= 1.0);
}

static void test_forwarding_score_no_match_low(void) {
    double s = hu_forwarding_score(false, 0.1, 168, true);
    HU_ASSERT_TRUE(s < 0.3);
}

static void test_weather_notable_snow_true(void) {
    bool notable = hu_weather_is_notable("snowing", 7, 45.0);
    HU_ASSERT_TRUE(notable);
}

static void test_weather_notable_normal_false(void) {
    bool notable = hu_weather_is_notable("sunny", 5, 72.0);
    HU_ASSERT_FALSE(notable);
}

static void test_weather_directive_notable_non_null(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_weather_state_t w = {
        .condition = hu_strndup(&alloc, "snowing", 7),
        .condition_len = 7,
        .temp_f = 30.0,
        .location = NULL,
        .location_len = 0,
        .is_notable = true,
    };
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_weather_build_directive(&alloc, &w, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(out_len > 0);
    HU_ASSERT_TRUE(strstr(out, "WEATHER") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
    hu_weather_state_deinit(&alloc, &w);
}

static void test_weather_directive_normal_null(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_weather_state_t w = {
        .condition = hu_strndup(&alloc, "sunny", 5),
        .condition_len = 5,
        .temp_f = 72.0,
        .location = NULL,
        .location_len = 0,
        .is_notable = false,
    };
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_weather_build_directive(&alloc, &w, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NULL(out);
    HU_ASSERT_EQ(out_len, 0);
    hu_weather_state_deinit(&alloc, &w);
}

static void test_events_create_table_valid(void) {
    char buf[1024];
    size_t len = 0;
    hu_error_t err = hu_events_create_table_sql(buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_TRUE(strstr(buf, "CREATE TABLE") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "current_events") != NULL);
}

static void test_events_insert_sql_valid(void) {
    hu_current_event_t e = {
        .topic = "election",
        .topic_len = 7,
        .summary = "Results announced",
        .summary_len = 16,
        .source = "rss",
        .source_len = 3,
        .published_at = 2000,
        .relevance = 0.9,
    };
    char buf[512];
    size_t len = 0;
    hu_error_t err = hu_events_insert_sql(&e, buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_TRUE(strstr(buf, "INSERT INTO") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "current_events") != NULL);
}

static void test_events_build_prompt_with_data(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_current_event_t events[1];
    memset(events, 0, sizeof(events));
    events[0].topic = hu_strndup(&alloc, "tech", 4);
    events[0].topic_len = 4;
    events[0].summary = hu_strndup(&alloc, "New release", 11);
    events[0].summary_len = 11;
    events[0].source = NULL;
    events[0].source_len = 0;
    events[0].published_at = 1000;
    events[0].relevance = 0.8;

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_events_build_prompt(&alloc, events, 1, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "CURRENT EVENTS") != NULL);
    HU_ASSERT_TRUE(strstr(out, "tech") != NULL);
    HU_ASSERT_TRUE(strstr(out, "New release") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
    hu_current_event_deinit(&alloc, &events[0]);
}

static void test_events_build_prompt_empty_no_notable(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_events_build_prompt(&alloc, NULL, 0, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "No notable") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void test_group_should_respond_mentioned_true(void) {
    hu_group_chat_state_t state = {
        .total_messages = 10,
        .our_messages = 2,
        .response_rate = 0.3,
        .was_mentioned = true,
        .has_direct_question = false,
        .active_participants = 5,
    };
    bool r = hu_group_should_respond(&state, 12345);
    HU_ASSERT_TRUE(r);
}

static void test_group_should_respond_question_true(void) {
    hu_group_chat_state_t state = {
        .total_messages = 10,
        .our_messages = 2,
        .response_rate = 0.3,
        .was_mentioned = false,
        .has_direct_question = true,
        .active_participants = 5,
    };
    bool r = hu_group_should_respond(&state, 12345);
    HU_ASSERT_TRUE(r);
}

static void test_group_should_respond_lurk_sometimes_false(void) {
    hu_group_chat_state_t state = {
        .total_messages = 10,
        .our_messages = 0,
        .response_rate = 0.1,
        .was_mentioned = false,
        .has_direct_question = false,
        .active_participants = 10,
    };
    bool any_false = false;
    for (uint32_t seed = 0; seed < 100; seed++) {
        bool r = hu_group_should_respond(&state, seed);
        if (!r)
            any_false = true;
    }
    HU_ASSERT_TRUE(any_false);
}

static void test_group_should_mention_match_true(void) {
    bool r = hu_group_should_mention("Hey Alice, what do you think?", 28, "Alice", 5);
    HU_ASSERT_TRUE(r);
}

static void test_group_should_mention_no_false(void) {
    bool r = hu_group_should_mention("Hey everyone", 11, "Alice", 5);
    HU_ASSERT_FALSE(r);
}

static void test_group_build_directive_contains_group_chat(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_group_chat_state_t state = {
        .total_messages = 10,
        .our_messages = 2,
        .response_rate = 0.3,
        .was_mentioned = true,
        .has_direct_question = false,
        .active_participants = 5,
    };
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_group_build_directive(&alloc, &state, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "GROUP CHAT") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void test_deinit_frees_all(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_shareable_content_t c = {0};
    c.content = hu_strndup(&alloc, "x", 1);
    c.content_len = 1;
    c.source = hu_strndup(&alloc, "y", 1);
    c.source_len = 1;
    c.topic = hu_strndup(&alloc, "z", 1);
    c.topic_len = 1;
    hu_shareable_content_deinit(&alloc, &c);
    HU_ASSERT_NULL(c.content);
    HU_ASSERT_NULL(c.source);
    HU_ASSERT_NULL(c.topic);
}

void run_context_ext_tests(void) {
    HU_TEST_SUITE("context_ext");
    HU_RUN_TEST(test_forwarding_create_table_valid);
    HU_RUN_TEST(test_forwarding_insert_sql_valid);
    HU_RUN_TEST(test_forwarding_query_sql_valid);
    HU_RUN_TEST(test_forwarding_score_topic_match_high);
    HU_RUN_TEST(test_forwarding_score_no_match_low);
    HU_RUN_TEST(test_weather_notable_snow_true);
    HU_RUN_TEST(test_weather_notable_normal_false);
    HU_RUN_TEST(test_weather_directive_notable_non_null);
    HU_RUN_TEST(test_weather_directive_normal_null);
    HU_RUN_TEST(test_events_create_table_valid);
    HU_RUN_TEST(test_events_insert_sql_valid);
    HU_RUN_TEST(test_events_build_prompt_with_data);
    HU_RUN_TEST(test_events_build_prompt_empty_no_notable);
    HU_RUN_TEST(test_group_should_respond_mentioned_true);
    HU_RUN_TEST(test_group_should_respond_question_true);
    HU_RUN_TEST(test_group_should_respond_lurk_sometimes_false);
    HU_RUN_TEST(test_group_should_mention_match_true);
    HU_RUN_TEST(test_group_should_mention_no_false);
    HU_RUN_TEST(test_group_build_directive_contains_group_chat);
    HU_RUN_TEST(test_deinit_frees_all);
}
