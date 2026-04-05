#ifndef HU_AGENT_CHOREOGRAPHY_H
#define HU_AGENT_CHOREOGRAPHY_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct hu_message_segment {
    char *text;
    size_t text_len;
    uint32_t delay_ms;
    bool show_typing_indicator;
} hu_message_segment_t;

typedef struct hu_message_plan {
    hu_message_segment_t *segments;
    size_t segment_count;
} hu_message_plan_t;

typedef struct hu_choreography_config {
    float burst_probability;
    float double_text_probability;
    float energy_level;
    uint32_t max_segments;
    uint32_t ms_per_word;
    bool message_splitting_enabled;
} hu_choreography_config_t;

hu_choreography_config_t hu_choreography_config_default(void);

hu_error_t hu_choreography_plan(hu_allocator_t *alloc, const char *response, size_t response_len,
                                const hu_choreography_config_t *config, uint32_t seed,
                                hu_message_plan_t *out);

void hu_choreography_plan_free(hu_allocator_t *alloc, hu_message_plan_t *plan);

#endif
