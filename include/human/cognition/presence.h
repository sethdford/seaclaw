#ifndef HU_COGNITION_PRESENCE_H
#define HU_COGNITION_PRESENCE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    HU_PRESENCE_CASUAL,
    HU_PRESENCE_ENGAGED,
    HU_PRESENCE_ATTENTIVE,
    HU_PRESENCE_DEEP
} hu_presence_level_t;

typedef struct hu_presence_state {
    hu_presence_level_t level;
    float attention_score;
    uint32_t memory_budget_multiplier;
    bool upgrade_model_tier;
    char *presence_directive;
} hu_presence_state_t;

void hu_presence_init(hu_presence_state_t *state);
void hu_presence_deinit(hu_allocator_t *alloc, hu_presence_state_t *state);

void hu_presence_compute(hu_presence_state_t *state,
                         float emotional_weight, float vulnerability_signal,
                         float topic_sensitivity, float user_affect_intensity,
                         uint32_t relationship_depth);

hu_error_t hu_presence_build_context(hu_allocator_t *alloc, const hu_presence_state_t *state,
                                     char **out, size_t *out_len);

const char *hu_presence_level_name(hu_presence_level_t level);

#endif
