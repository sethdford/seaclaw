#include "human/tools/cache_ttl.h"
#include "human/core/string.h"
#include <string.h>
#include <time.h>

hu_error_t hu_tool_cache_ttl_init(hu_tool_cache_ttl_t *cache, hu_allocator_t *alloc) {
    if (!cache || !alloc)
        return HU_ERR_INVALID_ARGUMENT;
    memset(cache, 0, sizeof(*cache));
    cache->alloc = alloc;
    return HU_OK;
}

void hu_tool_cache_ttl_deinit(hu_tool_cache_ttl_t *cache) {
    if (!cache)
        return;
    for (size_t i = 0; i < cache->count; i++) {
        if (cache->entries[i].result)
            hu_str_free(cache->alloc, cache->entries[i].result);
    }
    memset(cache->entries, 0, sizeof(cache->entries));
    cache->count = 0;
}

uint64_t hu_tool_cache_ttl_key(const char *tool_name, size_t name_len, const char *args_json,
                               size_t args_len) {
    /* FNV-1a combining tool name + args */
    uint64_t h = 14695981039346656037ULL;
    if (tool_name) {
        for (size_t i = 0; i < name_len; i++) {
            h ^= (uint64_t)(unsigned char)tool_name[i];
            h *= 1099511628211ULL;
        }
    }
    h ^= 0xFF;
    h *= 1099511628211ULL;
    if (args_json) {
        for (size_t i = 0; i < args_len; i++) {
            h ^= (uint64_t)(unsigned char)args_json[i];
            h *= 1099511628211ULL;
        }
    }
    return h;
}

const char *hu_tool_cache_ttl_get(hu_tool_cache_ttl_t *cache, uint64_t key, size_t *out_len) {
    if (!cache || !out_len)
        return NULL;
    *out_len = 0;
    int64_t now = (int64_t)time(NULL);
    for (size_t i = 0; i < cache->count; i++) {
        if (cache->entries[i].key_hash == key) {
            if (cache->entries[i].expires_at > 0 && cache->entries[i].expires_at <= now) {
                /* Expired — lazy eviction */
                hu_str_free(cache->alloc, cache->entries[i].result);
                cache->entries[i] = cache->entries[cache->count - 1];
                memset(&cache->entries[cache->count - 1], 0, sizeof(cache->entries[0]));
                cache->count--;
                cache->total_misses++;
                return NULL;
            }
            cache->entries[i].hit_count++;
            cache->total_hits++;
            *out_len = cache->entries[i].result_len;
            return cache->entries[i].result;
        }
    }
    cache->total_misses++;
    return NULL;
}

hu_error_t hu_tool_cache_ttl_put(hu_tool_cache_ttl_t *cache, uint64_t key, const char *result,
                                 size_t result_len, int64_t ttl_seconds) {
    if (!cache || !result)
        return HU_ERR_INVALID_ARGUMENT;

    int64_t now = (int64_t)time(NULL);

    /* Update existing entry */
    for (size_t i = 0; i < cache->count; i++) {
        if (cache->entries[i].key_hash == key) {
            if (cache->entries[i].result)
                hu_str_free(cache->alloc, cache->entries[i].result);
            cache->entries[i].result = hu_strndup(cache->alloc, result, result_len);
            cache->entries[i].result_len = result_len;
            cache->entries[i].created_at = now;
            cache->entries[i].expires_at = ttl_seconds > 0 ? now + ttl_seconds : 0;
            return HU_OK;
        }
    }

    /* Evict LRU if full */
    if (cache->count >= HU_TOOL_CACHE_TTL_MAX_ENTRIES) {
        (void)hu_tool_cache_ttl_evict_expired(cache, now);
        if (cache->count >= HU_TOOL_CACHE_TTL_MAX_ENTRIES) {
            size_t lru = 0;
            for (size_t i = 1; i < cache->count; i++) {
                if (cache->entries[i].created_at < cache->entries[lru].created_at)
                    lru = i;
            }
            if (cache->entries[lru].result)
                hu_str_free(cache->alloc, cache->entries[lru].result);
            cache->entries[lru] = cache->entries[cache->count - 1];
            memset(&cache->entries[cache->count - 1], 0, sizeof(cache->entries[0]));
            cache->count--;
        }
    }

    size_t idx = cache->count++;
    cache->entries[idx].key_hash = key;
    cache->entries[idx].result = hu_strndup(cache->alloc, result, result_len);
    cache->entries[idx].result_len = result_len;
    cache->entries[idx].created_at = now;
    cache->entries[idx].expires_at = ttl_seconds > 0 ? now + ttl_seconds : 0;
    cache->entries[idx].hit_count = 0;
    return HU_OK;
}

size_t hu_tool_cache_ttl_evict_expired(hu_tool_cache_ttl_t *cache, int64_t now) {
    if (!cache)
        return 0;
    size_t evicted = 0;
    size_t i = 0;
    while (i < cache->count) {
        if (cache->entries[i].expires_at > 0 && cache->entries[i].expires_at <= now) {
            if (cache->entries[i].result)
                hu_str_free(cache->alloc, cache->entries[i].result);
            cache->entries[i] = cache->entries[cache->count - 1];
            memset(&cache->entries[cache->count - 1], 0, sizeof(cache->entries[0]));
            cache->count--;
            evicted++;
        } else {
            i++;
        }
    }
    return evicted;
}

int64_t hu_tool_cache_ttl_default_for(const char *tool_name, size_t name_len) {
    if (!tool_name || name_len == 0)
        return 0;

    typedef struct {
        const char *prefix;
        size_t prefix_len;
        int64_t ttl;
    } rule_t;

    static const rule_t rules[] = {
        {"read_file", 9, 60},     {"list_dir", 8, 30},      {"web_search", 10, 300},
        {"get_weather", 11, 600}, {"calculator", 10, 3600}, {"datetime", 8, 1},
        {"shell", 5, 0},          {"write_file", 10, 0},    {"delete", 6, 0},
        {"send_message", 12, 0},
    };
    static const size_t rule_count = sizeof(rules) / sizeof(rules[0]);

    for (size_t i = 0; i < rule_count; i++) {
        if (name_len >= rules[i].prefix_len &&
            memcmp(tool_name, rules[i].prefix, rules[i].prefix_len) == 0)
            return rules[i].ttl;
    }
    return 0;
}
