#include "seaclaw/agent/proactive.h"
#include "seaclaw/core/allocator.h"
#include "test_framework.h"
#include <string.h>

static void proactive_milestone_at_10_sessions(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_proactive_result_t result;
    memset(&result, 0, sizeof(result));

    SC_ASSERT_EQ(sc_proactive_check(&alloc, 10, 14, &result), SC_OK);
    SC_ASSERT_TRUE(result.count > 0);

    bool has_milestone = false;
    for (size_t i = 0; i < result.count; i++) {
        if (result.actions[i].type == SC_PROACTIVE_MILESTONE) {
            has_milestone = true;
            SC_ASSERT_NOT_NULL(result.actions[i].message);
            SC_ASSERT_TRUE(strstr(result.actions[i].message, "session 10") != NULL);
            SC_ASSERT_TRUE(strstr(result.actions[i].message, "milestone") != NULL);
            break;
        }
    }
    SC_ASSERT_TRUE(has_milestone);

    sc_proactive_result_deinit(&result, &alloc);
}

static void proactive_morning_briefing_at_9am(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_proactive_result_t result;
    memset(&result, 0, sizeof(result));

    SC_ASSERT_EQ(sc_proactive_check(&alloc, 5, 9, &result), SC_OK);
    SC_ASSERT_TRUE(result.count > 0);

    bool has_briefing = false;
    for (size_t i = 0; i < result.count; i++) {
        if (result.actions[i].type == SC_PROACTIVE_MORNING_BRIEFING) {
            has_briefing = true;
            SC_ASSERT_NOT_NULL(result.actions[i].message);
            SC_ASSERT_TRUE(strstr(result.actions[i].message, "Good morning") != NULL);
            break;
        }
    }
    SC_ASSERT_TRUE(has_briefing);

    sc_proactive_result_deinit(&result, &alloc);
}

static void proactive_no_milestone_at_7_sessions(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_proactive_result_t result;
    memset(&result, 0, sizeof(result));

    SC_ASSERT_EQ(sc_proactive_check(&alloc, 7, 14, &result), SC_OK);

    bool has_milestone = false;
    for (size_t i = 0; i < result.count; i++) {
        if (result.actions[i].type == SC_PROACTIVE_MILESTONE) {
            has_milestone = true;
            break;
        }
    }
    SC_ASSERT_FALSE(has_milestone);

    sc_proactive_result_deinit(&result, &alloc);
}

static void proactive_build_context_formats(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_proactive_result_t result;
    memset(&result, 0, sizeof(result));

    SC_ASSERT_EQ(sc_proactive_check(&alloc, 10, 9, &result), SC_OK);
    SC_ASSERT_TRUE(result.count >= 2);

    char *ctx = NULL;
    size_t ctx_len = 0;
    SC_ASSERT_EQ(sc_proactive_build_context(&result, &alloc, 8, &ctx, &ctx_len), SC_OK);
    SC_ASSERT_NOT_NULL(ctx);
    SC_ASSERT_TRUE(ctx_len > 0);
    SC_ASSERT_TRUE(strstr(ctx, "### Proactive Awareness") != NULL);
    SC_ASSERT_TRUE(strstr(ctx, "session 10") != NULL);
    SC_ASSERT_TRUE(strstr(ctx, "Good morning") != NULL);

    alloc.free(alloc.ctx, ctx, ctx_len + 1);
    sc_proactive_result_deinit(&result, &alloc);
}

void run_proactive_tests(void) {
    SC_TEST_SUITE("proactive");
    SC_RUN_TEST(proactive_milestone_at_10_sessions);
    SC_RUN_TEST(proactive_morning_briefing_at_9am);
    SC_RUN_TEST(proactive_no_milestone_at_7_sessions);
    SC_RUN_TEST(proactive_build_context_formats);
}
