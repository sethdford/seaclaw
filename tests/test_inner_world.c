#include "human/core/allocator.h"
#include "human/persona.h"
#include "human/persona/relationship.h"
#include "test_framework.h"
#include <string.h>

/* Helper: build a persona with inner world content */
static hu_persona_t make_inner_world_persona(void) {
    hu_persona_t p = {0};
    static char *contradictions[] = {(char *)"Says values solitude but texts constantly"};
    static char *embodied[] = {(char *)"Smell of pine trees on a mountain trail"};
    static char *flashpoints[] = {(char *)"Mentions of absent parent"};
    static char *unfinished[] = {(char *)"Never finished that novel"};
    static char *secret[] = {(char *)"Secretly doubts own competence"};

    p.inner_world.contradictions = contradictions;
    p.inner_world.contradictions_count = 1;
    p.inner_world.embodied_memories = embodied;
    p.inner_world.embodied_memories_count = 1;
    p.inner_world.emotional_flashpoints = flashpoints;
    p.inner_world.emotional_flashpoints_count = 1;
    p.inner_world.unfinished_business = unfinished;
    p.inner_world.unfinished_business_count = 1;
    p.inner_world.secret_self = secret;
    p.inner_world.secret_self_count = 1;
    return p;
}

/* --- hu_persona_inner_world_for_stage tests --- */

static void inner_world_new_gets_only_embodied(void) {
    hu_persona_t p = make_inner_world_persona();
    hu_inner_world_t iw = hu_persona_inner_world_for_stage(&p, HU_REL_NEW);
    HU_ASSERT_EQ(iw.embodied_memories_count, 1u);
    HU_ASSERT_EQ(iw.contradictions_count, 0u);
    HU_ASSERT_EQ(iw.emotional_flashpoints_count, 0u);
    HU_ASSERT_EQ(iw.unfinished_business_count, 0u);
    HU_ASSERT_EQ(iw.secret_self_count, 0u);
}

static void inner_world_familiar_adds_contradictions(void) {
    hu_persona_t p = make_inner_world_persona();
    hu_inner_world_t iw = hu_persona_inner_world_for_stage(&p, HU_REL_FAMILIAR);
    HU_ASSERT_EQ(iw.embodied_memories_count, 1u);
    HU_ASSERT_EQ(iw.contradictions_count, 1u);
    HU_ASSERT_EQ(iw.emotional_flashpoints_count, 0u);
    HU_ASSERT_EQ(iw.unfinished_business_count, 0u);
    HU_ASSERT_EQ(iw.secret_self_count, 0u);
}

static void inner_world_trusted_adds_flashpoints_and_unfinished(void) {
    hu_persona_t p = make_inner_world_persona();
    hu_inner_world_t iw = hu_persona_inner_world_for_stage(&p, HU_REL_TRUSTED);
    HU_ASSERT_EQ(iw.embodied_memories_count, 1u);
    HU_ASSERT_EQ(iw.contradictions_count, 1u);
    HU_ASSERT_EQ(iw.emotional_flashpoints_count, 1u);
    HU_ASSERT_EQ(iw.unfinished_business_count, 1u);
    HU_ASSERT_EQ(iw.secret_self_count, 0u);
}

static void inner_world_deep_gets_everything(void) {
    hu_persona_t p = make_inner_world_persona();
    hu_inner_world_t iw = hu_persona_inner_world_for_stage(&p, HU_REL_DEEP);
    HU_ASSERT_EQ(iw.embodied_memories_count, 1u);
    HU_ASSERT_EQ(iw.contradictions_count, 1u);
    HU_ASSERT_EQ(iw.emotional_flashpoints_count, 1u);
    HU_ASSERT_EQ(iw.unfinished_business_count, 1u);
    HU_ASSERT_EQ(iw.secret_self_count, 1u);
}

static void inner_world_null_persona_returns_empty(void) {
    hu_inner_world_t iw = hu_persona_inner_world_for_stage(NULL, HU_REL_DEEP);
    HU_ASSERT_EQ(iw.embodied_memories_count, 0u);
    HU_ASSERT_EQ(iw.contradictions_count, 0u);
    HU_ASSERT_EQ(iw.emotional_flashpoints_count, 0u);
    HU_ASSERT_EQ(iw.unfinished_business_count, 0u);
    HU_ASSERT_EQ(iw.secret_self_count, 0u);
}

/* --- hu_persona_build_inner_world_graduated tests --- */

static void inner_world_graduated_new_has_embodied(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_persona_t p = make_inner_world_persona();
    size_t out_len = 0;
    char *ctx = hu_persona_build_inner_world_graduated(&alloc, &p, HU_REL_NEW, &out_len);
    HU_ASSERT_NOT_NULL(ctx);
    HU_ASSERT_TRUE(out_len > 0);
    HU_ASSERT_TRUE(strstr(ctx, "Sense memory") != NULL);
    /* Should NOT have contradictions at NEW */
    HU_ASSERT_TRUE(strstr(ctx, "Contradiction") == NULL);
    HU_ASSERT_TRUE(strstr(ctx, "flashpoint") == NULL);
    HU_ASSERT_TRUE(strstr(ctx, "unresolved") == NULL);
    HU_ASSERT_TRUE(strstr(ctx, "Private truth") == NULL);
    alloc.free(alloc.ctx, ctx, 4096);
}

static void inner_world_graduated_familiar_has_contradictions(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_persona_t p = make_inner_world_persona();
    size_t out_len = 0;
    char *ctx = hu_persona_build_inner_world_graduated(&alloc, &p, HU_REL_FAMILIAR, &out_len);
    HU_ASSERT_NOT_NULL(ctx);
    HU_ASSERT_TRUE(strstr(ctx, "Sense memory") != NULL);
    HU_ASSERT_TRUE(strstr(ctx, "Contradiction") != NULL);
    HU_ASSERT_TRUE(strstr(ctx, "flashpoint") == NULL);
    alloc.free(alloc.ctx, ctx, 4096);
}

static void inner_world_graduated_trusted_has_flashpoints(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_persona_t p = make_inner_world_persona();
    size_t out_len = 0;
    char *ctx = hu_persona_build_inner_world_graduated(&alloc, &p, HU_REL_TRUSTED, &out_len);
    HU_ASSERT_NOT_NULL(ctx);
    HU_ASSERT_TRUE(strstr(ctx, "Emotional flashpoint") != NULL);
    HU_ASSERT_TRUE(strstr(ctx, "unresolved") != NULL);
    HU_ASSERT_TRUE(strstr(ctx, "Private truth") == NULL);
    alloc.free(alloc.ctx, ctx, 4096);
}

static void inner_world_graduated_deep_has_secret_self(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_persona_t p = make_inner_world_persona();
    size_t out_len = 0;
    char *ctx = hu_persona_build_inner_world_graduated(&alloc, &p, HU_REL_DEEP, &out_len);
    HU_ASSERT_NOT_NULL(ctx);
    HU_ASSERT_TRUE(strstr(ctx, "Private truth") != NULL);
    HU_ASSERT_TRUE(strstr(ctx, "Sense memory") != NULL);
    HU_ASSERT_TRUE(strstr(ctx, "Contradiction") != NULL);
    HU_ASSERT_TRUE(strstr(ctx, "Emotional flashpoint") != NULL);
    HU_ASSERT_TRUE(strstr(ctx, "unresolved") != NULL);
    alloc.free(alloc.ctx, ctx, 4096);
}

static void inner_world_graduated_empty_persona_returns_null(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_persona_t p = {0};
    size_t out_len = 0;
    char *ctx = hu_persona_build_inner_world_graduated(&alloc, &p, HU_REL_DEEP, &out_len);
    HU_ASSERT_NULL(ctx);
    HU_ASSERT_EQ(out_len, 0u);
}

static void inner_world_graduated_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_persona_t p = make_inner_world_persona();
    size_t out_len = 99;
    char *ctx = hu_persona_build_inner_world_graduated(NULL, &p, HU_REL_DEEP, &out_len);
    HU_ASSERT_NULL(ctx);
    ctx = hu_persona_build_inner_world_graduated(&alloc, NULL, HU_REL_DEEP, &out_len);
    HU_ASSERT_NULL(ctx);
    ctx = hu_persona_build_inner_world_graduated(&alloc, &p, HU_REL_DEEP, NULL);
    HU_ASSERT_NULL(ctx);
}

static void inner_world_acquaintance_returns_empty(void) {
    /* Empty persona at lowest stage (NEW) should return all counts 0 */
    hu_persona_t p = {0};
    hu_inner_world_t iw = hu_persona_inner_world_for_stage(&p, HU_REL_NEW);
    HU_ASSERT_EQ(iw.embodied_memories_count, 0u);
    HU_ASSERT_EQ(iw.contradictions_count, 0u);
    HU_ASSERT_EQ(iw.emotional_flashpoints_count, 0u);
    HU_ASSERT_EQ(iw.unfinished_business_count, 0u);
    HU_ASSERT_EQ(iw.secret_self_count, 0u);
}

void run_inner_world_tests(void) {
    HU_TEST_SUITE("inner_world");
    /* Filtered view by stage */
    HU_RUN_TEST(inner_world_new_gets_only_embodied);
    HU_RUN_TEST(inner_world_familiar_adds_contradictions);
    HU_RUN_TEST(inner_world_trusted_adds_flashpoints_and_unfinished);
    HU_RUN_TEST(inner_world_deep_gets_everything);
    HU_RUN_TEST(inner_world_null_persona_returns_empty);
    HU_RUN_TEST(inner_world_acquaintance_returns_empty);
    /* Graduated context builder */
    HU_RUN_TEST(inner_world_graduated_new_has_embodied);
    HU_RUN_TEST(inner_world_graduated_familiar_has_contradictions);
    HU_RUN_TEST(inner_world_graduated_trusted_has_flashpoints);
    HU_RUN_TEST(inner_world_graduated_deep_has_secret_self);
    HU_RUN_TEST(inner_world_graduated_empty_persona_returns_null);
    HU_RUN_TEST(inner_world_graduated_null_args);
}
