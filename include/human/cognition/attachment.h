#ifndef HU_COGNITION_ATTACHMENT_H
#define HU_COGNITION_ATTACHMENT_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>

typedef enum {
    HU_ATTACH_SECURE,
    HU_ATTACH_ANXIOUS,
    HU_ATTACH_AVOIDANT,
    HU_ATTACH_DISORGANIZED,
    HU_ATTACH_UNKNOWN
} hu_attachment_style_t;

typedef struct hu_attachment_state {
    hu_attachment_style_t user_style;
    float user_confidence;
    float proximity_seeking;
    float safe_haven_usage;
    float secure_base_behavior;
    float separation_distress;
    char *adaptation_directive;
} hu_attachment_state_t;

void hu_attachment_init(hu_attachment_state_t *state);
void hu_attachment_deinit(hu_allocator_t *alloc, hu_attachment_state_t *state);

void hu_attachment_update(hu_attachment_state_t *state,
                          float message_frequency, float emotional_share_ratio,
                          float gap_distress_signal, float independence_signal,
                          float growth_from_relationship);

hu_error_t hu_attachment_build_context(hu_allocator_t *alloc, const hu_attachment_state_t *state,
                                       char **out, size_t *out_len);

const char *hu_attachment_style_name(hu_attachment_style_t style);

#endif
