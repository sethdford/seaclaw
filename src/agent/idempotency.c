#include "human/agent/idempotency.h"
#include "human/core/string.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* FNV-1a hash for consistent key generation */
static uint64_t fnv1a_hash(const char *data, size_t len) {
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint8_t)data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

/* Internal hash bucket for quick lookups */
typedef struct hu_idempotency_bucket {
    hu_idempotency_entry_t *entries;
    size_t count;
    size_t cap;
} hu_idempotency_bucket_t;

struct hu_idempotency_registry {
    hu_idempotency_bucket_t *buckets;
    size_t bucket_count;       /* Number of hash buckets */
    size_t total_entries;      /* Current total entries across all buckets */
    size_t total_hits;         /* Stats: cache hits */
    size_t total_misses;       /* Stats: cache misses */
};

#define HU_IDEMPOTENCY_BUCKET_COUNT 128

/* Create a new idempotency registry. */
hu_error_t hu_idempotency_create(hu_allocator_t *alloc, hu_idempotency_registry_t **out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;

    hu_idempotency_registry_t *reg =
        (hu_idempotency_registry_t *)alloc->alloc(alloc->ctx, sizeof(*reg));
    if (!reg)
        return HU_ERR_OUT_OF_MEMORY;

    reg->buckets =
        (hu_idempotency_bucket_t *)alloc->alloc(alloc->ctx, HU_IDEMPOTENCY_BUCKET_COUNT * sizeof(*reg->buckets));
    if (!reg->buckets) {
        alloc->free(alloc->ctx, reg, sizeof(*reg));
        return HU_ERR_OUT_OF_MEMORY;
    }

    memset(reg->buckets, 0, HU_IDEMPOTENCY_BUCKET_COUNT * sizeof(*reg->buckets));
    reg->bucket_count = HU_IDEMPOTENCY_BUCKET_COUNT;
    reg->total_entries = 0;
    reg->total_hits = 0;
    reg->total_misses = 0;

    *out = reg;
    return HU_OK;
}

/* Generate idempotency key: "<tool_name>:<16-hex-hash>" */
hu_error_t hu_idempotency_key_generate(hu_allocator_t *alloc, const char *tool_name,
                                       const char *args_json, char **out_key, size_t *out_key_len) {
    if (!alloc || !tool_name || !out_key || !out_key_len)
        return HU_ERR_INVALID_ARGUMENT;

    /* Hash the args to get a deterministic key component */
    size_t args_len = args_json ? strlen(args_json) : 0;
    uint64_t hash = fnv1a_hash(args_json ? args_json : "", args_len);

    /* Format: "tool_name:0123456789abcdef" (16 hex digits = 64 bits) */
    size_t tool_name_len = strlen(tool_name);
    size_t key_len = tool_name_len + 1 + 16; /* +1 for ':', +16 for hex hash */
    char *key = (char *)alloc->alloc(alloc->ctx, key_len + 1);
    if (!key)
        return HU_ERR_OUT_OF_MEMORY;

    int wrote = snprintf(key, key_len + 1, "%s:%016llx", tool_name, (unsigned long long)hash);
    if (wrote < 0 || (size_t)wrote != key_len) {
        alloc->free(alloc->ctx, key, key_len + 1);
        return HU_ERR_INTERNAL;
    }

    *out_key = key;
    *out_key_len = key_len;
    return HU_OK;
}

/* Helper: find entry in bucket by key */
static hu_idempotency_entry_t *bucket_find(hu_idempotency_bucket_t *bucket, const char *key,
                                           size_t key_len) {
    if (!bucket || !key)
        return NULL;
    for (size_t i = 0; i < bucket->count; i++) {
        hu_idempotency_entry_t *e = &bucket->entries[i];
        if (e->key_len == key_len && memcmp(e->key, key, key_len) == 0)
            return e;
    }
    return NULL;
}

/* Helper: check if entry has expired */
static bool entry_expired(const hu_idempotency_entry_t *e) {
    if (e->expires_at == 0)
        return false; /* Never expires */
    time_t now = time(NULL);
    return e->expires_at <= (int64_t)now;
}

/* Check if a tool call has been cached. Returns true + cached result if found. */
bool hu_idempotency_check(hu_idempotency_registry_t *reg, const char *tool_name,
                          const char *args_json, hu_idempotency_entry_t *out) {
    if (!reg || !tool_name || !args_json || !out)
        return false;

    /* Generate the key */
    char key_buf[256];
    size_t args_len = strlen(args_json);
    uint64_t hash = fnv1a_hash(args_json, args_len);

    int wrote = snprintf(key_buf, sizeof(key_buf), "%s:%016llx", tool_name, (unsigned long long)hash);
    if (wrote < 0 || (size_t)wrote >= sizeof(key_buf))
        return false;

    size_t key_len = (size_t)wrote;

    /* Hash the key to pick a bucket */
    uint64_t bucket_hash = fnv1a_hash(key_buf, key_len);
    size_t bucket_idx = bucket_hash % reg->bucket_count;

    hu_idempotency_bucket_t *bucket = &reg->buckets[bucket_idx];
    hu_idempotency_entry_t *found = bucket_find(bucket, key_buf, key_len);

    if (!found) {
        reg->total_misses++;
        return false;
    }

    /* Check expiration */
    if (entry_expired(found)) {
        /* Entry expired, treat as miss */
        reg->total_misses++;
        return false;
    }

    /* Copy the entry to output */
    *out = *found;
    reg->total_hits++;
    return true;
}

/* Record a tool call result for future dedup. */
hu_error_t hu_idempotency_record(hu_idempotency_registry_t *reg, hu_allocator_t *alloc,
                                 const char *tool_name, const char *args_json,
                                 const char *result_json, bool is_error) {
    if (!reg || !alloc || !tool_name || !args_json || !result_json)
        return HU_ERR_INVALID_ARGUMENT;

    /* Generate the key */
    char *key = NULL;
    size_t key_len = 0;
    hu_error_t err = hu_idempotency_key_generate(alloc, tool_name, args_json, &key, &key_len);
    if (err != HU_OK)
        return err;

    /* Hash the key to pick a bucket */
    uint64_t bucket_hash = fnv1a_hash(key, key_len);
    size_t bucket_idx = bucket_hash % reg->bucket_count;

    hu_idempotency_bucket_t *bucket = &reg->buckets[bucket_idx];

    /* Check if entry already exists */
    hu_idempotency_entry_t *existing = bucket_find(bucket, key, key_len);
    if (existing) {
        /* Update existing entry */
        alloc->free(alloc->ctx, existing->result_json, existing->result_json_len + 1);
        existing->result_json = hu_strndup(alloc, result_json, strlen(result_json));
        if (!existing->result_json) {
            alloc->free(alloc->ctx, key, key_len + 1);
            return HU_ERR_OUT_OF_MEMORY;
        }
        existing->result_json_len = strlen(result_json);
        existing->is_error = is_error;
        existing->created_at = (int64_t)time(NULL);
        existing->expires_at = 0; /* Default: never expire */
        alloc->free(alloc->ctx, key, key_len + 1);
        return HU_OK;
    }

    /* Check if we need to evict (LRU: remove oldest entry in bucket) */
    if (reg->total_entries >= HU_IDEMPOTENCY_MAX_ENTRIES && bucket->count > 0) {
        /* Find oldest entry in this bucket */
        size_t oldest_idx = 0;
        int64_t oldest_time = bucket->entries[0].created_at;
        for (size_t i = 1; i < bucket->count; i++) {
            if (bucket->entries[i].created_at < oldest_time) {
                oldest_time = bucket->entries[i].created_at;
                oldest_idx = i;
            }
        }
        /* Remove oldest entry */
        hu_idempotency_entry_t *old_e = &bucket->entries[oldest_idx];
        if (old_e->key)
            alloc->free(alloc->ctx, old_e->key, old_e->key_len + 1);
        if (old_e->result_json)
            alloc->free(alloc->ctx, old_e->result_json, old_e->result_json_len + 1);
        /* Shift remaining entries */
        if (oldest_idx < bucket->count - 1) {
            memmove(&bucket->entries[oldest_idx], &bucket->entries[oldest_idx + 1],
                    (bucket->count - oldest_idx - 1) * sizeof(hu_idempotency_entry_t));
        }
        bucket->count--;
        reg->total_entries--;
    }

    /* Grow bucket if needed */
    if (bucket->count >= bucket->cap) {
        size_t new_cap = bucket->cap ? bucket->cap * 2 : 8;
        hu_idempotency_entry_t *new_entries =
            (hu_idempotency_entry_t *)alloc->realloc(alloc->ctx, bucket->entries,
                                                     bucket->cap * sizeof(*bucket->entries),
                                                     new_cap * sizeof(*bucket->entries));
        if (!new_entries) {
            alloc->free(alloc->ctx, key, key_len + 1);
            return HU_ERR_OUT_OF_MEMORY;
        }
        bucket->entries = new_entries;
        bucket->cap = new_cap;
    }

    /* Add new entry to bucket */
    hu_idempotency_entry_t *new_e = &bucket->entries[bucket->count++];
    new_e->key = key;
    new_e->key_len = key_len;
    new_e->result_json = hu_strndup(alloc, result_json, strlen(result_json));
    if (!new_e->result_json) {
        alloc->free(alloc->ctx, key, key_len + 1);
        bucket->count--;
        return HU_ERR_OUT_OF_MEMORY;
    }
    new_e->result_json_len = strlen(result_json);
    new_e->is_error = is_error;
    new_e->created_at = (int64_t)time(NULL);
    new_e->expires_at = 0; /* Default: never expire */

    reg->total_entries++;
    return HU_OK;
}

/* Clear all entries (e.g., on new workflow). */
void hu_idempotency_clear(hu_idempotency_registry_t *reg, hu_allocator_t *alloc) {
    if (!reg || !alloc)
        return;

    for (size_t i = 0; i < reg->bucket_count; i++) {
        hu_idempotency_bucket_t *bucket = &reg->buckets[i];
        for (size_t j = 0; j < bucket->count; j++) {
            hu_idempotency_entry_t *e = &bucket->entries[j];
            if (e->key)
                alloc->free(alloc->ctx, e->key, e->key_len + 1);
            if (e->result_json)
                alloc->free(alloc->ctx, e->result_json, e->result_json_len + 1);
        }
        if (bucket->entries)
            alloc->free(alloc->ctx, bucket->entries, bucket->cap * sizeof(*bucket->entries));
        bucket->entries = NULL;
        bucket->count = 0;
        bucket->cap = 0;
    }
    reg->total_entries = 0;
    reg->total_hits = 0;
    reg->total_misses = 0;
}

/* Destroy registry and free all resources. */
void hu_idempotency_destroy(hu_idempotency_registry_t *reg, hu_allocator_t *alloc) {
    if (!reg || !alloc)
        return;

    hu_idempotency_clear(reg, alloc);

    if (reg->buckets)
        alloc->free(alloc->ctx, reg->buckets, reg->bucket_count * sizeof(*reg->buckets));

    alloc->free(alloc->ctx, reg, sizeof(*reg));
}

/* Get registry stats for debugging/monitoring. */
void hu_idempotency_stats(const hu_idempotency_registry_t *reg, hu_idempotency_stats_t *out) {
    if (!reg || !out)
        return;

    out->entry_count = reg->total_entries;
    out->max_entries = HU_IDEMPOTENCY_MAX_ENTRIES;
    out->total_hits = reg->total_hits;
    out->total_misses = reg->total_misses;
}
