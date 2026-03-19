#include "human/agent/relationship_dynamics.h"
#include "test_framework.h"
#include <string.h>

static hu_allocator_t test_alloc;

static void *test_alloc_fn(void *ctx, size_t size) {
    (void)ctx;
    return malloc(size);
}

static void test_free_fn(void *ctx, void *ptr, size_t size) {
    (void)ctx;
    (void)size;
    free(ptr);
}

static void setup_alloc(void) {
    test_alloc = (hu_allocator_t){
        .alloc = test_alloc_fn,
        .free = test_free_fn,
        .ctx = NULL,
    };
}

static void reldyn_config_default_values(void) {
    hu_reldyn_config_t cfg = hu_reldyn_config_default();
    HU_ASSERT_FLOAT_EQ(cfg.drift_threshold, -0.1, 0.001);
    HU_ASSERT_FLOAT_EQ(cfg.clear_drift_threshold, -0.3, 0.001);
    HU_ASSERT_EQ(cfg.repair_exit_days, 3);
    HU_ASSERT_FLOAT_EQ(cfg.drift_budget_mult, 0.5, 0.001);
}

static void reldyn_compute_positive_velocity_deepens(void) {
    hu_reldyn_signals_t signals = {
        .frequency_delta = 0.5,
        .initiation_delta = 0.3,
        .response_time_delta = 0.2,
        .msg_length_delta = 0.4,
        .vulnerability_delta = 0.3,
        .plan_completion = 0.8,
        .sentiment_delta = 0.5,
        .topic_diversity = 0.6,
    };
    hu_reldyn_state_t prev = {0};
    prev.closeness = 0.5;
    hu_reldyn_state_t out;

    HU_ASSERT_EQ(hu_reldyn_compute(&signals, &prev, NULL, &out), HU_OK);
    HU_ASSERT(out.velocity > 0.15);
    HU_ASSERT_EQ(out.mode, (int)HU_RELDYN_DEEPENING);
    HU_ASSERT(out.closeness > 0.5);
}

static void reldyn_compute_negative_velocity_drifts(void) {
    hu_reldyn_signals_t signals = {
        .frequency_delta = -0.5,
        .initiation_delta = -0.3,
        .response_time_delta = -0.4,
        .msg_length_delta = -0.3,
        .vulnerability_delta = -0.2,
        .plan_completion = 0.1,
        .sentiment_delta = -0.3,
        .topic_diversity = -0.2,
    };
    hu_reldyn_state_t prev = {0};
    prev.closeness = 0.7;
    hu_reldyn_state_t out;

    HU_ASSERT_EQ(hu_reldyn_compute(&signals, &prev, NULL, &out), HU_OK);
    HU_ASSERT(out.velocity < -0.1);
    HU_ASSERT_EQ(out.mode, (int)HU_RELDYN_DRIFTING);
    HU_ASSERT(out.closeness < 0.7);
}

static void reldyn_compute_closeness_clamped(void) {
    hu_reldyn_signals_t signals = {
        .frequency_delta = 1.0,
        .initiation_delta = 1.0,
        .response_time_delta = 1.0,
        .msg_length_delta = 1.0,
        .vulnerability_delta = 1.0,
        .plan_completion = 1.0,
        .sentiment_delta = 1.0,
        .topic_diversity = 1.0,
    };
    hu_reldyn_state_t prev = {0};
    prev.closeness = 0.99;
    hu_reldyn_state_t out;

    HU_ASSERT_EQ(hu_reldyn_compute(&signals, &prev, NULL, &out), HU_OK);
    HU_ASSERT(out.closeness <= 1.0);
    HU_ASSERT(out.closeness >= 0.0);
}

static void reldyn_compute_null_args_rejected(void) {
    hu_reldyn_state_t out;
    HU_ASSERT_EQ(hu_reldyn_compute(NULL, NULL, NULL, &out),
                  HU_ERR_INVALID_ARGUMENT);
    hu_reldyn_signals_t s = {0};
    HU_ASSERT_EQ(hu_reldyn_compute(&s, NULL, NULL, NULL),
                  HU_ERR_INVALID_ARGUMENT);
}

static void reldyn_detect_drift_early(void) {
    hu_reldyn_state_t state = {.velocity = -0.15};
    bool drifting = false, clear = false;

    HU_ASSERT_EQ(hu_reldyn_detect_drift(&state, NULL, &drifting, &clear), HU_OK);
    HU_ASSERT(drifting);
    HU_ASSERT(!clear);
}

static void reldyn_detect_drift_clear(void) {
    hu_reldyn_state_t state = {.velocity = -0.4};
    bool drifting = false, clear = false;

    HU_ASSERT_EQ(hu_reldyn_detect_drift(&state, NULL, &drifting, &clear), HU_OK);
    HU_ASSERT(drifting);
    HU_ASSERT(clear);
}

static void reldyn_detect_drift_null_args(void) {
    bool d, c;
    HU_ASSERT_EQ(hu_reldyn_detect_drift(NULL, NULL, &d, &c),
                  HU_ERR_INVALID_ARGUMENT);
}

static void reldyn_enter_repair_sets_mode(void) {
    hu_reldyn_state_t state = {.mode = HU_RELDYN_DRIFTING};
    HU_ASSERT_EQ(hu_reldyn_enter_repair(&state, 100000), HU_OK);
    HU_ASSERT_EQ(state.mode, (int)HU_RELDYN_REPAIR);
    HU_ASSERT_EQ(state.mode_entered_ms, 100000);
}

static void reldyn_enter_repair_null_rejected(void) {
    HU_ASSERT_EQ(hu_reldyn_enter_repair(NULL, 0), HU_ERR_INVALID_ARGUMENT);
}

static void reldyn_repair_exit_after_threshold(void) {
    hu_reldyn_state_t state = {
        .mode = HU_RELDYN_REPAIR,
        .mode_entered_ms = 0,
        .velocity = 0.05,
    };
    hu_reldyn_config_t cfg = hu_reldyn_config_default();
    bool should_exit = false;

    /* 3 days in ms = 259200000 */
    int64_t after_3_days = 260000000;
    HU_ASSERT_EQ(hu_reldyn_check_repair_exit(&state, &cfg, after_3_days, &should_exit), HU_OK);
    HU_ASSERT(should_exit);
}

static void reldyn_repair_no_exit_if_negative_velocity(void) {
    hu_reldyn_state_t state = {
        .mode = HU_RELDYN_REPAIR,
        .mode_entered_ms = 0,
        .velocity = -0.1,
    };
    bool should_exit = false;
    int64_t after_3_days = 260000000;
    HU_ASSERT_EQ(hu_reldyn_check_repair_exit(&state, NULL, after_3_days, &should_exit), HU_OK);
    HU_ASSERT(!should_exit);
}

static void reldyn_repair_no_exit_too_soon(void) {
    hu_reldyn_state_t state = {
        .mode = HU_RELDYN_REPAIR,
        .mode_entered_ms = 0,
        .velocity = 0.1,
    };
    bool should_exit = false;
    int64_t after_1_day = 86400000;
    HU_ASSERT_EQ(hu_reldyn_check_repair_exit(&state, NULL, after_1_day, &should_exit), HU_OK);
    HU_ASSERT(!should_exit);
}

static void reldyn_check_repair_exit_null_rejected(void) {
    bool b;
    HU_ASSERT_EQ(hu_reldyn_check_repair_exit(NULL, NULL, 0, &b), HU_ERR_INVALID_ARGUMENT);
}

static void reldyn_compute_preserves_repair_mode(void) {
    hu_reldyn_signals_t signals = {
        .frequency_delta = 0.5,
        .initiation_delta = 0.3,
        .response_time_delta = 0.2,
        .msg_length_delta = 0.4,
        .vulnerability_delta = 0.3,
        .plan_completion = 0.8,
        .sentiment_delta = 0.5,
        .topic_diversity = 0.6,
    };
    hu_reldyn_state_t prev = {.mode = HU_RELDYN_REPAIR, .closeness = 0.5};
    hu_reldyn_state_t out;

    HU_ASSERT_EQ(hu_reldyn_compute(&signals, &prev, NULL, &out), HU_OK);
    HU_ASSERT_EQ(out.mode, (int)HU_RELDYN_REPAIR);
}

static void reldyn_reconnecting_after_drift(void) {
    hu_reldyn_signals_t signals = {
        .frequency_delta = 0.05,
        .initiation_delta = 0.02,
        .response_time_delta = 0.01,
        .msg_length_delta = 0.02,
        .vulnerability_delta = 0.01,
        .plan_completion = 0.1,
        .sentiment_delta = 0.02,
        .topic_diversity = 0.03,
    };
    hu_reldyn_state_t prev = {.mode = HU_RELDYN_DRIFTING, .closeness = 0.4};
    hu_reldyn_state_t out;

    HU_ASSERT_EQ(hu_reldyn_compute(&signals, &prev, NULL, &out), HU_OK);
    HU_ASSERT_EQ(out.mode, (int)HU_RELDYN_RECONNECTING);
}

static void reldyn_build_prompt_basic(void) {
    setup_alloc();
    hu_reldyn_state_t state = {
        .closeness = 0.75,
        .velocity = -0.15,
        .vulnerability_depth = 0.4,
        .reciprocity = -0.3,
        .mode = HU_RELDYN_DRIFTING,
    };
    memcpy(state.contact_id, "alice", 5);
    state.contact_id_len = 5;

    char *prompt = NULL;
    size_t len = 0;
    HU_ASSERT_EQ(hu_reldyn_build_prompt(&test_alloc, &state, &prompt, &len), HU_OK);
    HU_ASSERT(prompt != NULL);
    HU_ASSERT(len > 0);
    HU_ASSERT(strstr(prompt, "alice") != NULL);
    HU_ASSERT(strstr(prompt, "drifting") != NULL);
    HU_ASSERT(strstr(prompt, "cooling") != NULL);
    free(prompt);
}

static void reldyn_build_prompt_null_args(void) {
    setup_alloc();
    hu_reldyn_state_t state = {0};
    char *p;
    size_t l;
    HU_ASSERT_EQ(hu_reldyn_build_prompt(NULL, &state, &p, &l), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_reldyn_build_prompt(&test_alloc, NULL, &p, &l), HU_ERR_INVALID_ARGUMENT);
}

static void reldyn_mode_name_all(void) {
    HU_ASSERT(strcmp(hu_reldyn_mode_name(HU_RELDYN_NORMAL), "normal") == 0);
    HU_ASSERT(strcmp(hu_reldyn_mode_name(HU_RELDYN_DEEPENING), "deepening") == 0);
    HU_ASSERT(strcmp(hu_reldyn_mode_name(HU_RELDYN_DRIFTING), "drifting") == 0);
    HU_ASSERT(strcmp(hu_reldyn_mode_name(HU_RELDYN_REPAIR), "repair") == 0);
    HU_ASSERT(strcmp(hu_reldyn_mode_name(HU_RELDYN_RECONNECTING), "reconnecting") == 0);
}

void run_relationship_dynamics_tests(void) {
    HU_RUN_TEST(reldyn_config_default_values);
    HU_RUN_TEST(reldyn_compute_positive_velocity_deepens);
    HU_RUN_TEST(reldyn_compute_negative_velocity_drifts);
    HU_RUN_TEST(reldyn_compute_closeness_clamped);
    HU_RUN_TEST(reldyn_compute_null_args_rejected);
    HU_RUN_TEST(reldyn_detect_drift_early);
    HU_RUN_TEST(reldyn_detect_drift_clear);
    HU_RUN_TEST(reldyn_detect_drift_null_args);
    HU_RUN_TEST(reldyn_enter_repair_sets_mode);
    HU_RUN_TEST(reldyn_enter_repair_null_rejected);
    HU_RUN_TEST(reldyn_repair_exit_after_threshold);
    HU_RUN_TEST(reldyn_repair_no_exit_if_negative_velocity);
    HU_RUN_TEST(reldyn_repair_no_exit_too_soon);
    HU_RUN_TEST(reldyn_check_repair_exit_null_rejected);
    HU_RUN_TEST(reldyn_compute_preserves_repair_mode);
    HU_RUN_TEST(reldyn_reconnecting_after_drift);
    HU_RUN_TEST(reldyn_build_prompt_basic);
    HU_RUN_TEST(reldyn_build_prompt_null_args);
    HU_RUN_TEST(reldyn_mode_name_all);
}
