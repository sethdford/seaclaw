#ifndef HU_CONTEXT_STYLE_TRACKER_H
#define HU_CONTEXT_STYLE_TRACKER_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/memory.h"
#include <stdbool.h>
#include <stddef.h>

/* Style fingerprint: per-contact texting style learned from our sent messages.
 * Used to enforce consistency (e.g. "haha" not "lol" if we used "haha" recently). */
typedef struct hu_style_fingerprint {
    bool uses_lowercase;
    bool uses_periods;
    char laugh_style[32];
    int avg_message_length;
    char common_phrases[512];
    char distinctive_words[512];
} hu_style_fingerprint_t;

/* Update fingerprint from a sent message. Merges with existing row. */
hu_error_t hu_style_fingerprint_update(hu_memory_t *memory, hu_allocator_t *alloc,
                                       const char *contact_id, size_t contact_id_len,
                                       const char *message, size_t message_len);

/* Fetch fingerprint for contact. Returns HU_OK and fills out on success. */
hu_error_t hu_style_fingerprint_get(hu_memory_t *memory, hu_allocator_t *alloc,
                                    const char *contact_id, size_t contact_id_len,
                                    hu_style_fingerprint_t *out);

/* Build directive string for LLM injection. Returns bytes written, 0 if empty. */
size_t hu_style_fingerprint_build_directive(const hu_style_fingerprint_t *fp,
                                           char *buf, size_t cap);

#endif /* HU_CONTEXT_STYLE_TRACKER_H */
