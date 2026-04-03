#ifndef HU_INNER_THOUGHTS_H
#define HU_INNER_THOUGHTS_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HU_INNER_THOUGHT_MAX_SURFACE 3
#define HU_INNER_THOUGHT_STALE_DAYS  14

typedef struct hu_inner_thought {
    char *contact_id;
    size_t contact_id_len;
    char *topic;
    size_t topic_len;
    char *thought_text;
    size_t thought_text_len;
    double relevance_score;  /* 0.0–1.0 */
    uint64_t accumulated_at; /* ms since epoch */
    bool surfaced;
} hu_inner_thought_t;

/**
 * Thread safety: NOT THREAD SAFE. Caller must ensure single-threaded access.
 *   Mutating functions (accumulate, surface, deinit) assert non-reentrancy
 *   in debug builds.
 */
typedef struct hu_inner_thought_store {
    hu_inner_thought_t *items;
    size_t count;
    size_t capacity;
    hu_allocator_t *alloc;
} hu_inner_thought_store_t;

/* Init / deinit */
hu_error_t hu_inner_thought_store_init(hu_inner_thought_store_t *store, hu_allocator_t *alloc);
void hu_inner_thought_store_deinit(hu_inner_thought_store_t *store);

/* Accumulate a thought between conversations */
hu_error_t hu_inner_thought_accumulate(hu_inner_thought_store_t *store, const char *contact_id,
                                       size_t contact_id_len, const char *topic, size_t topic_len,
                                       const char *text, size_t text_len, double relevance,
                                       uint64_t now_ms);

/* Check whether a thought should surface given current time and context topic */
bool hu_inner_thought_should_surface(const hu_inner_thought_t *thought, const char *context_topic,
                                     size_t context_topic_len, uint64_t now_ms);

/* Surface best thoughts for a contact — returns count of surfaced thoughts (up to max_count).
 * Writes pointers into surfaced[] (caller provides array). Marks thoughts as surfaced. */
size_t hu_inner_thought_surface(hu_inner_thought_store_t *store, const char *contact_id,
                                size_t contact_id_len, const char *context_topic,
                                size_t context_topic_len, uint64_t now_ms,
                                hu_inner_thought_t **surfaced, size_t max_count);

/* Count unsurfaced thoughts for a contact */
size_t hu_inner_thought_count_pending(const hu_inner_thought_store_t *store, const char *contact_id,
                                      size_t contact_id_len);

#endif /* HU_INNER_THOUGHTS_H */
