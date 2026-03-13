#include "human/agent/speculative.h"
#include "human/core/string.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void prediction_clear(hu_prediction_t *p, hu_allocator_t *alloc) {
    if (p->query) {
        hu_str_free(alloc, p->query);
        p->query = NULL;
    }
    p->query_len = 0;
    if (p->response) {
        hu_str_free(alloc, p->response);
        p->response = NULL;
    }
    p->response_len = 0;
    p->confidence = 0.0;
    p->created_at = 0;
}

hu_error_t hu_speculative_cache_init(hu_speculative_cache_t *cache, hu_allocator_t *alloc) {
    if (!cache || !alloc)
            return HU_ERR_INVALID_ARGUMENT;
    memset(cache, 0, sizeof(*cache));
    cache->alloc = alloc;
    return HU_OK;
}

void hu_speculative_cache_deinit(hu_speculative_cache_t *cache) {
    if (!cache)
        return;
    for (size_t i = 0; i < cache->count; i++)
        prediction_clear(&cache->entries[i], cache->alloc);
    cache->count = 0;
}

static int tolower_strcmp(const char *a, size_t a_len, const char *b, size_t b_len) {
    size_t n = a_len < b_len ? a_len : b_len;
    for (size_t i = 0; i < n; i++) {
        int ca = (unsigned char)tolower((unsigned char)a[i]);
        int cb = (unsigned char)tolower((unsigned char)b[i]);
        if (ca != cb)
            return ca - cb;
    }
    return (int)(a_len - b_len);
}

static bool prefix_match(const char *query, size_t query_len,
                         const char *entry_query, size_t entry_len) {
    if (query_len == 0 || entry_len == 0)
        return false;
    size_t n = query_len < entry_len ? query_len : entry_len;
    return tolower_strcmp(query, n, entry_query, n) == 0;
}

static size_t count_words(const char *s, size_t len) {
    size_t n = 0;
    bool in_word = false;
    for (size_t i = 0; i < len; i++) {
        if (isspace((unsigned char)s[i]) || s[i] == '\0') {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            n++;
        }
    }
    return n;
}

static size_t shared_word_count(const char *query, size_t query_len,
                                const char *entry_query, size_t entry_len) {
    size_t shared = 0;
    const char *p = query;
    const char *end = query + query_len;
    while (p < end) {
        while (p < end && isspace((unsigned char)*p))
            p++;
        if (p >= end)
            break;
        const char *word_start = p;
        while (p < end && !isspace((unsigned char)*p) && *p != '\0')
            p++;
        size_t wlen = (size_t)(p - word_start);
        if (wlen == 0)
            continue;
        /* Check if this word appears in entry_query (case-insensitive) */
        const char *eq = entry_query;
        const char *eq_end = entry_query + entry_len;
        while (eq < eq_end) {
            while (eq < eq_end && isspace((unsigned char)*eq))
                eq++;
            if (eq >= eq_end)
                break;
            const char *ew_start = eq;
            while (eq < eq_end && !isspace((unsigned char)*eq) && *eq != '\0')
                eq++;
            size_t ewlen = (size_t)(eq - ew_start);
            if (ewlen == wlen && tolower_strcmp(word_start, wlen, ew_start, ewlen) == 0) {
                shared++;
                break;
            }
        }
    }
    return shared;
}

static double word_similarity(const char *query, size_t query_len,
                              const char *entry_query, size_t entry_len) {
    size_t total = count_words(query, query_len);
    if (total == 0)
        return 1.0;
    size_t shared = shared_word_count(query, query_len, entry_query, entry_len);
    return (double)shared / (double)total;
}

hu_error_t hu_speculative_cache_store(hu_speculative_cache_t *cache,
                                      const char *query, size_t query_len,
                                      const char *response, size_t response_len,
                                      double confidence, int64_t now_ts) {
    if (!cache || !cache->alloc)
        return HU_ERR_INVALID_ARGUMENT;
    if (!query || !response)
        return HU_ERR_INVALID_ARGUMENT;

    if (cache->count >= HU_SPEC_MAX_CACHE) {
        /* Evict oldest by created_at */
        size_t oldest_idx = 0;
        int64_t oldest_ts = cache->entries[0].created_at;
        for (size_t i = 1; i < cache->count; i++) {
            if (cache->entries[i].created_at < oldest_ts) {
                oldest_ts = cache->entries[i].created_at;
                oldest_idx = i;
            }
        }
        prediction_clear(&cache->entries[oldest_idx], cache->alloc);
        memmove(&cache->entries[oldest_idx], &cache->entries[oldest_idx + 1],
               (cache->count - 1 - oldest_idx) * sizeof(hu_prediction_t));
        cache->count--;
    }

    hu_prediction_t *p = &cache->entries[cache->count];
    p->query = hu_strndup(cache->alloc, query, query_len);
    if (!p->query)
        return HU_ERR_OUT_OF_MEMORY;
    p->query_len = query_len;
    p->response = hu_strndup(cache->alloc, response, response_len);
    if (!p->response) {
        hu_str_free(cache->alloc, p->query);
        p->query = NULL;
        return HU_ERR_OUT_OF_MEMORY;
    }
    p->response_len = response_len;
    p->confidence = confidence;
    p->created_at = now_ts;
    cache->count++;
    return HU_OK;
}

hu_error_t hu_speculative_cache_lookup(hu_speculative_cache_t *cache,
                                       const char *query, size_t query_len,
                                       int64_t now_ts,
                                       const hu_speculative_config_t *config,
                                       hu_prediction_t **out) {
    if (!cache || !config || !out)
        return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;

    cache->total_lookups++;
    int64_t ttl = config->ttl_seconds > 0 ? config->ttl_seconds : 300;

    for (size_t i = 0; i < cache->count; i++) {
        hu_prediction_t *p = &cache->entries[i];
        if (p->created_at <= 0 || (now_ts - p->created_at) > ttl)
            continue;
        if (p->confidence < config->min_confidence)
            continue;

        bool prefix_ok = prefix_match(query, query_len, p->query, p->query_len)
                     || prefix_match(p->query, p->query_len, query, query_len);
        double sim = word_similarity(query, query_len, p->query, p->query_len);

        if ((prefix_ok || sim >= 0.7) && sim >= 0.7) {
            cache->total_hits++;
            cache->hit_rate = (double)cache->total_hits / (double)cache->total_lookups;
            *out = p;
            return HU_OK;
        }
    }
    cache->hit_rate = (double)cache->total_hits / (double)cache->total_lookups;
    return HU_ERR_NOT_FOUND;
}

void hu_speculative_cache_evict(hu_speculative_cache_t *cache, int64_t now_ts,
                                int64_t ttl_seconds) {
    if (!cache || ttl_seconds <= 0)
        return;
    size_t write = 0;
    for (size_t i = 0; i < cache->count; i++) {
        if ((now_ts - cache->entries[i].created_at) <= ttl_seconds) {
            if (write != i)
                cache->entries[write] = cache->entries[i];
            write++;
        } else {
            prediction_clear(&cache->entries[i], cache->alloc);
        }
    }
    cache->count = write;
}

static bool response_has_list(const char *response, size_t len) {
    const char *p = response;
    const char *end = response + len;
    int num_count = 0;
    while (p < end) {
        if (*p == '\n' && p + 1 < end && (p[1] == '-' || p[1] == '*' ||
            (p[1] >= '0' && p[1] <= '9')))
            num_count++;
        if (num_count >= 2)
            return true;
        p++;
    }
    return false;
}

static bool response_is_howto(const char *response, size_t len) {
    const char *howto[] = {"how to", "step 1", "first,", "then,", "finally,"};
    for (size_t i = 0; i < sizeof(howto) / sizeof(howto[0]); i++) {
        size_t n = strlen(howto[i]);
        if (len >= n) {
            int match = 1;
            for (size_t j = 0; j < n; j++) {
                if (tolower((unsigned char)response[j]) != (unsigned char)howto[i][j]) {
                    match = 0;
                    break;
                }
            }
            if (match)
                return true;
        }
    }
    return false;
}

static const char *first_list_item(const char *response, size_t len, size_t *out_len) {
    const char *p = response;
    const char *end = response + len;
    while (p < end) {
        if (*p == '\n' && p + 1 < end) {
            p++;
            if (*p == '-' || *p == '*' || (*p >= '0' && *p <= '9')) {
                p++;
                while (p < end && (*p == ' ' || *p == '.'))
                    p++;
                const char *start = p;
                while (p < end && *p != '\n' && *p != '\0')
                    p++;
                if (p > start) {
                    *out_len = (size_t)(p - start);
                    return start;
                }
            }
        } else {
            p++;
        }
    }
    return NULL;
}

hu_error_t hu_speculative_predict(hu_allocator_t *alloc,
                                   const char *user_msg, size_t user_msg_len,
                                   const char *response, size_t response_len,
                                   char **predictions, size_t *prediction_lens,
                                   double *confidences, size_t max_predictions,
                                   size_t *actual_count) {
    (void)user_msg;
    (void)user_msg_len;
    if (!alloc || !predictions || !prediction_lens || !confidences || !actual_count)
        return HU_ERR_INVALID_ARGUMENT;
    *actual_count = 0;
    if (max_predictions == 0 || !response)
        return HU_OK;

    size_t n = 0;

    /* Always predict summary request */
    if (n < max_predictions) {
        const char *sum = "can you summarize that?";
        predictions[n] = hu_strndup(alloc, sum, strlen(sum));
        if (predictions[n]) {
            prediction_lens[n] = strlen(sum);
            confidences[n] = 0.5;
            n++;
        }
    }

    /* If response mentions a list, predict "tell me more about <first item>" */
    if (n < max_predictions && response_has_list(response, response_len)) {
        size_t item_len = 0;
        const char *item = first_list_item(response, response_len, &item_len);
        if (item && item_len > 0 && item_len < 64) {
            char buf[128];
            int written = snprintf(buf, sizeof(buf), "tell me more about %.*s",
                                  (int)item_len, item);
            if (written > 0 && (size_t)written < sizeof(buf)) {
                predictions[n] = hu_strndup(alloc, buf, (size_t)written);
                if (predictions[n]) {
                    prediction_lens[n] = (size_t)written;
                    confidences[n] = 0.7;
                    n++;
                }
            }
        }
    }

    /* If response is a how-to, predict "what if that doesn't work?" */
    if (n < max_predictions && response_is_howto(response, response_len)) {
        const char *q = "what if that doesn't work?";
        predictions[n] = hu_strndup(alloc, q, strlen(q));
        if (predictions[n]) {
            prediction_lens[n] = strlen(q);
            confidences[n] = 0.6;
            n++;
        }
    }

    *actual_count = n;
    return HU_OK;
}

hu_speculative_config_t hu_speculative_config_default(void) {
    return (hu_speculative_config_t){
        .enabled = true,
        .min_confidence = 0.6,
        .ttl_seconds = 300,
        .max_predictions = 2,
    };
}
