#ifndef HU_TOOLS_CACHE_TTL_H
#define HU_TOOLS_CACHE_TTL_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Cross-turn tool result cache with TTL.
 *
 * Extends the per-turn tool cache to persist across turns, with:
 *   - Configurable TTL per tool (e.g., weather: 300s, file_read: 0s)
 *   - Key = hash(tool_name + args_json)
 *   - LRU eviction when capacity exceeded
 *   - Content-addressed dedup for identical results
 */

#define HU_TOOL_CACHE_TTL_MAX_ENTRIES 64

typedef struct hu_tool_cache_ttl_entry {
    uint64_t key_hash;
    char *result;
    size_t result_len;
    int64_t created_at;
    int64_t expires_at;
    uint32_t hit_count;
} hu_tool_cache_ttl_entry_t;

typedef struct hu_tool_cache_ttl {
    hu_allocator_t *alloc;
    hu_tool_cache_ttl_entry_t entries[HU_TOOL_CACHE_TTL_MAX_ENTRIES];
    size_t count;
    uint32_t total_hits;
    uint32_t total_misses;
} hu_tool_cache_ttl_t;

hu_error_t hu_tool_cache_ttl_init(hu_tool_cache_ttl_t *cache, hu_allocator_t *alloc);
void hu_tool_cache_ttl_deinit(hu_tool_cache_ttl_t *cache);

/** Compute cache key from tool name + arguments. */
uint64_t hu_tool_cache_ttl_key(const char *tool_name, size_t name_len, const char *args_json,
                               size_t args_len);

/** Look up a cached result. Returns NULL on miss. Caller must not free returned pointer. */
const char *hu_tool_cache_ttl_get(hu_tool_cache_ttl_t *cache, uint64_t key, size_t *out_len);

/** Store a result with TTL. Copies the result string. */
hu_error_t hu_tool_cache_ttl_put(hu_tool_cache_ttl_t *cache, uint64_t key, const char *result,
                                 size_t result_len, int64_t ttl_seconds);

/** Evict expired entries. Returns number of entries evicted. */
size_t hu_tool_cache_ttl_evict_expired(hu_tool_cache_ttl_t *cache, int64_t now);

/** Get default TTL for a tool by name (heuristic: side-effect-free tools get longer TTL). */
int64_t hu_tool_cache_ttl_default_for(const char *tool_name, size_t name_len);

#endif /* HU_TOOLS_CACHE_TTL_H */
