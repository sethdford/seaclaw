#include "human/agent/kv_cache.h"
#include "human/core/string.h"
#include <string.h>
#include <time.h>

hu_error_t hu_kv_cache_init(hu_kv_cache_manager_t *mgr, hu_allocator_t *alloc,
                            uint32_t max_tokens) {
    if (!mgr || !alloc || max_tokens == 0)
        return HU_ERR_INVALID_ARGUMENT;
    memset(mgr, 0, sizeof(*mgr));
    mgr->alloc = alloc;
    mgr->max_tokens = max_tokens;
    mgr->eviction_threshold = 0.9f;
    return HU_OK;
}

void hu_kv_cache_deinit(hu_kv_cache_manager_t *mgr) {
    if (!mgr)
        return;
    hu_kv_cache_clear(mgr);
}

void hu_kv_cache_clear(hu_kv_cache_manager_t *mgr) {
    if (!mgr)
        return;
    for (size_t i = 0; i < mgr->segment_count; i++) {
        if (mgr->segments[i].label && mgr->alloc)
            mgr->alloc->free(mgr->alloc->ctx, mgr->segments[i].label,
                             mgr->segments[i].label_len + 1);
    }
    mgr->segment_count = 0;
    mgr->total_tokens = 0;
}

hu_error_t hu_kv_cache_add_segment(hu_kv_cache_manager_t *mgr, const char *label, size_t label_len,
                                   uint32_t token_count, bool pinned) {
    if (!mgr || !label || label_len == 0)
        return HU_ERR_INVALID_ARGUMENT;
    if (mgr->segment_count >= HU_KV_CACHE_MAX_SEGMENTS)
        return HU_ERR_INVALID_ARGUMENT;

    hu_kv_segment_t *seg = &mgr->segments[mgr->segment_count];
    seg->label = hu_strndup(mgr->alloc, label, label_len);
    if (!seg->label)
        return HU_ERR_OUT_OF_MEMORY;
    seg->label_len = label_len;
    seg->token_count = token_count;
    seg->attention_score = pinned ? 1.0f : 0.5f;
    seg->created_at = (int64_t)time(NULL);
    seg->pinned = pinned;
    mgr->segment_count++;
    mgr->total_tokens += token_count;
    return HU_OK;
}

hu_error_t hu_kv_cache_boost_attention(hu_kv_cache_manager_t *mgr, const char *label,
                                       size_t label_len, float delta) {
    if (!mgr || !label)
        return HU_ERR_INVALID_ARGUMENT;
    for (size_t i = 0; i < mgr->segment_count; i++) {
        if (mgr->segments[i].label_len == label_len &&
            memcmp(mgr->segments[i].label, label, label_len) == 0) {
            mgr->segments[i].attention_score += delta;
            if (mgr->segments[i].attention_score > 1.0f)
                mgr->segments[i].attention_score = 1.0f;
            return HU_OK;
        }
    }
    return HU_ERR_NOT_FOUND;
}

float hu_kv_cache_utilization(const hu_kv_cache_manager_t *mgr) {
    if (!mgr || mgr->max_tokens == 0)
        return 0.0f;
    return (float)mgr->total_tokens / (float)mgr->max_tokens;
}

bool hu_kv_cache_needs_eviction(const hu_kv_cache_manager_t *mgr) {
    return hu_kv_cache_utilization(mgr) >= mgr->eviction_threshold;
}

size_t hu_kv_cache_prune(hu_kv_cache_manager_t *mgr, const char **out_evicted_labels,
                         size_t max_evicted) {
    if (!mgr || mgr->segment_count == 0)
        return 0;

    size_t evicted = 0;
    float target = mgr->eviction_threshold * 0.8f; /* prune to 80% of threshold */
    float target_tokens = target * (float)mgr->max_tokens;

    while (mgr->total_tokens > (uint32_t)target_tokens && evicted < max_evicted) {
        /* Find lowest-attention unpinned segment */
        size_t min_idx = (size_t)-1;
        float min_score = 2.0f;
        for (size_t i = 0; i < mgr->segment_count; i++) {
            if (!mgr->segments[i].pinned && mgr->segments[i].attention_score < min_score) {
                min_score = mgr->segments[i].attention_score;
                min_idx = i;
            }
        }
        if (min_idx == (size_t)-1)
            break; /* only pinned segments remain */

        if (out_evicted_labels)
            out_evicted_labels[evicted] = mgr->segments[min_idx].label;

        mgr->total_tokens -= mgr->segments[min_idx].token_count;

        /* Don't free label if caller will read it via out_evicted_labels */
        if (!out_evicted_labels && mgr->segments[min_idx].label)
            mgr->alloc->free(mgr->alloc->ctx, mgr->segments[min_idx].label,
                             mgr->segments[min_idx].label_len + 1);

        /* Shift remaining segments */
        for (size_t j = min_idx; j + 1 < mgr->segment_count; j++)
            mgr->segments[j] = mgr->segments[j + 1];
        mgr->segment_count--;
        evicted++;
    }
    return evicted;
}
