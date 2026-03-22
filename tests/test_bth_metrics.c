#include "human/core/allocator.h"
#include "human/observability/bth_metrics.h"
#include "test_framework.h"
#include <stdint.h>
#include <string.h>

static void bth_metrics_init_zeros(void) {
    hu_bth_metrics_t m;
    hu_bth_metrics_init(&m);
    HU_ASSERT_EQ(m.emotions_surfaced, 0u);
    HU_ASSERT_EQ(m.facts_extracted, 0u);
    HU_ASSERT_EQ(m.commitment_followups, 0u);
    HU_ASSERT_EQ(m.pattern_insights, 0u);
    HU_ASSERT_EQ(m.emotions_promoted, 0u);
    HU_ASSERT_EQ(m.events_extracted, 0u);
    HU_ASSERT_EQ(m.mood_contexts_built, 0u);
    HU_ASSERT_EQ(m.silence_checkins, 0u);
    HU_ASSERT_EQ(m.event_followups, 0u);
    HU_ASSERT_EQ(m.starters_built, 0u);
    HU_ASSERT_EQ(m.typos_applied, 0u);
    HU_ASSERT_EQ(m.corrections_sent, 0u);
    HU_ASSERT_EQ(m.thinking_responses, 0u);
    HU_ASSERT_EQ(m.double_texts, 0u);
    HU_ASSERT_EQ(m.callbacks_triggered, 0u);
    HU_ASSERT_EQ(m.reactions_sent, 0u);
    HU_ASSERT_EQ(m.link_contexts, 0u);
    HU_ASSERT_EQ(m.attachment_contexts, 0u);
    HU_ASSERT_EQ(m.ab_evaluations, 0u);
    HU_ASSERT_EQ(m.ab_alternates_chosen, 0u);
    HU_ASSERT_EQ(m.replay_analyses, 0u);
    HU_ASSERT_EQ(m.egraph_contexts, 0u);
    HU_ASSERT_EQ(m.vision_descriptions, 0u);
    HU_ASSERT_EQ(m.skills_applied, 0u);
    HU_ASSERT_EQ(m.skills_evolved, 0u);
    HU_ASSERT_EQ(m.skills_retired, 0u);
    HU_ASSERT_EQ(m.reflections_daily, 0u);
    HU_ASSERT_EQ(m.reflections_weekly, 0u);
    HU_ASSERT_EQ(m.total_turns, 0u);
    HU_ASSERT_EQ(m.cognition_fast_turns, 0u);
    HU_ASSERT_EQ(m.metacog_regens, 0u);
    HU_ASSERT_EQ(m.hula_tool_turns, 0u);
    HU_ASSERT_EQ(m.metacog_difficulty_hard, 0u);
    HU_ASSERT_EQ(m.skill_routes_semantic, 0u);
    HU_ASSERT_EQ(m.evolving_outcomes, 0u);
}

static void bth_metrics_summary_with_data(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_bth_metrics_t m;
    hu_bth_metrics_init(&m);
    m.emotions_surfaced = 3;
    m.facts_extracted = 7;
    m.typos_applied = 2;
    m.total_turns = 100;

    size_t len = 0;
    char *summary = hu_bth_metrics_summary(&alloc, &m, &len);
    HU_ASSERT_NOT_NULL(summary);
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_TRUE(strstr(summary, "emotions_surfaced=3") != NULL);
    HU_ASSERT_TRUE(strstr(summary, "facts_extracted=7") != NULL);
    HU_ASSERT_TRUE(strstr(summary, "typos_applied=2") != NULL);
    HU_ASSERT_TRUE(strstr(summary, "total_turns=100") != NULL);

    alloc.free(alloc.ctx, summary, len);
}

static void bth_metrics_summary_empty(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_bth_metrics_t m;
    hu_bth_metrics_init(&m);

    size_t len = 0;
    char *summary = hu_bth_metrics_summary(&alloc, &m, &len);
    HU_ASSERT_NOT_NULL(summary);
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_TRUE(strstr(summary, "emotions_surfaced=0") != NULL);
    HU_ASSERT_TRUE(strstr(summary, "total_turns=0") != NULL);

    alloc.free(alloc.ctx, summary, len);
}

static void bth_metrics_all_counters_addressable(void) {
    hu_bth_metrics_t m;
    hu_bth_metrics_init(&m);

    m.emotions_surfaced = 1;
    m.facts_extracted = 1;
    m.commitment_followups = 1;
    m.pattern_insights = 1;
    m.emotions_promoted = 1;
    m.events_extracted = 1;
    m.mood_contexts_built = 1;
    m.silence_checkins = 1;
    m.event_followups = 1;
    m.starters_built = 1;
    m.typos_applied = 1;
    m.corrections_sent = 1;
    m.thinking_responses = 1;
    m.double_texts = 1;
    m.callbacks_triggered = 1;
    m.reactions_sent = 1;
    m.link_contexts = 1;
    m.attachment_contexts = 1;
    m.ab_evaluations = 1;
    m.ab_alternates_chosen = 1;
    m.replay_analyses = 1;
    m.egraph_contexts = 1;
    m.vision_descriptions = 1;
    m.skills_applied = 1;
    m.skills_evolved = 1;
    m.skills_retired = 1;
    m.reflections_daily = 1;
    m.reflections_weekly = 1;
    m.total_turns = 1;
    m.cognition_fast_turns = 1;
    m.cognition_slow_turns = 1;
    m.cognition_emotional_turns = 1;
    m.metacog_interventions = 1;
    m.metacog_regens = 1;
    m.metacog_difficulty_easy = 1;
    m.metacog_difficulty_medium = 1;
    m.metacog_difficulty_hard = 1;
    m.metacog_hysteresis_suppressed = 1;
    m.hula_tool_turns = 1;
    m.episodic_patterns_stored = 1;
    m.episodic_replays = 1;
    m.skill_routes_semantic = 1;
    m.skill_routes_blended = 1;
    m.skill_routes_embedded = 1;
    m.evolving_outcomes = 1;

    hu_allocator_t alloc = hu_system_allocator();
    size_t out_len = 0;
    char *summary = hu_bth_metrics_summary(&alloc, &m, &out_len);
    HU_ASSERT_NOT_NULL(summary);
    HU_ASSERT_TRUE(strstr(summary, "metacog_regens=1") != NULL);
    HU_ASSERT_TRUE(strstr(summary, "hula_tool_turns=1") != NULL);
    HU_ASSERT_TRUE(strstr(summary, "cognition_fast_turns=1") != NULL);
    HU_ASSERT_TRUE(strstr(summary, "skill_routes_semantic=1") != NULL);
    alloc.free(alloc.ctx, summary, out_len);
}

static void bth_metrics_log_does_not_crash_with_zeros(void) {
    hu_bth_metrics_t m;
    hu_bth_metrics_init(&m);
    hu_bth_metrics_log(&m);
}

static void bth_metrics_record_hula_hook(void) {
    hu_bth_metrics_t m;
    hu_bth_metrics_init(&m);
    hu_bth_metrics_record_hula_tool_turn(NULL);
    HU_ASSERT_EQ(m.hula_tool_turns, 0u);
    hu_bth_metrics_record_hula_tool_turn(&m);
    HU_ASSERT_EQ(m.hula_tool_turns, 1u);
    hu_bth_metrics_record_hula_tool_turn(&m);
    HU_ASSERT_EQ(m.hula_tool_turns, 2u);
}

static void bth_metrics_summary_contains_counter_names(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_bth_metrics_t m;
    hu_bth_metrics_init(&m);
    m.commitment_followups = 5;
    m.pattern_insights = 3;
    m.reactions_sent = 7;

    size_t len = 0;
    char *summary = hu_bth_metrics_summary(&alloc, &m, &len);
    HU_ASSERT_NOT_NULL(summary);
    HU_ASSERT_TRUE(strstr(summary, "commitment_followups=5") != NULL);
    HU_ASSERT_TRUE(strstr(summary, "pattern_insights=3") != NULL);
    HU_ASSERT_TRUE(strstr(summary, "reactions_sent=7") != NULL);
    alloc.free(alloc.ctx, summary, len);
}

static void bth_metrics_summary_null_alloc(void) {
    hu_bth_metrics_t m;
    hu_bth_metrics_init(&m);
    size_t len = 0;
    char *summary = hu_bth_metrics_summary(NULL, &m, &len);
    HU_ASSERT_NULL(summary);
}

static void bth_metrics_summary_null_metrics(void) {
    hu_allocator_t alloc = hu_system_allocator();
    size_t len = 0;
    char *summary = hu_bth_metrics_summary(&alloc, NULL, &len);
    HU_ASSERT_NULL(summary);
}

void run_bth_metrics_tests(void) {
    HU_TEST_SUITE("bth_metrics");
    HU_RUN_TEST(bth_metrics_init_zeros);
    HU_RUN_TEST(bth_metrics_summary_with_data);
    HU_RUN_TEST(bth_metrics_summary_empty);
    HU_RUN_TEST(bth_metrics_all_counters_addressable);
    HU_RUN_TEST(bth_metrics_log_does_not_crash_with_zeros);
    HU_RUN_TEST(bth_metrics_record_hula_hook);
    HU_RUN_TEST(bth_metrics_summary_contains_counter_names);
    HU_RUN_TEST(bth_metrics_summary_null_alloc);
    HU_RUN_TEST(bth_metrics_summary_null_metrics);
}
