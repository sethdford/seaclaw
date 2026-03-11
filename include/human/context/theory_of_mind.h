#ifndef HU_CONTEXT_THEORY_OF_MIND_H
#define HU_CONTEXT_THEORY_OF_MIND_H

#include "human/channel.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/memory.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Per-contact behavioral baseline for Theory of Mind deviation detection. */
typedef struct hu_contact_baseline {
    double avg_message_length;
    double avg_response_time_ms;
    double emoji_frequency;
    double topic_diversity;
    double sentiment_baseline;
    int messages_sampled;
    int64_t updated_at;
} hu_contact_baseline_t;

/* Detected deviations from baseline. */
typedef struct hu_theory_of_mind_deviation {
    bool length_drop;
    bool response_slow;
    bool emoji_drop;
    bool topic_narrowing;
    bool sentiment_shift;
    float severity;
} hu_theory_of_mind_deviation_t;

/* Update baseline from recent messages (only from_me=false). Uses exponential
 * moving average (alpha=0.1). Requires at least 5 contact messages.
 * Returns HU_ERR_NOT_SUPPORTED when HU_ENABLE_SQLITE is off. */
hu_error_t hu_theory_of_mind_update_baseline(hu_memory_t *memory, hu_allocator_t *alloc,
                                             const char *contact_id, size_t contact_id_len,
                                             const hu_channel_history_entry_t *entries,
                                             size_t entry_count);

/* Get baseline for contact. Returns HU_OK if found, HU_ERR_NOT_FOUND if none. */
hu_error_t hu_theory_of_mind_get_baseline(hu_memory_t *memory, hu_allocator_t *alloc,
                                         const char *contact_id, size_t contact_id_len,
                                         hu_contact_baseline_t *out);

/* Detect deviations comparing current window to baseline. */
hu_theory_of_mind_deviation_t hu_theory_of_mind_detect_deviation(
    const hu_contact_baseline_t *baseline, const hu_channel_history_entry_t *entries,
    size_t count);

/* Build "[THEORY OF MIND: ...]" inference string for prompt injection.
 * Returns NULL if severity < 0.3. Caller owns returned string.
 * contact_name: display name; pronoun: "she"/"he"/"they" (NULL = "they"). */
char *hu_theory_of_mind_build_inference(hu_allocator_t *alloc,
                                        const char *contact_name, size_t contact_name_len,
                                        const char *pronoun, size_t pronoun_len,
                                        const hu_theory_of_mind_deviation_t *dev,
                                        size_t *out_len);

#endif /* HU_CONTEXT_THEORY_OF_MIND_H */
