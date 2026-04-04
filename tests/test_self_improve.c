#include "test_framework.h"
#include "human/agent/self_improve.h"
#include "human/core/allocator.h"
#include <string.h>

/* ── Fidelity score ──────────────────────────────────────────────── */

static void fidelity_composite_weighted(void) {
    hu_fidelity_score_t score = {
        .personality_consistency = 0.8f,
        .vulnerability_willingness = 0.6f,
        .humor_naturalness = 0.7f,
        .opinion_having = 0.9f,
        .energy_matching = 0.5f,
        .genuine_warmth = 0.8f,
    };
    float c = hu_fidelity_composite(&score);
    HU_ASSERT(c > 0.5f && c < 1.0f);
}

static void fidelity_composite_null_zero(void) {
    HU_ASSERT(hu_fidelity_composite(NULL) == 0.0f);
}

/* ── Initialization ──────────────────────────────────────────────── */

static void init_with_defaults(void) {
    hu_self_improve_state_t state;
    hu_self_improve_init(&state, NULL);
    HU_ASSERT(state.config.max_experiments == 10);
    HU_ASSERT(state.experiments_run == 0);
    HU_ASSERT(!state.running);
}

static void init_with_config(void) {
    hu_self_improve_state_t state;
    hu_self_improve_config_t cfg = {
        .max_experiments = 50,
        .max_seconds = 600,
        .min_improvement = 0.02f,
        .dry_run = true,
    };
    hu_self_improve_init(&state, &cfg);
    HU_ASSERT(state.config.max_experiments == 50);
    HU_ASSERT(state.config.dry_run);
}

static void init_null_safe(void) {
    hu_self_improve_init(NULL, NULL);
}

/* ── Baseline ────────────────────────────────────────────────────── */

static void set_baseline(void) {
    hu_self_improve_state_t state;
    hu_self_improve_init(&state, NULL);
    hu_fidelity_score_t baseline = {
        .personality_consistency = 0.7f,
        .genuine_warmth = 0.8f,
    };
    hu_self_improve_set_baseline(&state, &baseline);
    HU_ASSERT(state.current_baseline.personality_consistency == 0.7f);
    HU_ASSERT(state.current_baseline.composite > 0.0f);
}

/* ── Mutation proposal ───────────────────────────────────────────── */

static void propose_mutation(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_self_improve_state_t state;
    hu_self_improve_init(&state, NULL);
    char *mutation = NULL;
    size_t mutation_len = 0;
    HU_ASSERT(hu_self_improve_propose(&alloc, &state, &mutation, &mutation_len) == HU_OK);
    HU_ASSERT(mutation != NULL);
    HU_ASSERT(mutation_len > 0);
    alloc.free(alloc.ctx, mutation, mutation_len + 1);
}

static void propose_null_fails(void) {
    char *m = NULL;
    size_t ml = 0;
    HU_ASSERT(hu_self_improve_propose(NULL, NULL, &m, &ml) == HU_ERR_INVALID_ARGUMENT);
}

/* ── Record experiment ───────────────────────────────────────────── */

static void record_improvement_kept(void) {
    hu_self_improve_state_t state;
    hu_self_improve_init(&state, NULL);
    hu_fidelity_score_t baseline = {.personality_consistency = 0.5f, .genuine_warmth = 0.5f};
    hu_self_improve_set_baseline(&state, &baseline);
    hu_fidelity_score_t after = {.personality_consistency = 0.7f, .genuine_warmth = 0.7f};
    bool kept = hu_self_improve_record(&state, "be more casual", 14, &after);
    HU_ASSERT(kept);
    HU_ASSERT(state.experiments_run == 1);
    HU_ASSERT(state.experiments_kept == 1);
}

static void record_regression_discarded(void) {
    hu_self_improve_state_t state;
    hu_self_improve_init(&state, NULL);
    hu_fidelity_score_t baseline = {.personality_consistency = 0.8f, .genuine_warmth = 0.8f};
    hu_self_improve_set_baseline(&state, &baseline);
    hu_fidelity_score_t after = {.personality_consistency = 0.3f, .genuine_warmth = 0.3f};
    bool kept = hu_self_improve_record(&state, "bad idea", 8, &after);
    HU_ASSERT(!kept);
    HU_ASSERT(state.experiments_run == 1);
    HU_ASSERT(state.experiments_kept == 0);
}

static void record_null_safe(void) {
    HU_ASSERT(!hu_self_improve_record(NULL, NULL, 0, NULL));
}

/* ── Budget ──────────────────────────────────────────────────────── */

static void budget_exhausted_after_max(void) {
    hu_self_improve_state_t state;
    hu_self_improve_config_t cfg = {.max_experiments = 3, .min_improvement = 0.01f};
    hu_self_improve_init(&state, &cfg);
    hu_fidelity_score_t base = {.genuine_warmth = 0.5f};
    hu_self_improve_set_baseline(&state, &base);
    HU_ASSERT(!hu_self_improve_budget_exhausted(&state));
    for (int i = 0; i < 3; i++) {
        hu_fidelity_score_t after = {.genuine_warmth = 0.5f + (float)(i + 1) * 0.05f};
        hu_self_improve_record(&state, "try", 3, &after);
    }
    HU_ASSERT(hu_self_improve_budget_exhausted(&state));
}

static void budget_null_exhausted(void) {
    HU_ASSERT(hu_self_improve_budget_exhausted(NULL));
}

/* ── Runner ──────────────────────────────────────────────────────── */

void run_self_improve_tests(void) {
    HU_TEST_SUITE("Self Improvement");

    HU_RUN_TEST(fidelity_composite_weighted);
    HU_RUN_TEST(fidelity_composite_null_zero);

    HU_RUN_TEST(init_with_defaults);
    HU_RUN_TEST(init_with_config);
    HU_RUN_TEST(init_null_safe);

    HU_RUN_TEST(set_baseline);
    HU_RUN_TEST(propose_mutation);
    HU_RUN_TEST(propose_null_fails);

    HU_RUN_TEST(record_improvement_kept);
    HU_RUN_TEST(record_regression_discarded);
    HU_RUN_TEST(record_null_safe);

    HU_RUN_TEST(budget_exhausted_after_max);
    HU_RUN_TEST(budget_null_exhausted);
}
