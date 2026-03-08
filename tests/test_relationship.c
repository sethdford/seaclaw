#include "seaclaw/core/allocator.h"
#include "seaclaw/persona/relationship.h"
#include "test_framework.h"
#include <string.h>

static void relationship_new_stage(void) {
    sc_relationship_state_t state = {0};
    SC_ASSERT_EQ(state.stage, SC_REL_NEW);
    sc_relationship_update(&state, 1);
    SC_ASSERT_EQ(state.stage, SC_REL_NEW);
    SC_ASSERT_EQ(state.session_count, 1u);
}

static void relationship_familiar_after_5(void) {
    sc_relationship_state_t state = {0};
    for (int i = 0; i < 5; i++)
        sc_relationship_update(&state, 1);
    SC_ASSERT_EQ(state.stage, SC_REL_FAMILIAR);
    SC_ASSERT_EQ(state.session_count, 5u);
}

static void relationship_trusted_after_20(void) {
    sc_relationship_state_t state = {0};
    for (int i = 0; i < 20; i++)
        sc_relationship_update(&state, 1);
    SC_ASSERT_EQ(state.stage, SC_REL_TRUSTED);
    SC_ASSERT_EQ(state.session_count, 20u);
}

static void relationship_deep_after_50(void) {
    sc_relationship_state_t state = {0};
    for (int i = 0; i < 50; i++)
        sc_relationship_update(&state, 1);
    SC_ASSERT_EQ(state.stage, SC_REL_DEEP);
    SC_ASSERT_EQ(state.session_count, 50u);
}

static void relationship_build_prompt_contains_stage(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_relationship_state_t state = {.stage = SC_REL_FAMILIAR, .session_count = 10, .total_turns = 25};
    char *out = NULL;
    size_t out_len = 0;
    sc_error_t err = sc_relationship_build_prompt(&alloc, &state, &out, &out_len);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(out);
    SC_ASSERT_TRUE(strstr(out, "Relationship Context") != NULL);
    SC_ASSERT_TRUE(strstr(out, "familiar") != NULL);
    SC_ASSERT_TRUE(strstr(out, "Sessions: 10") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

void run_relationship_tests(void) {
    SC_TEST_SUITE("relationship");
    SC_RUN_TEST(relationship_new_stage);
    SC_RUN_TEST(relationship_familiar_after_5);
    SC_RUN_TEST(relationship_trusted_after_20);
    SC_RUN_TEST(relationship_deep_after_50);
    SC_RUN_TEST(relationship_build_prompt_contains_stage);
}
