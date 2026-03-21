#include "human/cognition/skill_routing.h"
#include "human/core/allocator.h"
#include "human/skillforge.h"
#include "test_framework.h"
#include <math.h>
#include <string.h>

static hu_allocator_t alloc;

static void setup(void) {
    alloc = hu_system_allocator();
}

static void cosine_identical_returns_one(void) {
    float a[] = {1.0f, 0.0f, 0.0f};
    float b[] = {1.0f, 0.0f, 0.0f};
    float sim = hu_cosine_similarity_f(a, b, 3);
    HU_ASSERT_TRUE(sim > 0.99f);
}

static void cosine_orthogonal_returns_zero(void) {
    float a[] = {1.0f, 0.0f, 0.0f};
    float b[] = {0.0f, 1.0f, 0.0f};
    float sim = hu_cosine_similarity_f(a, b, 3);
    HU_ASSERT_TRUE(fabsf(sim) < 0.01f);
}

static void cosine_null_returns_zero(void) {
    float a[] = {1.0f};
    float sim = hu_cosine_similarity_f(a, NULL, 1);
    HU_ASSERT_TRUE(fabsf(sim) < 0.01f);
}

/* Mock embedder: creates a simple bag-of-chars vector of fixed dims */
#define MOCK_DIMS 26

static hu_error_t mock_embed(void *ctx, hu_allocator_t *a,
                              const char *text, size_t text_len,
                              float **out_vec, size_t *out_dims) {
    (void)ctx;
    float *vec = a->alloc(a->ctx, MOCK_DIMS * sizeof(float));
    if (!vec) return HU_ERR_OUT_OF_MEMORY;
    memset(vec, 0, MOCK_DIMS * sizeof(float));

    for (size_t i = 0; i < text_len; i++) {
        char c = text[i];
        if (c >= 'a' && c <= 'z')
            vec[c - 'a'] += 1.0f;
        else if (c >= 'A' && c <= 'Z')
            vec[c - 'A'] += 1.0f;
    }
    /* Normalize */
    float norm = 0.0f;
    for (size_t i = 0; i < MOCK_DIMS; i++) norm += vec[i] * vec[i];
    if (norm > 0.0f) {
        norm = sqrtf(norm);
        for (size_t i = 0; i < MOCK_DIMS; i++) vec[i] /= norm;
    }

    *out_vec = vec;
    *out_dims = MOCK_DIMS;
    return HU_OK;
}

static void routing_init_and_deinit(void) {
    setup();
    hu_skill_routing_ctx_t ctx;
    hu_skill_routing_init(&ctx);
    HU_ASSERT_TRUE(!ctx.initialized);
    hu_skill_routing_deinit(&ctx, &alloc);
}

static void embed_catalog_succeeds(void) {
    setup();
    hu_skill_routing_ctx_t ctx;
    hu_skill_routing_init(&ctx);

    hu_skill_t skills[3] = {
        {.name = "brainstorming", .description = "creative idea generation"},
        {.name = "critical-thinking", .description = "logical analysis and reasoning"},
        {.name = "empathy-mapping", .description = "understanding emotions and feelings"},
    };

    hu_error_t err = hu_skill_routing_embed_catalog(&ctx, &alloc, skills, 3,
                                                     mock_embed, NULL);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(ctx.initialized);
    HU_ASSERT_EQ(ctx.skills_count, (size_t)3);
    HU_ASSERT_EQ(ctx.embed_dims, (size_t)MOCK_DIMS);

    hu_skill_routing_deinit(&ctx, &alloc);
}

static void routing_returns_best_match(void) {
    setup();
    hu_skill_routing_ctx_t ctx;
    hu_skill_routing_init(&ctx);

    hu_skill_t skills[3] = {
        {.name = "brainstorming", .description = "creative idea generation"},
        {.name = "critical-thinking", .description = "logical analysis and reasoning"},
        {.name = "empathy-mapping", .description = "understanding emotions and feelings"},
    };

    hu_skill_routing_embed_catalog(&ctx, &alloc, skills, 3, mock_embed, NULL);

    hu_skill_blend_t blend;
    hu_error_t err = hu_skill_routing_route(
        &ctx, &alloc,
        "I need creative ideas", 21,
        mock_embed, NULL,
        skills, 3,
        NULL, NULL, 0.0f, &blend);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(blend.count >= 1);
    HU_ASSERT_NOT_NULL(blend.routes[0].skill);

    hu_skill_routing_deinit(&ctx, &alloc);
}

static void routing_keyword_fallback_without_embedder(void) {
    setup();

    hu_skill_t skills[2] = {
        {.name = "brainstorming", .description = "creative idea generation"},
        {.name = "critical-thinking", .description = "logical analysis"},
    };

    float keyword_scores[] = {0.8f, 0.3f};

    hu_skill_blend_t blend;
    hu_error_t err = hu_skill_routing_route(
        NULL, &alloc,
        "some query", 10,
        NULL, NULL,
        skills, 2,
        keyword_scores, NULL, 0.0f, &blend);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(blend.count >= 1);
    HU_ASSERT_TRUE(blend.routes[0].keyword_score > blend.routes[1].keyword_score ||
                    blend.count == 1);
}

static void emotional_boost_applied(void) {
    setup();
    hu_skill_routing_ctx_t ctx;
    hu_skill_routing_init(&ctx);

    hu_skill_t skills[2] = {
        {.name = "coding", .description = "writing code"},
        {.name = "empathy-mapping", .description = "understanding emotions and empathy"},
    };

    hu_skill_routing_embed_catalog(&ctx, &alloc, skills, 2, mock_embed, NULL);

    hu_skill_blend_t blend;
    hu_skill_routing_route(
        &ctx, &alloc,
        "test query", 10,
        mock_embed, NULL,
        skills, 2,
        NULL, NULL, 0.8f, &blend);

    /* The empathy skill should get emotional boost */
    bool empathy_has_boost = false;
    for (size_t i = 0; i < blend.count; i++) {
        if (blend.routes[i].skill == &skills[1] && blend.routes[i].emotional_boost > 0.0f)
            empathy_has_boost = true;
    }
    HU_ASSERT_TRUE(empathy_has_boost);

    hu_skill_routing_deinit(&ctx, &alloc);
}

static void build_catalog_formats_output(void) {
    setup();

    hu_skill_t skills[2] = {
        {.name = "brainstorming", .description = "creative idea generation"},
        {.name = "empathy", .description = "emotional understanding"},
    };

    hu_skill_blend_t blend = {
        .routes = {
            {.skill = &skills[0], .combined_score = 0.9f},
            {.skill = &skills[1], .combined_score = 0.5f},
        },
        .count = 2,
    };

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_skill_routing_build_catalog(&alloc, &blend, skills, 2, 10,
                                                     &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "brainstorming") != NULL);
    HU_ASSERT_TRUE(strstr(out, "empathy") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

void run_skill_routing_tests(void) {
    HU_TEST_SUITE("SkillRouting");
    HU_RUN_TEST(cosine_identical_returns_one);
    HU_RUN_TEST(cosine_orthogonal_returns_zero);
    HU_RUN_TEST(cosine_null_returns_zero);
    HU_RUN_TEST(routing_init_and_deinit);
    HU_RUN_TEST(embed_catalog_succeeds);
    HU_RUN_TEST(routing_returns_best_match);
    HU_RUN_TEST(routing_keyword_fallback_without_embedder);
    HU_RUN_TEST(emotional_boost_applied);
    HU_RUN_TEST(build_catalog_formats_output);
}
