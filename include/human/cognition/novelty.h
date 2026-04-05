#ifndef HU_COGNITION_NOVELTY_H
#define HU_COGNITION_NOVELTY_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>
#include <stdint.h>

typedef struct hu_novelty_signal {
    float novelty_score;
    char *novel_element;
    char *surprise_prompt;
} hu_novelty_signal_t;

#define HU_NOVELTY_MAX_SEEN 64

typedef struct hu_novelty_tracker {
    uint32_t turns_since_last_surprise;
    uint32_t cooldown_turns;
    uint32_t seen_hashes[HU_NOVELTY_MAX_SEEN];
    size_t seen_count;
} hu_novelty_tracker_t;

void hu_novelty_tracker_init(hu_novelty_tracker_t *tracker);

hu_error_t hu_novelty_evaluate(hu_allocator_t *alloc, hu_novelty_tracker_t *tracker,
                               const char *message, size_t message_len, const char *const *known_topics,
                               size_t known_count, const char *const *stm_topics, size_t stm_count,
                               hu_novelty_signal_t *out);

void hu_novelty_signal_free(hu_allocator_t *alloc, hu_novelty_signal_t *signal);

#endif
