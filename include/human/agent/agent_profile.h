#ifndef HU_AGENT_AGENT_PROFILE_H
#define HU_AGENT_AGENT_PROFILE_H

#include "human/core/error.h"
#include <stddef.h>

/*
 * Agent Specialization — match agents to tasks by category.
 * success_rates[0..7] map to: coding, reasoning, research, general,
 * ops, messaging, analysis, creative.
 */

#define HU_AGENT_PROFILE_CATEGORIES 8

typedef struct hu_agent_profile {
    char name[128];
    char specialization[128]; /* "coding", "reasoning", "research", "general" */
    double success_rates[HU_AGENT_PROFILE_CATEGORIES];
    int task_count;
} hu_agent_profile_t;

hu_error_t hu_agent_match_score(const hu_agent_profile_t *profile,
                                const char *task_category, size_t cat_len,
                                double *score);

#endif /* HU_AGENT_AGENT_PROFILE_H */
