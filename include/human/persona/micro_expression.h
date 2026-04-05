#ifndef HU_PERSONA_MICRO_EXPRESSION_H
#define HU_PERSONA_MICRO_EXPRESSION_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct hu_micro_expression {
    float target_length_factor;
    float punctuation_density;
    float emoji_probability;
    float capitalization_energy;
    float ellipsis_frequency;
    bool use_fragments;
    char *style_directive;
} hu_micro_expression_t;

void hu_micro_expression_init(hu_micro_expression_t *me);
void hu_micro_expression_deinit(hu_allocator_t *alloc, hu_micro_expression_t *me);

void hu_micro_expression_compute(hu_micro_expression_t *me,
                                 float energy, float social_battery,
                                 float emotional_valence, float emotional_intensity,
                                 float relationship_closeness);

hu_error_t hu_micro_expression_build_context(hu_allocator_t *alloc,
                                           const hu_micro_expression_t *me,
                                           char **out, size_t *out_len);

#endif
