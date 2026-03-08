#include "seaclaw/core/allocator.h"
#include "seaclaw/observability/bth_metrics.h"
#include "test_framework.h"
#include <string.h>

static void bth_metrics_init_zeros(void) {
    sc_bth_metrics_t m;
    sc_bth_metrics_init(&m);
    SC_ASSERT_EQ(m.emotions_surfaced, 0u);
    SC_ASSERT_EQ(m.facts_extracted, 0u);
    SC_ASSERT_EQ(m.commitment_followups, 0u);
    SC_ASSERT_EQ(m.pattern_insights, 0u);
    SC_ASSERT_EQ(m.emotions_promoted, 0u);
    SC_ASSERT_EQ(m.events_extracted, 0u);
    SC_ASSERT_EQ(m.mood_contexts_built, 0u);
    SC_ASSERT_EQ(m.silence_checkins, 0u);
    SC_ASSERT_EQ(m.event_followups, 0u);
    SC_ASSERT_EQ(m.starters_built, 0u);
    SC_ASSERT_EQ(m.typos_applied, 0u);
    SC_ASSERT_EQ(m.corrections_sent, 0u);
    SC_ASSERT_EQ(m.thinking_responses, 0u);
    SC_ASSERT_EQ(m.callbacks_triggered, 0u);
    SC_ASSERT_EQ(m.reactions_sent, 0u);
    SC_ASSERT_EQ(m.link_contexts, 0u);
    SC_ASSERT_EQ(m.attachment_contexts, 0u);
    SC_ASSERT_EQ(m.ab_evaluations, 0u);
    SC_ASSERT_EQ(m.ab_alternates_chosen, 0u);
    SC_ASSERT_EQ(m.replay_analyses, 0u);
    SC_ASSERT_EQ(m.egraph_contexts, 0u);
    SC_ASSERT_EQ(m.vision_descriptions, 0u);
    SC_ASSERT_EQ(m.total_turns, 0u);
}

static void bth_metrics_summary_with_data(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_bth_metrics_t m;
    sc_bth_metrics_init(&m);
    m.emotions_surfaced = 3;
    m.facts_extracted = 7;
    m.typos_applied = 2;
    m.total_turns = 100;

    size_t len = 0;
    char *summary = sc_bth_metrics_summary(&alloc, &m, &len);
    SC_ASSERT_NOT_NULL(summary);
    SC_ASSERT_TRUE(len > 0);
    SC_ASSERT_TRUE(strstr(summary, "emotions_surfaced=3") != NULL);
    SC_ASSERT_TRUE(strstr(summary, "facts_extracted=7") != NULL);
    SC_ASSERT_TRUE(strstr(summary, "typos_applied=2") != NULL);
    SC_ASSERT_TRUE(strstr(summary, "total_turns=100") != NULL);

    alloc.free(alloc.ctx, summary, len);
}

static void bth_metrics_summary_empty(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_bth_metrics_t m;
    sc_bth_metrics_init(&m);

    size_t len = 0;
    char *summary = sc_bth_metrics_summary(&alloc, &m, &len);
    SC_ASSERT_NOT_NULL(summary);
    SC_ASSERT_TRUE(len > 0);
    SC_ASSERT_TRUE(strstr(summary, "emotions_surfaced=0") != NULL);
    SC_ASSERT_TRUE(strstr(summary, "total_turns=0") != NULL);

    alloc.free(alloc.ctx, summary, len);
}

void run_bth_metrics_tests(void) {
    SC_TEST_SUITE("bth_metrics");
    SC_RUN_TEST(bth_metrics_init_zeros);
    SC_RUN_TEST(bth_metrics_summary_with_data);
    SC_RUN_TEST(bth_metrics_summary_empty);
}
