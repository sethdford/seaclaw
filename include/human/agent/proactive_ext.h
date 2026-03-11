#ifndef HU_AGENT_PROACTIVE_EXT_H
#define HU_AGENT_PROACTIVE_EXT_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* F123 — Reciprocity Throttling */

typedef struct hu_reciprocity_state {
    double initiation_ratio;
    uint32_t unanswered_proactive;
    bool contact_just_reengaged;
} hu_reciprocity_state_t;

double hu_reciprocity_budget_multiplier(const hu_reciprocity_state_t *state);

/* F124 — Busyness Simulation */

typedef struct hu_busyness_state {
    bool calendar_busy;
    bool life_sim_stressed;
    uint32_t seed;
} hu_busyness_state_t;

double hu_busyness_budget_multiplier(const hu_busyness_state_t *state);

/* F129 — "Did I tell you?" Pattern */

typedef enum hu_disclosure_action {
    HU_DISCLOSE_TELL_NATURALLY,
    HU_DISCLOSE_ASK_FIRST,
    HU_DISCLOSE_SKIP
} hu_disclosure_action_t;

hu_disclosure_action_t hu_disclosure_decide(double confidence, uint32_t seed);

hu_error_t hu_disclosure_build_prefix(hu_allocator_t *alloc,
                                       hu_disclosure_action_t action,
                                       const char *topic, size_t topic_len,
                                       char **out, size_t *out_len);

/* F30 — Spontaneous Curiosity */

typedef struct hu_curiosity_topic {
    char *topic;
    size_t topic_len;
    char *prompt;
    size_t prompt_len;
    double relevance;
} hu_curiosity_topic_t;

hu_error_t hu_curiosity_query_sql(const char *contact_id, size_t contact_id_len,
                                   uint32_t max_age_days,
                                   char *buf, size_t cap, size_t *out_len);

double hu_curiosity_score(uint32_t days_since_mentioned, uint32_t times_discussed,
                           bool we_brought_it_up_last);

void hu_curiosity_topic_deinit(hu_allocator_t *alloc, hu_curiosity_topic_t *t);

/* F31 — Callback Opportunities */

typedef struct hu_callback_opportunity {
    char *topic;
    size_t topic_len;
    char *original_context;
    size_t original_context_len;
    uint64_t mentioned_at;
    double callback_score;
} hu_callback_opportunity_t;

hu_error_t hu_callback_query_sql(const char *contact_id, size_t contact_id_len,
                                  uint64_t min_age_ms, uint64_t max_age_ms,
                                  char *buf, size_t cap, size_t *out_len);

double hu_callback_score(uint32_t days_old, double emotional_weight, bool was_important);

hu_error_t hu_proactive_ext_build_prompt(hu_allocator_t *alloc,
                                          const hu_curiosity_topic_t *curiosity,
                                          size_t curiosity_count,
                                          const hu_callback_opportunity_t *callbacks,
                                          size_t callback_count,
                                          char **out, size_t *out_len);

void hu_callback_opportunity_deinit(hu_allocator_t *alloc, hu_callback_opportunity_t *c);

#endif /* HU_AGENT_PROACTIVE_EXT_H */
