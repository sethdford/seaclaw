#ifndef HU_FEEDS_TRENDS_H
#define HU_FEEDS_TRENDS_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>
#include <stdint.h>

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>

typedef struct hu_feed_trend {
    char keyword[64];
    int count_6h;
    int count_24h;
    int count_7d;
    double spike_ratio;
} hu_feed_trend_t;

hu_error_t hu_feed_detect_trends(hu_allocator_t *alloc, sqlite3 *db,
                                 const char *keywords, size_t keywords_len,
                                 hu_feed_trend_t **out, size_t *out_count);

hu_error_t hu_feed_trends_build_section(hu_allocator_t *alloc,
                                        const hu_feed_trend_t *trends, size_t count,
                                        char **out, size_t *out_len);

void hu_feed_trends_free(hu_allocator_t *alloc, hu_feed_trend_t *trends, size_t count);

#endif /* HU_ENABLE_SQLITE */
#endif
