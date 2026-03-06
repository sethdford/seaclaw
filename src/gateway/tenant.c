#include "seaclaw/gateway/tenant.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_TENANTS 256

typedef struct rate_bucket {
    int64_t window_start;
    uint32_t count;
} rate_bucket_t;

struct sc_tenant_store {
    sc_allocator_t *alloc;
    sc_tenant_t tenants[MAX_TENANTS];
    rate_bucket_t rate_buckets[MAX_TENANTS];
    size_t count;
};

sc_error_t sc_tenant_store_init(sc_allocator_t *alloc, sc_tenant_store_t **out) {
    if (!alloc || !out)
        return SC_ERR_INVALID_ARGUMENT;
    sc_tenant_store_t *s = (sc_tenant_store_t *)alloc->alloc(alloc->ctx, sizeof(sc_tenant_store_t));
    if (!s)
        return SC_ERR_OUT_OF_MEMORY;
    memset(s, 0, sizeof(*s));
    s->alloc = alloc;
    *out = s;
    return SC_OK;
}

void sc_tenant_store_destroy(sc_tenant_store_t *store) {
    if (!store || !store->alloc)
        return;
    store->alloc->free(store->alloc->ctx, store, sizeof(sc_tenant_store_t));
}

static int find_tenant(sc_tenant_store_t *store, const char *user_id) {
    if (!store || !user_id)
        return -1;
    for (size_t i = 0; i < store->count; i++) {
        if (strcmp(store->tenants[i].user_id, user_id) == 0)
            return (int)i;
    }
    return -1;
}

sc_error_t sc_tenant_create(sc_tenant_store_t *store, const sc_tenant_t *tenant) {
    if (!store || !tenant || tenant->user_id[0] == '\0')
        return SC_ERR_INVALID_ARGUMENT;
    if (find_tenant(store, tenant->user_id) >= 0)
        return SC_ERR_ALREADY_EXISTS;
    if (store->count >= MAX_TENANTS)
        return SC_ERR_OUT_OF_MEMORY;
    store->tenants[store->count] = *tenant;
    memset(&store->rate_buckets[store->count], 0, sizeof(rate_bucket_t));
    store->count++;
    return SC_OK;
}

sc_error_t sc_tenant_get(sc_tenant_store_t *store, const char *user_id, sc_tenant_t *out) {
    if (!store || !user_id || !out)
        return SC_ERR_INVALID_ARGUMENT;
    int idx = find_tenant(store, user_id);
    if (idx < 0)
        return SC_ERR_NOT_FOUND;
    *out = store->tenants[idx];
    return SC_OK;
}

sc_error_t sc_tenant_update(sc_tenant_store_t *store, const sc_tenant_t *tenant) {
    if (!store || !tenant)
        return SC_ERR_INVALID_ARGUMENT;
    int idx = find_tenant(store, tenant->user_id);
    if (idx < 0)
        return SC_ERR_NOT_FOUND;
    store->tenants[idx] = *tenant;
    return SC_OK;
}

sc_error_t sc_tenant_delete(sc_tenant_store_t *store, const char *user_id) {
    if (!store || !user_id)
        return SC_ERR_INVALID_ARGUMENT;
    int idx = find_tenant(store, user_id);
    if (idx < 0)
        return SC_ERR_NOT_FOUND;
    if ((size_t)idx < store->count - 1) {
        store->tenants[idx] = store->tenants[store->count - 1];
        store->rate_buckets[idx] = store->rate_buckets[store->count - 1];
    }
    store->count--;
    return SC_OK;
}

sc_error_t sc_tenant_list(sc_tenant_store_t *store, sc_tenant_t *out, size_t max, size_t *count) {
    if (!store || !out || !count)
        return SC_ERR_INVALID_ARGUMENT;
    size_t n = store->count < max ? store->count : max;
    for (size_t i = 0; i < n; i++)
        out[i] = store->tenants[i];
    *count = n;
    return SC_OK;
}

sc_error_t sc_tenant_increment_usage(sc_tenant_store_t *store, const char *user_id,
                                     uint64_t tokens) {
    if (!store || !user_id)
        return SC_ERR_INVALID_ARGUMENT;
    int idx = find_tenant(store, user_id);
    if (idx < 0)
        return SC_ERR_NOT_FOUND;
    store->tenants[idx].usage_current_tokens += tokens;
    return SC_OK;
}

bool sc_tenant_check_quota(sc_tenant_store_t *store, const char *user_id) {
    if (!store || !user_id)
        return false;
    int idx = find_tenant(store, user_id);
    if (idx < 0)
        return false;
    if (store->tenants[idx].usage_quota_tokens == 0)
        return true; /* unlimited */
    return store->tenants[idx].usage_current_tokens < store->tenants[idx].usage_quota_tokens;
}

bool sc_tenant_check_rate_limit(sc_tenant_store_t *store, const char *user_id) {
    if (!store || !user_id)
        return false;
    int idx = find_tenant(store, user_id);
    if (idx < 0)
        return false;
    if (store->tenants[idx].rate_limit_rpm == 0)
        return true; /* unlimited */

    int64_t now = (int64_t)time(NULL);
    rate_bucket_t *b = &store->rate_buckets[idx];
    if (now - b->window_start >= 60) {
        b->window_start = now;
        b->count = 0;
    }
    if (b->count >= store->tenants[idx].rate_limit_rpm)
        return false;
    b->count++;
    return true;
}
