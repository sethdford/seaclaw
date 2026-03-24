#ifndef HU_AGENT_PROMPT_CACHE_H
#define HU_AGENT_PROMPT_CACHE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Prompt hash cache for system prompt deduplication.
 *
 * Avoids re-encoding identical system prompts across turns by caching
 * a hash of the prompt content. When the hash matches, the provider can
 * skip re-tokenizing the system prompt (e.g., via provider-level prompt
 * caching APIs like Anthropic's or Gemini's cached_content).
 */

#define HU_PROMPT_CACHE_SLOTS 8

typedef struct hu_prompt_cache_entry {
    uint64_t hash;
    char *provider_cache_id;
    size_t provider_cache_id_len;
    int64_t created_at;
    int64_t expires_at;
} hu_prompt_cache_entry_t;

typedef struct hu_prompt_cache {
    hu_allocator_t *alloc;
    hu_prompt_cache_entry_t entries[HU_PROMPT_CACHE_SLOTS];
    size_t count;
} hu_prompt_cache_t;

hu_error_t hu_prompt_cache_init(hu_prompt_cache_t *cache, hu_allocator_t *alloc);
void hu_prompt_cache_deinit(hu_prompt_cache_t *cache);

/** Compute a stable hash for prompt content. */
uint64_t hu_prompt_cache_hash(const char *text, size_t text_len);

/** Look up a cached provider cache ID for a prompt hash. Returns NULL if miss. */
const char *hu_prompt_cache_lookup(const hu_prompt_cache_t *cache, uint64_t hash,
                                   size_t *out_id_len);

/** Store a provider cache ID associated with a prompt hash. Evicts oldest on overflow. */
hu_error_t hu_prompt_cache_store(hu_prompt_cache_t *cache, uint64_t hash,
                                 const char *provider_cache_id, size_t id_len, int64_t ttl_seconds);

/** Invalidate all entries. */
void hu_prompt_cache_clear(hu_prompt_cache_t *cache);

#endif /* HU_AGENT_PROMPT_CACHE_H */
