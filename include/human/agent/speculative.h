#ifndef HU_AGENT_SPECULATIVE_H
#define HU_AGENT_SPECULATIVE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Speculative Pre-Computation — predicts likely follow-up queries and
 * pre-generates responses for instant delivery. Caches predictions
 * with confidence scores; hits bypass the full agent turn.
 */

#define HU_SPEC_MAX_PREDICTIONS 3
#define HU_SPEC_MAX_CACHE 8

typedef struct hu_prediction {
    char *query;
    size_t query_len;
    char *response;
    size_t response_len;
    double confidence;   /* 0.0-1.0 */
    int64_t created_at;
} hu_prediction_t;

typedef struct hu_speculative_cache {
    hu_allocator_t *alloc;
    hu_prediction_t entries[HU_SPEC_MAX_CACHE];
    size_t count;
    double hit_rate;
    size_t total_lookups;
    size_t total_hits;
} hu_speculative_cache_t;

typedef struct hu_speculative_config {
    bool enabled;
    double min_confidence;  /* only cache predictions above this (default 0.6) */
    int64_t ttl_seconds;    /* expire predictions after this long (default 300) */
    int max_predictions;    /* how many follow-ups to predict (default 2) */
} hu_speculative_config_t;

hu_error_t hu_speculative_cache_init(hu_speculative_cache_t *cache, hu_allocator_t *alloc);
void hu_speculative_cache_deinit(hu_speculative_cache_t *cache);

/* Store predicted follow-up queries with pre-generated responses. */
hu_error_t hu_speculative_cache_store(hu_speculative_cache_t *cache,
                                      const char *query, size_t query_len,
                                      const char *response, size_t response_len,
                                      double confidence, int64_t now_ts);

/* Look up a query in the cache. Returns HU_OK and sets *out if found.
 * out->response is owned by the cache — caller must NOT free it. */
hu_error_t hu_speculative_cache_lookup(hu_speculative_cache_t *cache,
                                       const char *query, size_t query_len,
                                       int64_t now_ts,
                                       const hu_speculative_config_t *config,
                                       hu_prediction_t **out);

/* Evict expired entries. */
void hu_speculative_cache_evict(hu_speculative_cache_t *cache, int64_t now_ts,
                                 int64_t ttl_seconds);

/* Generate follow-up predictions from a conversation turn.
 * Predicts what the user might ask next. Returns predicted queries
 * (not full responses — responses would be pre-computed separately). */
hu_error_t hu_speculative_predict(hu_allocator_t *alloc,
                                   const char *user_msg, size_t user_msg_len,
                                   const char *response, size_t response_len,
                                   char **predictions, size_t *prediction_lens,
                                   double *confidences, size_t max_predictions,
                                   size_t *actual_count);

hu_speculative_config_t hu_speculative_config_default(void);

#endif
