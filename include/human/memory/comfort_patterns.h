#ifndef HU_COMFORT_PATTERNS_H
#define HU_COMFORT_PATTERNS_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/memory.h"
#include <stddef.h>

/* Comfort pattern: what response type (distraction, empathy, space, advice)
 * helps when a contact expressed negative emotion. Learned from engagement. */

typedef struct hu_comfort_pattern {
    char contact_id[128];
    char emotion[64];
    char response_type[32];
    float engagement_score;
    int sample_count;
} hu_comfort_pattern_t;

/* Record engagement for a response type. Uses INSERT OR REPLACE with running
 * average: new_score = (old_score * old_count + engagement_score) / (old_count + 1).
 * Increments sample_count. Requires SQLite-backed memory. */
hu_error_t hu_comfort_pattern_record(hu_memory_t *memory, const char *contact_id,
                                     size_t contact_id_len, const char *emotion, size_t emotion_len,
                                     const char *response_type, size_t response_type_len,
                                     float engagement_score);

/* Get preferred response type for contact+emotion. Returns best type by
 * engagement_score where sample_count >= 2. Writes into out_type (up to out_cap),
 * sets *out_len. If none found, *out_len = 0. */
hu_error_t hu_comfort_pattern_get_preferred(hu_allocator_t *alloc, hu_memory_t *memory,
                                            const char *contact_id, size_t contact_id_len,
                                            const char *emotion, size_t emotion_len, char *out_type,
                                            size_t out_cap, size_t *out_len);

#endif /* HU_COMFORT_PATTERNS_H */
