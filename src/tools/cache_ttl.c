#include "human/tools/cache_ttl.h"
#include "human/core/string.h"
#include <ctype.h>
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

static bool nm_has(const char *tool_name, size_t name_len, const char *sub) {
    if (!tool_name || name_len == 0 || !sub)
        return false;
    size_t sl = strlen(sub);
    if (sl > name_len)
        return false;
    for (size_t i = 0; i + sl <= name_len; i++) {
        size_t j = 0;
        while (j < sl && tolower((unsigned char)tool_name[i + j]) == tolower((unsigned char)sub[j]))
            j++;
        if (j == sl)
            return true;
    }
    return false;
}

static bool nm_prefix_ci(const char *tool_name, size_t name_len, const char *pre) {
    size_t pl = strlen(pre);
    if (pl > name_len)
        return false;
    for (size_t i = 0; i < pl; i++) {
        if (tolower((unsigned char)tool_name[i]) != tolower((unsigned char)pre[i]))
            return false;
    }
    return true;
}

hu_tool_cacheability_t hu_tool_cache_classify(const char *tool_name, size_t name_len,
                                              const char *args_json, size_t args_len) {
    (void)args_json;
    (void)args_len;
    if (!tool_name || name_len == 0)
        return HU_TOOL_CACHE_SHORT;

    if (nm_prefix_ci(tool_name, name_len, "send_") ||
        nm_prefix_ci(tool_name, name_len, "delete_") ||
        nm_prefix_ci(tool_name, name_len, "write_") || nm_prefix_ci(tool_name, name_len, "run_") ||
        nm_prefix_ci(tool_name, name_len, "create_") ||
        nm_prefix_ci(tool_name, name_len, "update_") || nm_has(tool_name, name_len, "exec") ||
        nm_has(tool_name, name_len, "shell") || nm_has(tool_name, name_len, "spawn") ||
        nm_prefix_ci(tool_name, name_len, "media_"))
        return HU_TOOL_CACHE_NEVER;

    if (nm_has(tool_name, name_len, "weather") || nm_has(tool_name, name_len, "time") ||
        nm_has(tool_name, name_len, "price") || nm_has(tool_name, name_len, "status"))
        return HU_TOOL_CACHE_SHORT;

    if (nm_has(tool_name, name_len, "get_config") || nm_prefix_ci(tool_name, name_len, "list_") ||
        nm_has(tool_name, name_len, "schema") || nm_prefix_ci(tool_name, name_len, "describe_") ||
        nm_has(tool_name, name_len, "help"))
        return HU_TOOL_CACHE_LONG;

    if (nm_prefix_ci(tool_name, name_len, "search_") ||
        nm_prefix_ci(tool_name, name_len, "lookup_") || nm_prefix_ci(tool_name, name_len, "get_"))
        return HU_TOOL_CACHE_MEDIUM;
    if (name_len == 10 && memcmp(tool_name, "web_search", 10) == 0)
        return HU_TOOL_CACHE_MEDIUM;

    return HU_TOOL_CACHE_SHORT;
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

    switch (hu_tool_cache_classify(tool_name, name_len, NULL, 0)) {
    case HU_TOOL_CACHE_NEVER:
        return 0;
    case HU_TOOL_CACHE_SHORT:
        return 60;
    case HU_TOOL_CACHE_MEDIUM:
        return 300;
    case HU_TOOL_CACHE_LONG:
        return 3600;
    default:
        return 0;
    }
}
