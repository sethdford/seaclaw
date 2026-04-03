#ifndef HU_AGENT_IDEMPOTENCY_H
#define HU_AGENT_IDEMPOTENCY_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Idempotency registry for crash-proof tool execution.
 *
 * Deduplicates tool calls by maintaining an in-memory registry of
 * (tool_name, args_json) -> cached_result mappings. If a tool is
 * called with identical parameters, the cached result is replayed
 * instead of re-executing.
 *
 * Key format: "<tool_name>:<16-hex-hash-of-args>"
 * Hashing: FNV-1a hash of args_json to provide deterministic keys
 *          with low collision probability.
 *
 * Storage: In-memory hash map with max 4096 entries and LRU eviction.
 */

#define HU_IDEMPOTENCY_MAX_ENTRIES 4096

typedef struct hu_idempotency_entry {
    char *key;                    /* format: "<tool_name>:<args_hash>" */
    size_t key_len;
    char *result_json;            /* cached tool result (JSON string) */
    size_t result_json_len;
    bool is_error;                /* was the result an error? */
    int64_t created_at;           /* unix timestamp */
    int64_t expires_at;           /* 0 = never; entry removed if expires_at <= now */
} hu_idempotency_entry_t;

typedef struct hu_idempotency_registry hu_idempotency_registry_t;

/* Create a new idempotency registry. */
hu_error_t hu_idempotency_create(hu_allocator_t *alloc, hu_idempotency_registry_t **out);

/* Check if a tool call has been cached. Returns true + cached result if found. */
bool hu_idempotency_check(hu_idempotency_registry_t *reg, const char *tool_name,
                          const char *args_json, hu_idempotency_entry_t *out);

/* Record a tool call result for future dedup. */
hu_error_t hu_idempotency_record(hu_idempotency_registry_t *reg, hu_allocator_t *alloc,
                                 const char *tool_name, const char *args_json,
                                 const char *result_json, bool is_error);

/* Generate idempotency key from tool name + args. Caller frees result. */
hu_error_t hu_idempotency_key_generate(hu_allocator_t *alloc, const char *tool_name,
                                       const char *args_json, char **out_key, size_t *out_key_len);

/* Clear all entries (e.g., on new workflow). */
void hu_idempotency_clear(hu_idempotency_registry_t *reg, hu_allocator_t *alloc);

/* Destroy registry and free all resources. */
void hu_idempotency_destroy(hu_idempotency_registry_t *reg, hu_allocator_t *alloc);

/* Get registry stats for debugging/monitoring. */
typedef struct hu_idempotency_stats {
    size_t entry_count;
    size_t max_entries;
    size_t total_hits;
    size_t total_misses;
} hu_idempotency_stats_t;

void hu_idempotency_stats(const hu_idempotency_registry_t *reg, hu_idempotency_stats_t *out);

#endif /* HU_AGENT_IDEMPOTENCY_H */
