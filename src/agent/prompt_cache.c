#include "human/agent/prompt_cache.h"
#include "human/core/string.h"
#include <string.h>
#include <time.h>

hu_error_t hu_prompt_cache_init(hu_prompt_cache_t *cache, hu_allocator_t *alloc) {
    if (!cache || !alloc)
        return HU_ERR_INVALID_ARGUMENT;
    memset(cache, 0, sizeof(*cache));
    cache->alloc = alloc;
    return HU_OK;
}

void hu_prompt_cache_deinit(hu_prompt_cache_t *cache) {
    if (!cache)
        return;
    for (size_t i = 0; i < cache->count; i++) {
        if (cache->entries[i].provider_cache_id)
            hu_str_free(cache->alloc, cache->entries[i].provider_cache_id);
    }
    memset(cache->entries, 0, sizeof(cache->entries));
    cache->count = 0;
}

uint64_t hu_prompt_cache_hash(const char *text, size_t text_len) {
    if (!text || text_len == 0)
        return 0;
    /* FNV-1a 64-bit */
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < text_len; i++) {
        h ^= (uint64_t)(unsigned char)text[i];
        h *= 1099511628211ULL;
    }
    return h;
}

const char *hu_prompt_cache_lookup(const hu_prompt_cache_t *cache, uint64_t hash,
                                   size_t *out_id_len) {
    if (!cache || !out_id_len)
        return NULL;
    *out_id_len = 0;
    int64_t now = (int64_t)time(NULL);
    for (size_t i = 0; i < cache->count; i++) {
        if (cache->entries[i].hash == hash && cache->entries[i].expires_at > now) {
            *out_id_len = cache->entries[i].provider_cache_id_len;
            return cache->entries[i].provider_cache_id;
        }
    }
    return NULL;
}

hu_error_t hu_prompt_cache_store(hu_prompt_cache_t *cache, uint64_t hash,
                                 const char *provider_cache_id, size_t id_len,
                                 int64_t ttl_seconds) {
    if (!cache || !provider_cache_id || id_len == 0)
        return HU_ERR_INVALID_ARGUMENT;

    int64_t now = (int64_t)time(NULL);

    /* Check for existing entry with same hash */
    for (size_t i = 0; i < cache->count; i++) {
        if (cache->entries[i].hash == hash) {
            if (cache->entries[i].provider_cache_id)
                hu_str_free(cache->alloc, cache->entries[i].provider_cache_id);
            cache->entries[i].provider_cache_id =
                hu_strndup(cache->alloc, provider_cache_id, id_len);
            cache->entries[i].provider_cache_id_len = id_len;
            cache->entries[i].created_at = now;
            cache->entries[i].expires_at = now + ttl_seconds;
            return HU_OK;
        }
    }

    /* Evict oldest if full */
    if (cache->count >= HU_PROMPT_CACHE_SLOTS) {
        size_t oldest = 0;
        for (size_t i = 1; i < cache->count; i++) {
            if (cache->entries[i].created_at < cache->entries[oldest].created_at)
                oldest = i;
        }
        if (cache->entries[oldest].provider_cache_id)
            hu_str_free(cache->alloc, cache->entries[oldest].provider_cache_id);
        cache->entries[oldest].hash = hash;
        cache->entries[oldest].provider_cache_id =
            hu_strndup(cache->alloc, provider_cache_id, id_len);
        cache->entries[oldest].provider_cache_id_len = id_len;
        cache->entries[oldest].created_at = now;
        cache->entries[oldest].expires_at = now + ttl_seconds;
        return HU_OK;
    }

    size_t idx = cache->count++;
    cache->entries[idx].hash = hash;
    cache->entries[idx].provider_cache_id = hu_strndup(cache->alloc, provider_cache_id, id_len);
    cache->entries[idx].provider_cache_id_len = id_len;
    cache->entries[idx].created_at = now;
    cache->entries[idx].expires_at = now + ttl_seconds;
    return HU_OK;
}

void hu_prompt_cache_clear(hu_prompt_cache_t *cache) {
    if (!cache)
        return;
    for (size_t i = 0; i < cache->count; i++) {
        if (cache->entries[i].provider_cache_id)
            hu_str_free(cache->alloc, cache->entries[i].provider_cache_id);
    }
    memset(cache->entries, 0, sizeof(cache->entries));
    cache->count = 0;
}
