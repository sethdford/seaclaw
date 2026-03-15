/*
 * Feed trend detection — topic volume analysis and spike detection.
 * Compares recent item counts per keyword against historical baselines
 * to identify trending topics for the research agent.
 */
#include "human/feeds/trends.h"
#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

hu_error_t hu_feed_detect_trends(hu_allocator_t *alloc, sqlite3 *db,
                                 const char *keywords, size_t keywords_len,
                                 hu_feed_trend_t **out, size_t *out_count) {
    if (!alloc || !db || !keywords || !out || !out_count)
        return HU_ERR_INVALID_ARGUMENT;
    (void)keywords_len;
    *out = NULL;
    *out_count = 0;

    int64_t now = (int64_t)time(NULL);
    int64_t since_6h = now - 6 * 3600;
    int64_t since_24h = now - 24 * 3600;
    int64_t since_7d = now - 7 * 86400;

    const char *count_sql =
        "SELECT COUNT(*) FROM feed_items WHERE ingested_at >= ? AND content LIKE ?";

    hu_feed_trend_t *trends = NULL;
    size_t count = 0, cap = 0;

    const char *kw = keywords;
    while (*kw) {
        while (*kw == ' ') kw++;
        if (!*kw) break;
        const char *end = kw;
        while (*end && *end != ' ') end++;
        size_t kwlen = (size_t)(end - kw);
        if (kwlen < 2) { kw = end; continue; }

        char like_pat[256];
        if (kwlen + 3 > sizeof(like_pat)) { kw = end; continue; }
        like_pat[0] = '%';
        memcpy(like_pat + 1, kw, kwlen);
        like_pat[kwlen + 1] = '%';
        like_pat[kwlen + 2] = '\0';

        int64_t windows[] = { since_6h, since_24h, since_7d };
        int counts[3] = {0, 0, 0};

        for (int w = 0; w < 3; w++) {
            sqlite3_stmt *stmt = NULL;
            if (sqlite3_prepare_v2(db, count_sql, -1, &stmt, NULL) != SQLITE_OK)
                continue;
            sqlite3_bind_int64(stmt, 1, windows[w]);
            sqlite3_bind_text(stmt, 2, like_pat, -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) == SQLITE_ROW)
                counts[w] = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
        }

        int count_6h = counts[0];
        int count_24h = counts[1];
        int count_7d = counts[2];

        double avg_6h_in_24h = (count_24h > 0) ? (double)count_24h / 4.0 : 0.0;
        double avg_6h_in_7d = (count_7d > 0) ? (double)count_7d / 28.0 : 0.0;
        double baseline = (avg_6h_in_24h > avg_6h_in_7d) ? avg_6h_in_24h : avg_6h_in_7d;

        double spike_ratio = (baseline > 0.5) ? (double)count_6h / baseline : 0.0;

        if (count_6h >= 2 && spike_ratio >= 1.5) {
            if (count >= cap) {
                size_t nc = cap == 0 ? 8 : cap * 2;
                hu_feed_trend_t *tmp = (hu_feed_trend_t *)alloc->alloc(
                    alloc->ctx, nc * sizeof(hu_feed_trend_t));
                if (!tmp) {
                    if (trends) alloc->free(alloc->ctx, trends, cap * sizeof(hu_feed_trend_t));
                    return HU_ERR_OUT_OF_MEMORY;
                }
                if (trends) {
                    memcpy(tmp, trends, count * sizeof(hu_feed_trend_t));
                    alloc->free(alloc->ctx, trends, cap * sizeof(hu_feed_trend_t));
                }
                trends = tmp;
                cap = nc;
            }
            hu_feed_trend_t *t = &trends[count];
            memset(t, 0, sizeof(*t));
            size_t copy_len = kwlen < sizeof(t->keyword) - 1 ? kwlen : sizeof(t->keyword) - 1;
            memcpy(t->keyword, kw, copy_len);
            t->keyword[copy_len] = '\0';
            t->count_6h = count_6h;
            t->count_24h = count_24h;
            t->count_7d = count_7d;
            t->spike_ratio = spike_ratio;
            count++;
        }

        kw = end;
    }

    *out = trends;
    *out_count = count;
    return HU_OK;
}

hu_error_t hu_feed_trends_build_section(hu_allocator_t *alloc,
                                        const hu_feed_trend_t *trends, size_t count,
                                        char **out, size_t *out_len) {
    if (!alloc || !out || !out_len) return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_len = 0;

    if (!trends || count == 0) {
        const char *msg = "## Trending Topics\n\n(No significant trends detected)\n\n";
        size_t len = strlen(msg);
        char *buf = (char *)alloc->alloc(alloc->ctx, len + 1);
        if (!buf) return HU_ERR_OUT_OF_MEMORY;
        memcpy(buf, msg, len + 1);
        *out = buf;
        *out_len = len;
        return HU_OK;
    }

    size_t need = 64;
    for (size_t i = 0; i < count; i++)
        need += strlen(trends[i].keyword) + 80;

    char *buf = (char *)alloc->alloc(alloc->ctx, need);
    if (!buf) return HU_ERR_OUT_OF_MEMORY;

    size_t pos = 0;
    int n = snprintf(buf + pos, need - pos, "## Trending Topics\n\n");
    if (n > 0) pos += (size_t)n;

    for (size_t i = 0; i < count && i < 10; i++) {
        n = snprintf(buf + pos, need - pos,
                     "- **%s**: %d mentions in 6h (%.1fx normal, %d in 24h, %d in 7d)\n",
                     trends[i].keyword, trends[i].count_6h, trends[i].spike_ratio,
                     trends[i].count_24h, trends[i].count_7d);
        if (n > 0) pos += (size_t)n;
    }
    n = snprintf(buf + pos, need - pos, "\n");
    if (n > 0) pos += (size_t)n;

    *out = buf;
    *out_len = pos;
    return HU_OK;
}

void hu_feed_trends_free(hu_allocator_t *alloc, hu_feed_trend_t *trends, size_t count) {
    if (trends && alloc)
        alloc->free(alloc->ctx, trends, count * sizeof(hu_feed_trend_t));
}

#endif /* HU_ENABLE_SQLITE */
