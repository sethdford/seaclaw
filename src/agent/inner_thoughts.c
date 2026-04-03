/*
 * Inner thoughts accumulation — per-contact anticipatory state between conversations.
 */
#include "human/agent/inner_thoughts.h"
#include "human/core/debug.h"
#include "human/core/string.h"
#include <string.h>

#define INITIAL_CAPACITY              16
#define HU_INNER_THOUGHT_MAX_CAPACITY 1024
#define MS_PER_DAY                    (86400ULL * 1000ULL)

hu_error_t hu_inner_thought_store_init(hu_inner_thought_store_t *store, hu_allocator_t *alloc) {
    if (!store || !alloc)
        return HU_ERR_INVALID_ARGUMENT;
    store->alloc = alloc;
    store->count = 0;
    store->capacity = INITIAL_CAPACITY;
    store->items = (hu_inner_thought_t *)alloc->alloc(alloc->ctx, INITIAL_CAPACITY *
                                                                      sizeof(hu_inner_thought_t));
    if (!store->items) {
        store->capacity = 0;
        return HU_ERR_OUT_OF_MEMORY;
    }
    memset(store->items, 0, INITIAL_CAPACITY * sizeof(hu_inner_thought_t));
    return HU_OK;
}

void hu_inner_thought_store_deinit(hu_inner_thought_store_t *store) {
    if (!store || !store->alloc)
        return;
    hu_allocator_t *alloc = store->alloc;
    for (size_t i = 0; i < store->count; i++) {
        hu_inner_thought_t *t = &store->items[i];
        if (t->contact_id)
            alloc->free(alloc->ctx, t->contact_id, t->contact_id_len + 1);
        if (t->topic)
            alloc->free(alloc->ctx, t->topic, t->topic_len + 1);
        if (t->thought_text)
            alloc->free(alloc->ctx, t->thought_text, t->thought_text_len + 1);
    }
    if (store->items)
        alloc->free(alloc->ctx, store->items, store->capacity * sizeof(hu_inner_thought_t));
    store->items = NULL;
    store->count = 0;
    store->capacity = 0;
}

hu_error_t hu_inner_thought_accumulate(hu_inner_thought_store_t *store, const char *contact_id,
                                       size_t contact_id_len, const char *topic, size_t topic_len,
                                       const char *text, size_t text_len, double relevance,
                                       uint64_t now_ms) {
    if (!store || !store->alloc || !contact_id || contact_id_len == 0 || !text || text_len == 0)
        return HU_ERR_INVALID_ARGUMENT;
    if (relevance < 0.0 || relevance > 1.0)
        return HU_ERR_INVALID_ARGUMENT;
    HU_ASSERT_NOT_REENTRANT(thought_accumulate);

    /* Evict oldest entry if at max capacity */
    if (store->count >= HU_INNER_THOUGHT_MAX_CAPACITY) {
        size_t oldest_idx = 0;
        uint64_t oldest_ts = store->items[0].accumulated_at;
        for (size_t i = 1; i < store->count; i++) {
            if (store->items[i].accumulated_at < oldest_ts) {
                oldest_ts = store->items[i].accumulated_at;
                oldest_idx = i;
            }
        }
        hu_allocator_t *a = store->alloc;
        hu_inner_thought_t *evict = &store->items[oldest_idx];
        if (evict->contact_id)
            a->free(a->ctx, evict->contact_id, evict->contact_id_len + 1);
        if (evict->topic)
            a->free(a->ctx, evict->topic, evict->topic_len + 1);
        if (evict->thought_text)
            a->free(a->ctx, evict->thought_text, evict->thought_text_len + 1);
        /* Shift remaining entries down */
        if (oldest_idx < store->count - 1) {
            memmove(&store->items[oldest_idx], &store->items[oldest_idx + 1],
                    (store->count - oldest_idx - 1) * sizeof(hu_inner_thought_t));
        }
        memset(&store->items[store->count - 1], 0, sizeof(hu_inner_thought_t));
        store->count--;
    }

    /* Grow if needed */
    if (store->count >= store->capacity) {
        size_t new_cap = store->capacity * 2;
        if (new_cap > HU_INNER_THOUGHT_MAX_CAPACITY)
            new_cap = HU_INNER_THOUGHT_MAX_CAPACITY;
        hu_allocator_t *alloc = store->alloc;
        hu_inner_thought_t *new_items = (hu_inner_thought_t *)alloc->realloc(
            alloc->ctx, store->items, store->capacity * sizeof(hu_inner_thought_t),
            new_cap * sizeof(hu_inner_thought_t));
        if (!new_items) {
            HU_LEAVE_NOT_REENTRANT(thought_accumulate);
            return HU_ERR_OUT_OF_MEMORY;
        }
        memset(new_items + store->capacity, 0,
               (new_cap - store->capacity) * sizeof(hu_inner_thought_t));
        store->items = new_items;
        store->capacity = new_cap;
    }

    hu_allocator_t *alloc = store->alloc;
    hu_inner_thought_t *t = &store->items[store->count];

    t->contact_id = hu_strndup(alloc, contact_id, contact_id_len);
    if (!t->contact_id) {
        HU_LEAVE_NOT_REENTRANT(thought_accumulate);
        return HU_ERR_OUT_OF_MEMORY;
    }
    t->contact_id_len = contact_id_len;

    if (topic && topic_len > 0) {
        t->topic = hu_strndup(alloc, topic, topic_len);
        if (!t->topic) {
            alloc->free(alloc->ctx, t->contact_id, contact_id_len + 1);
            t->contact_id = NULL;
            HU_LEAVE_NOT_REENTRANT(thought_accumulate);
            return HU_ERR_OUT_OF_MEMORY;
        }
        t->topic_len = topic_len;
    } else {
        t->topic = NULL;
        t->topic_len = 0;
    }

    t->thought_text = hu_strndup(alloc, text, text_len);
    if (!t->thought_text) {
        alloc->free(alloc->ctx, t->contact_id, contact_id_len + 1);
        t->contact_id = NULL;
        if (t->topic) {
            alloc->free(alloc->ctx, t->topic, topic_len + 1);
            t->topic = NULL;
        }
        HU_LEAVE_NOT_REENTRANT(thought_accumulate);
        return HU_ERR_OUT_OF_MEMORY;
    }
    t->thought_text_len = text_len;

    t->relevance_score = relevance;
    t->accumulated_at = now_ms;
    t->surfaced = false;
    store->count++;
    HU_LEAVE_NOT_REENTRANT(thought_accumulate);
    return HU_OK;
}

bool hu_inner_thought_should_surface(const hu_inner_thought_t *thought, const char *context_topic,
                                     size_t context_topic_len, uint64_t now_ms) {
    if (!thought || thought->surfaced)
        return false;

    /* Stale check: don't surface thoughts older than STALE_DAYS */
    uint64_t age_ms = (now_ms > thought->accumulated_at) ? (now_ms - thought->accumulated_at) : 0;
    uint64_t stale_ms = (uint64_t)HU_INNER_THOUGHT_STALE_DAYS * MS_PER_DAY;
    if (age_ms > stale_ms)
        return false;

    /* Low relevance thoughts suppressed */
    if (thought->relevance_score < 0.3)
        return false;

    /* Topic match boosts — if context_topic matches thought topic, always surface (if not stale) */
    if (context_topic && context_topic_len > 0 && thought->topic && thought->topic_len > 0) {
        if (thought->topic_len <= context_topic_len) {
            /* Case-insensitive substring check */
            for (size_t i = 0; i + thought->topic_len <= context_topic_len; i++) {
                bool match = true;
                for (size_t j = 0; j < thought->topic_len; j++) {
                    char a = context_topic[i + j];
                    char b = thought->topic[j];
                    if (a >= 'A' && a <= 'Z')
                        a = (char)(a + 32);
                    if (b >= 'A' && b <= 'Z')
                        b = (char)(b + 32);
                    if (a != b) {
                        match = false;
                        break;
                    }
                }
                if (match)
                    return true;
            }
        }
    }

    /* Without topic match, only surface high-relevance thoughts */
    return thought->relevance_score >= 0.6;
}

size_t hu_inner_thought_surface(hu_inner_thought_store_t *store, const char *contact_id,
                                size_t contact_id_len, const char *context_topic,
                                size_t context_topic_len, uint64_t now_ms,
                                hu_inner_thought_t **surfaced, size_t max_count) {
    if (!store || !contact_id || contact_id_len == 0 || !surfaced || max_count == 0)
        return 0;
    HU_ASSERT_NOT_REENTRANT(thought_surface);

    /* Collect eligible thoughts for this contact, sorted by relevance */
    size_t eligible_count = 0;
    size_t eligible_indices[HU_INNER_THOUGHT_MAX_SURFACE * 4]; /* over-provision for selection */
    size_t max_eligible = sizeof(eligible_indices) / sizeof(eligible_indices[0]);

    for (size_t i = 0; i < store->count && eligible_count < max_eligible; i++) {
        hu_inner_thought_t *t = &store->items[i];
        if (t->contact_id_len != contact_id_len)
            continue;
        if (memcmp(t->contact_id, contact_id, contact_id_len) != 0)
            continue;
        if (!hu_inner_thought_should_surface(t, context_topic, context_topic_len, now_ms))
            continue;
        eligible_indices[eligible_count++] = i;
    }

    if (eligible_count == 0) {
        HU_LEAVE_NOT_REENTRANT(thought_surface);
        return 0;
    }

    /* Simple selection sort by relevance descending, pick top max_count */
    for (size_t i = 0; i < eligible_count && i < max_count; i++) {
        size_t best = i;
        for (size_t j = i + 1; j < eligible_count; j++) {
            if (store->items[eligible_indices[j]].relevance_score >
                store->items[eligible_indices[best]].relevance_score) {
                best = j;
            }
        }
        if (best != i) {
            size_t tmp = eligible_indices[i];
            eligible_indices[i] = eligible_indices[best];
            eligible_indices[best] = tmp;
        }
    }

    size_t result_count = eligible_count < max_count ? eligible_count : max_count;
    for (size_t i = 0; i < result_count; i++) {
        hu_inner_thought_t *t = &store->items[eligible_indices[i]];
        t->surfaced = true;
        surfaced[i] = t;
    }
    HU_LEAVE_NOT_REENTRANT(thought_surface);
    return result_count;
}

size_t hu_inner_thought_count_pending(const hu_inner_thought_store_t *store, const char *contact_id,
                                      size_t contact_id_len) {
    if (!store || !contact_id || contact_id_len == 0)
        return 0;
    size_t count = 0;
    for (size_t i = 0; i < store->count; i++) {
        const hu_inner_thought_t *t = &store->items[i];
        if (t->surfaced)
            continue;
        if (t->contact_id_len != contact_id_len)
            continue;
        if (memcmp(t->contact_id, contact_id, contact_id_len) != 0)
            continue;
        count++;
    }
    return count;
}
