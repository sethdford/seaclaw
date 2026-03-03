#include "seaclaw/gateway/rate_limit.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SC_RATE_IP_MAX 64
#define SC_RATE_ENTRIES_MAX 512

typedef struct rate_entry {
    char ip[SC_RATE_IP_MAX];
    time_t *timestamps;
    size_t count;
    size_t cap;
    int max_requests;
    time_t window_secs;
} rate_entry_t;

struct sc_rate_limiter {
    sc_allocator_t *alloc;
    rate_entry_t entries[SC_RATE_ENTRIES_MAX];
    size_t count;
    int max_requests;
    int window_secs;
};

static void prune_entry(rate_entry_t *e) {
    if (!e || !e->timestamps)
        return;
    time_t now = time(NULL);
    time_t cutoff = now - (time_t)e->window_secs;
    size_t write = 0;
    for (size_t i = 0; i < e->count; i++) {
        if (e->timestamps[i] > cutoff)
            e->timestamps[write++] = e->timestamps[i];
    }
    e->count = write;
}

static rate_entry_t *find_or_create(sc_rate_limiter_t *lim, const char *ip) {
    for (size_t i = 0; i < lim->count; i++) {
        if (strcmp(lim->entries[i].ip, ip) == 0)
            return &lim->entries[i];
    }
    if (lim->count >= SC_RATE_ENTRIES_MAX)
        return NULL;
    rate_entry_t *e = &lim->entries[lim->count];
    memset(e, 0, sizeof(*e));
    strncpy(e->ip, ip, SC_RATE_IP_MAX - 1);
    e->ip[SC_RATE_IP_MAX - 1] = '\0';
    e->max_requests = lim->max_requests;
    e->window_secs = (time_t)lim->window_secs;
    e->cap = 64;
    e->timestamps =
        (time_t *)lim->alloc->alloc(lim->alloc->ctx, e->cap * sizeof(time_t));
    if (!e->timestamps)
        return NULL;
    lim->count++;
    return e;
}

sc_rate_limiter_t *sc_rate_limiter_create(sc_allocator_t *alloc, int requests_per_window,
                                          int window_secs) {
    if (!alloc)
        return NULL;
    if (requests_per_window <= 0)
        requests_per_window = 60;
    if (window_secs <= 0)
        window_secs = 60;

    sc_rate_limiter_t *lim =
        (sc_rate_limiter_t *)alloc->alloc(alloc->ctx, sizeof(sc_rate_limiter_t));
    if (!lim)
        return NULL;
    memset(lim, 0, sizeof(*lim));
    lim->alloc = alloc;
    lim->max_requests = requests_per_window;
    lim->window_secs = window_secs;
    return lim;
}

bool sc_rate_limiter_allow(sc_rate_limiter_t *lim, const char *ip) {
    if (!lim || !ip)
        return true;

    rate_entry_t *e = find_or_create(lim, ip);
    if (!e)
        return false;

    prune_entry(e);

    time_t now = time(NULL);
    if (e->count >= e->cap) {
        size_t new_cap = e->cap * 2;
        time_t *n = (time_t *)lim->alloc->realloc(lim->alloc->ctx, e->timestamps,
                                                  e->cap * sizeof(time_t),
                                                  new_cap * sizeof(time_t));
        if (!n)
            return false;
        e->timestamps = n;
        e->cap = new_cap;
    }
    e->timestamps[e->count++] = now;
    return e->count <= (size_t)e->max_requests;
}

void sc_rate_limiter_destroy(sc_rate_limiter_t *lim) {
    if (!lim)
        return;
    for (size_t i = 0; i < lim->count; i++) {
        if (lim->entries[i].timestamps)
            lim->alloc->free(lim->alloc->ctx, lim->entries[i].timestamps,
                            lim->entries[i].cap * sizeof(time_t));
    }
    lim->alloc->free(lim->alloc->ctx, lim, sizeof(*lim));
}
