#ifndef HU_AGENT_SCRATCHPAD_H
#define HU_AGENT_SCRATCHPAD_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Structured scratchpad: JSON key-value store separate from conversation.
 * Persists working memory across turns without polluting chat history.
 * Used for intermediate results, reasoning state, and task context.
 */

typedef struct hu_scratchpad_entry {
    char key[128];
    char *value; /* heap-allocated */
    size_t value_len;
    int64_t created_at;
    int64_t updated_at;
    uint32_t access_count;
} hu_scratchpad_entry_t;

#define HU_SCRATCHPAD_MAX_ENTRIES 256

typedef struct hu_scratchpad {
    hu_scratchpad_entry_t entries[HU_SCRATCHPAD_MAX_ENTRIES];
    size_t entry_count;
    size_t total_bytes; /* total value bytes stored */
    size_t max_bytes;   /* 0 = unlimited */
} hu_scratchpad_t;

void hu_scratchpad_init(hu_scratchpad_t *sp, size_t max_bytes);

hu_error_t hu_scratchpad_set(hu_scratchpad_t *sp, hu_allocator_t *alloc, const char *key,
                             size_t key_len, const char *value, size_t value_len);

hu_error_t hu_scratchpad_get(const hu_scratchpad_t *sp, const char *key, size_t key_len,
                             const char **value_out, size_t *value_len_out);

hu_error_t hu_scratchpad_delete(hu_scratchpad_t *sp, hu_allocator_t *alloc, const char *key,
                                size_t key_len);

bool hu_scratchpad_has(const hu_scratchpad_t *sp, const char *key, size_t key_len);

size_t hu_scratchpad_to_json(const hu_scratchpad_t *sp, char *buf, size_t buf_size);

void hu_scratchpad_clear(hu_scratchpad_t *sp, hu_allocator_t *alloc);

void hu_scratchpad_deinit(hu_scratchpad_t *sp, hu_allocator_t *alloc);

#endif /* HU_AGENT_SCRATCHPAD_H */
