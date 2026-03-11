#include "human/agent/conv_goals.h"
#include "human/core/allocator.h"
#include "human/core/string.h"
#include "test_framework.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static void conv_goals_create_table_sql_valid(void) {
    char buf[1024];
    size_t len = 0;

    HU_ASSERT_EQ(hu_conv_goals_create_table_sql(buf, sizeof(buf), &len), HU_OK);
    HU_ASSERT_TRUE(strstr(buf, "CREATE TABLE") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "conversation_goals") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "contact_id") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "description") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "success_signal") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "status") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "priority") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "created_at") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "target_by") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "attempts") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "max_attempts") != NULL);
}

static void conv_goals_insert_sql_escapes_strings(void) {
    hu_conv_goal_t goal = {
        .id = 1,
        .contact_id = (char *)"user_a",
        .contact_id_len = 6,
        .description = (char *)"Check O'Malley's feelings",
        .description_len = 25,
        .success_signal = NULL,
        .success_signal_len = 0,
        .status = HU_GOAL_PENDING,
        .priority = HU_GOAL_MEDIUM,
        .created_at = 1000000ULL,
        .target_by = 0,
        .achieved_at = 0,
        .attempts = 0,
        .max_attempts = 5,
    };
    char buf[1024];
    size_t len = 0;

    HU_ASSERT_EQ(hu_conv_goals_insert_sql(&goal, buf, sizeof(buf), &len), HU_OK);
    HU_ASSERT_TRUE(strstr(buf, "O''Malley''s") != NULL);
}

static void conv_goals_update_status_sql_valid(void) {
    char buf[256];
    size_t len = 0;

    HU_ASSERT_EQ(
        hu_conv_goals_update_status_sql(42, HU_GOAL_ACHIEVED, buf, sizeof(buf), &len), HU_OK);
    HU_ASSERT_TRUE(strstr(buf, "UPDATE conversation_goals") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "status") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "achieved") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "42") != NULL);
}

static void conv_goals_query_active_sql_valid(void) {
    char buf[512];
    size_t len = 0;
    const char *contact = "user_a";

    HU_ASSERT_EQ(
        hu_conv_goals_query_active_sql(contact, strlen(contact), buf, sizeof(buf), &len), HU_OK);
    HU_ASSERT_TRUE(strstr(buf, "WHERE contact_id") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "user_a") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "pending") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "in_progress") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "ORDER BY priority DESC") != NULL);
}

static void conv_goals_should_abandon_max_attempts(void) {
    hu_conv_goal_t goal = {
        .attempts = 5,
        .max_attempts = 5,
        .target_by = 0,
    };
    HU_ASSERT_TRUE(hu_conv_goal_should_abandon(&goal, 1000000ULL));
}

static void conv_goals_should_abandon_past_deadline(void) {
    hu_conv_goal_t goal = {
        .attempts = 2,
        .max_attempts = 5,
        .target_by = 500000ULL,
    };
    uint64_t now = 600000ULL;
    HU_ASSERT_TRUE(hu_conv_goal_should_abandon(&goal, now));
}

static void conv_goals_should_abandon_false(void) {
    hu_conv_goal_t goal = {
        .attempts = 2,
        .max_attempts = 5,
        .target_by = 0,
    };
    HU_ASSERT_FALSE(hu_conv_goal_should_abandon(&goal, 1000000ULL));
}

static void conv_goals_record_attempt_increments(void) {
    hu_conv_goal_t goal = {
        .attempts = 2,
        .max_attempts = 5,
        .status = HU_GOAL_IN_PROGRESS,
    };

    HU_ASSERT_EQ(hu_conv_goal_record_attempt(&goal), HU_OK);
    HU_ASSERT_EQ(goal.attempts, 3);
}

static void conv_goals_record_attempt_auto_abandons(void) {
    hu_conv_goal_t goal = {
        .attempts = 5,
        .max_attempts = 5,
        .status = HU_GOAL_IN_PROGRESS,
    };

    HU_ASSERT_EQ(hu_conv_goal_record_attempt(&goal), HU_OK);
    HU_ASSERT_EQ(goal.attempts, 6);
    HU_ASSERT_EQ(goal.status, HU_GOAL_ABANDONED);
}

static void conv_goals_urgency_critical_soon(void) {
    uint64_t now = 1000000ULL * 86400000ULL; /* ~1000 days from epoch */
    uint64_t tomorrow = now + 86400000ULL;
    hu_conv_goal_t goal = {
        .priority = HU_GOAL_CRITICAL,
        .target_by = tomorrow,
        .attempts = 0,
        .max_attempts = 5,
    };
    double u = hu_conv_goal_urgency(&goal, now);
    HU_ASSERT_TRUE(u > 0.8);
}

static void conv_goals_urgency_low_no_deadline(void) {
    hu_conv_goal_t goal = {
        .priority = HU_GOAL_LOW,
        .target_by = 0,
        .attempts = 0,
        .max_attempts = 5,
    };
    double u = hu_conv_goal_urgency(&goal, 1000000ULL);
    /* LOW (0.2) + no deadline (0.3) + 0 attempts (1.0) → ~0.47, lowest among typical cases */
    HU_ASSERT_TRUE(u < 0.5);
}

static void conv_goals_urgency_medium_far_deadline(void) {
    uint64_t now = 1000000ULL * 86400000ULL;
    uint64_t in_30_days = now + 30ULL * 86400000ULL;
    hu_conv_goal_t goal = {
        .priority = HU_GOAL_MEDIUM,
        .target_by = in_30_days,
        .attempts = 1,
        .max_attempts = 5,
    };
    double u = hu_conv_goal_urgency(&goal, now);
    HU_ASSERT_TRUE(u > 0.3 && u < 0.7);
}

static void conv_goals_build_prompt_with_goals(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_conv_goal_t goals[2] = {
        {
            .description = (char *)"Check on them after the breakup",
            .description_len = 32,
            .success_signal = (char *)"they share how they're feeling",
            .success_signal_len = 30,
            .priority = HU_GOAL_HIGH,
            .attempts = 0,
            .max_attempts = 5,
        },
        {
            .description = (char *)"Mention the concert Saturday",
            .description_len = 28,
            .success_signal = NULL,
            .success_signal_len = 0,
            .priority = HU_GOAL_MEDIUM,
            .attempts = 1,
            .max_attempts = 5,
        },
    };
    char *out = NULL;
    size_t out_len = 0;

    HU_ASSERT_EQ(hu_conv_goals_build_prompt(&alloc, goals, 2, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "Check on them after the breakup") != NULL);
    HU_ASSERT_TRUE(strstr(out, "Mention the concert Saturday") != NULL);
    HU_ASSERT_TRUE(strstr(out, "they share how they're feeling") != NULL);
    HU_ASSERT_TRUE(strstr(out, "Steer conversation") != NULL);

    hu_str_free(&alloc, out);
}

static void conv_goals_build_prompt_no_goals(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;

    HU_ASSERT_EQ(hu_conv_goals_build_prompt(&alloc, NULL, 0, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "No active conversation goals") != NULL);

    hu_str_free(&alloc, out);
}

static void conv_goals_status_str_roundtrip(void) {
    hu_goal_status_t statuses[] = {
        HU_GOAL_PENDING, HU_GOAL_IN_PROGRESS, HU_GOAL_ACHIEVED, HU_GOAL_ABANDONED, HU_GOAL_DEFERRED,
    };
    for (size_t i = 0; i < sizeof(statuses) / sizeof(statuses[0]); i++) {
        const char *str = hu_goal_status_str(statuses[i]);
        hu_goal_status_t out;
        HU_ASSERT_TRUE(hu_goal_status_from_str(str, &out));
        HU_ASSERT_EQ(out, statuses[i]);
    }
}

static void conv_goals_status_from_str_unknown(void) {
    hu_goal_status_t out;
    HU_ASSERT_FALSE(hu_goal_status_from_str("garbage", &out));
}

static void conv_goals_priority_str_values(void) {
    HU_ASSERT_STR_EQ(hu_goal_priority_str(HU_GOAL_LOW), "low");
    HU_ASSERT_STR_EQ(hu_goal_priority_str(HU_GOAL_CRITICAL), "critical");
}

static void conv_goals_deinit_frees_all(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_conv_goal_t goal = {0};
    goal.contact_id = hu_strndup(&alloc, "user_a", 6);
    goal.contact_id_len = 6;
    goal.description = hu_strndup(&alloc, "check on them", 13);
    goal.description_len = 13;
    goal.success_signal = hu_strndup(&alloc, "they respond", 11);
    goal.success_signal_len = 11;

    hu_conv_goal_deinit(&alloc, &goal);

    HU_ASSERT_NULL(goal.contact_id);
    HU_ASSERT_NULL(goal.description);
    HU_ASSERT_NULL(goal.success_signal);
}

void run_conv_goals_tests(void) {
    HU_TEST_SUITE("conv_goals");
    HU_RUN_TEST(conv_goals_create_table_sql_valid);
    HU_RUN_TEST(conv_goals_insert_sql_escapes_strings);
    HU_RUN_TEST(conv_goals_update_status_sql_valid);
    HU_RUN_TEST(conv_goals_query_active_sql_valid);
    HU_RUN_TEST(conv_goals_should_abandon_max_attempts);
    HU_RUN_TEST(conv_goals_should_abandon_past_deadline);
    HU_RUN_TEST(conv_goals_should_abandon_false);
    HU_RUN_TEST(conv_goals_record_attempt_increments);
    HU_RUN_TEST(conv_goals_record_attempt_auto_abandons);
    HU_RUN_TEST(conv_goals_urgency_critical_soon);
    HU_RUN_TEST(conv_goals_urgency_low_no_deadline);
    HU_RUN_TEST(conv_goals_urgency_medium_far_deadline);
    HU_RUN_TEST(conv_goals_build_prompt_with_goals);
    HU_RUN_TEST(conv_goals_build_prompt_no_goals);
    HU_RUN_TEST(conv_goals_status_str_roundtrip);
    HU_RUN_TEST(conv_goals_status_from_str_unknown);
    HU_RUN_TEST(conv_goals_priority_str_values);
    HU_RUN_TEST(conv_goals_deinit_frees_all);
}
