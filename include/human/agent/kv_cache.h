#ifndef HU_AGENT_KV_CACHE_H
#define HU_AGENT_KV_CACHE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * KV cache management — attention-aware context pruning.
 *
 * Manages the LLM's key-value cache window by tracking which context
 * segments are most attended-to, and pruning low-attention segments
 * when the context window approaches capacity.
 *
 * Strategy: score each segment by its cumulative attention weight
 * (approximated from response quality signals), then evict lowest-scored
 * segments first.
 */

#define HU_KV_CACHE_MAX_SEGMENTS 128

typedef struct hu_kv_segment {
    char *label; /* segment identifier (e.g. "system", "memory:3", "turn:7") */
    size_t label_len;
    uint32_t token_count; /* approximate tokens in this segment */
    float
        attention_score; /* cumulative attention weight (0.0 = never referenced, 1.0 = critical) */
    int64_t created_at;
    bool pinned; /* if true, never evict (e.g. system prompt, active tool results) */
} hu_kv_segment_t;

typedef struct hu_kv_cache_manager {
    hu_allocator_t *alloc;
    hu_kv_segment_t segments[HU_KV_CACHE_MAX_SEGMENTS];
    size_t segment_count;
    uint32_t total_tokens;
    uint32_t max_tokens;      /* context window capacity */
    float eviction_threshold; /* start evicting when total_tokens/max_tokens exceeds this (default
                                 0.9) */
} hu_kv_cache_manager_t;

hu_error_t hu_kv_cache_init(hu_kv_cache_manager_t *mgr, hu_allocator_t *alloc, uint32_t max_tokens);
void hu_kv_cache_deinit(hu_kv_cache_manager_t *mgr);

/* Add a segment to track. */
hu_error_t hu_kv_cache_add_segment(hu_kv_cache_manager_t *mgr, const char *label, size_t label_len,
                                   uint32_t token_count, bool pinned);

/* Update attention score for a segment (additive — called after each turn). */
hu_error_t hu_kv_cache_boost_attention(hu_kv_cache_manager_t *mgr, const char *label,
                                       size_t label_len, float delta);

/* Prune lowest-attention unpinned segments to bring total below threshold.
 * Returns number of segments evicted. Evicted labels written to out_evicted. */
size_t hu_kv_cache_prune(hu_kv_cache_manager_t *mgr, const char **out_evicted_labels,
                         size_t max_evicted);

/* Get current utilization ratio (total_tokens / max_tokens). */
float hu_kv_cache_utilization(const hu_kv_cache_manager_t *mgr);

/* Check if eviction is needed. */
bool hu_kv_cache_needs_eviction(const hu_kv_cache_manager_t *mgr);

#endif /* HU_AGENT_KV_CACHE_H */
