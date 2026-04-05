#include "human/observability/bth_metrics.h"
#include "human/core/string.h"
#include <stdio.h>
#include <string.h>

void hu_bth_metrics_init(hu_bth_metrics_t *m) {
    if (!m)
        return;
    memset(m, 0, sizeof(*m));
}

void hu_bth_metrics_record_hula_tool_turn(hu_bth_metrics_t *m) {
    if (m)
        m->hula_tool_turns++;
}

#define HU_BTH_LOG_FIELD(field) \
    do { \
        if (m->field) \
            fprintf(stderr, "[bth] " #field "=%u\n", m->field); \
    } while (0)

void hu_bth_metrics_log(const hu_bth_metrics_t *m) {
    if (!m)
        return;

    HU_BTH_LOG_FIELD(emotions_surfaced);
    HU_BTH_LOG_FIELD(facts_extracted);
    HU_BTH_LOG_FIELD(commitment_followups);
    HU_BTH_LOG_FIELD(pattern_insights);
    HU_BTH_LOG_FIELD(emotions_promoted);
    HU_BTH_LOG_FIELD(events_extracted);
    HU_BTH_LOG_FIELD(mood_contexts_built);
    HU_BTH_LOG_FIELD(silence_checkins);
    HU_BTH_LOG_FIELD(event_followups);
    HU_BTH_LOG_FIELD(starters_built);
    HU_BTH_LOG_FIELD(typos_applied);
    HU_BTH_LOG_FIELD(corrections_sent);
    HU_BTH_LOG_FIELD(thinking_responses);
    HU_BTH_LOG_FIELD(double_texts);
    HU_BTH_LOG_FIELD(callbacks_triggered);
    HU_BTH_LOG_FIELD(reactions_sent);
    HU_BTH_LOG_FIELD(link_contexts);
    HU_BTH_LOG_FIELD(attachment_contexts);
    HU_BTH_LOG_FIELD(ab_evaluations);
    HU_BTH_LOG_FIELD(ab_alternates_chosen);
    HU_BTH_LOG_FIELD(replay_analyses);
    HU_BTH_LOG_FIELD(egraph_contexts);
    HU_BTH_LOG_FIELD(vision_descriptions);
    HU_BTH_LOG_FIELD(skills_applied);
    HU_BTH_LOG_FIELD(skills_evolved);
    HU_BTH_LOG_FIELD(skills_retired);
    HU_BTH_LOG_FIELD(reflections_daily);
    HU_BTH_LOG_FIELD(reflections_weekly);
    HU_BTH_LOG_FIELD(total_turns);
    HU_BTH_LOG_FIELD(cognition_fast_turns);
    HU_BTH_LOG_FIELD(cognition_slow_turns);
    HU_BTH_LOG_FIELD(cognition_emotional_turns);
    HU_BTH_LOG_FIELD(metacog_interventions);
    HU_BTH_LOG_FIELD(metacog_regens);
    HU_BTH_LOG_FIELD(metacog_difficulty_easy);
    HU_BTH_LOG_FIELD(metacog_difficulty_medium);
    HU_BTH_LOG_FIELD(metacog_difficulty_hard);
    HU_BTH_LOG_FIELD(metacog_hysteresis_suppressed);
    HU_BTH_LOG_FIELD(hula_tool_turns);
    HU_BTH_LOG_FIELD(episodic_patterns_stored);
    HU_BTH_LOG_FIELD(episodic_replays);
    HU_BTH_LOG_FIELD(skill_routes_semantic);
    HU_BTH_LOG_FIELD(skill_routes_blended);
    HU_BTH_LOG_FIELD(skill_routes_embedded);
    HU_BTH_LOG_FIELD(evolving_outcomes);
}

#undef HU_BTH_LOG_FIELD

#define HU_BTH_SUMMARY_LINE(field, name) \
    do { \
        pos = hu_buf_appendf(buf, cap, pos, "%s=%u\n", name, m->field); \
    } while (0)

char *hu_bth_metrics_summary(hu_allocator_t *alloc, const hu_bth_metrics_t *m, size_t *out_len) {
    if (!alloc || !alloc->alloc || !m || !out_len)
        return NULL;

    size_t cap = 3072;
    char *buf = (char *)alloc->alloc(alloc->ctx, cap);
    if (!buf)
        return NULL;

    size_t pos = 0;
    HU_BTH_SUMMARY_LINE(emotions_surfaced, "emotions_surfaced");
    HU_BTH_SUMMARY_LINE(facts_extracted, "facts_extracted");
    HU_BTH_SUMMARY_LINE(commitment_followups, "commitment_followups");
    HU_BTH_SUMMARY_LINE(pattern_insights, "pattern_insights");
    HU_BTH_SUMMARY_LINE(emotions_promoted, "emotions_promoted");
    HU_BTH_SUMMARY_LINE(events_extracted, "events_extracted");
    HU_BTH_SUMMARY_LINE(mood_contexts_built, "mood_contexts_built");
    HU_BTH_SUMMARY_LINE(silence_checkins, "silence_checkins");
    HU_BTH_SUMMARY_LINE(event_followups, "event_followups");
    HU_BTH_SUMMARY_LINE(starters_built, "starters_built");
    HU_BTH_SUMMARY_LINE(typos_applied, "typos_applied");
    HU_BTH_SUMMARY_LINE(corrections_sent, "corrections_sent");
    HU_BTH_SUMMARY_LINE(thinking_responses, "thinking_responses");
    HU_BTH_SUMMARY_LINE(double_texts, "double_texts");
    HU_BTH_SUMMARY_LINE(callbacks_triggered, "callbacks_triggered");
    HU_BTH_SUMMARY_LINE(reactions_sent, "reactions_sent");
    HU_BTH_SUMMARY_LINE(link_contexts, "link_contexts");
    HU_BTH_SUMMARY_LINE(attachment_contexts, "attachment_contexts");
    HU_BTH_SUMMARY_LINE(ab_evaluations, "ab_evaluations");
    HU_BTH_SUMMARY_LINE(ab_alternates_chosen, "ab_alternates_chosen");
    HU_BTH_SUMMARY_LINE(replay_analyses, "replay_analyses");
    HU_BTH_SUMMARY_LINE(egraph_contexts, "egraph_contexts");
    HU_BTH_SUMMARY_LINE(vision_descriptions, "vision_descriptions");
    HU_BTH_SUMMARY_LINE(skills_applied, "skills_applied");
    HU_BTH_SUMMARY_LINE(skills_evolved, "skills_evolved");
    HU_BTH_SUMMARY_LINE(skills_retired, "skills_retired");
    HU_BTH_SUMMARY_LINE(reflections_daily, "reflections_daily");
    HU_BTH_SUMMARY_LINE(reflections_weekly, "reflections_weekly");
    HU_BTH_SUMMARY_LINE(total_turns, "total_turns");
    HU_BTH_SUMMARY_LINE(cognition_fast_turns, "cognition_fast_turns");
    HU_BTH_SUMMARY_LINE(cognition_slow_turns, "cognition_slow_turns");
    HU_BTH_SUMMARY_LINE(cognition_emotional_turns, "cognition_emotional_turns");
    HU_BTH_SUMMARY_LINE(metacog_interventions, "metacog_interventions");
    HU_BTH_SUMMARY_LINE(metacog_regens, "metacog_regens");
    HU_BTH_SUMMARY_LINE(metacog_difficulty_easy, "metacog_difficulty_easy");
    HU_BTH_SUMMARY_LINE(metacog_difficulty_medium, "metacog_difficulty_medium");
    HU_BTH_SUMMARY_LINE(metacog_difficulty_hard, "metacog_difficulty_hard");
    HU_BTH_SUMMARY_LINE(metacog_hysteresis_suppressed, "metacog_hysteresis_suppressed");
    HU_BTH_SUMMARY_LINE(hula_tool_turns, "hula_tool_turns");
    HU_BTH_SUMMARY_LINE(episodic_patterns_stored, "episodic_patterns_stored");
    HU_BTH_SUMMARY_LINE(episodic_replays, "episodic_replays");
    HU_BTH_SUMMARY_LINE(skill_routes_semantic, "skill_routes_semantic");
    HU_BTH_SUMMARY_LINE(skill_routes_blended, "skill_routes_blended");
    HU_BTH_SUMMARY_LINE(skill_routes_embedded, "skill_routes_embedded");
    HU_BTH_SUMMARY_LINE(evolving_outcomes, "evolving_outcomes");

    *out_len = cap;
    return buf;
}
