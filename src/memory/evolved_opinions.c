#include "human/memory/evolved_opinions.h"

#ifdef HU_ENABLE_SQLITE

#include "human/core/string.h"
#include <string.h>

hu_error_t hu_evolved_opinions_ensure_table(sqlite3 *db) {
    if (!db)
        return HU_ERR_INVALID_ARGUMENT;
    const char *sql = "CREATE TABLE IF NOT EXISTS evolved_opinions ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "topic TEXT NOT NULL UNIQUE, "
                      "stance TEXT NOT NULL, "
                      "conviction REAL NOT NULL DEFAULT 0.5, "
                      "formed_at INTEGER NOT NULL, "
                      "interactions INTEGER NOT NULL DEFAULT 1, "
                      "updated_at INTEGER NOT NULL)";
    char *errmsg = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        if (errmsg)
            sqlite3_free(errmsg);
        return HU_ERR_IO;
    }
    return HU_OK;
}

hu_error_t hu_evolved_opinion_upsert(sqlite3 *db, const char *topic, size_t topic_len,
                                     const char *stance, size_t stance_len, double conviction,
                                     int64_t now_ts) {
    if (!db || !topic || topic_len == 0 || !stance || stance_len == 0)
        return HU_ERR_INVALID_ARGUMENT;
    if (conviction < 0.0)
        conviction = 0.0;
    if (conviction > 1.0)
        conviction = 1.0;

    const char *sql = "INSERT INTO evolved_opinions (topic, stance, conviction, formed_at, "
                      "interactions, updated_at) "
                      "VALUES (?1, ?2, ?3, ?4, 1, ?4) "
                      "ON CONFLICT(topic) DO UPDATE SET "
                      "stance = ?2, "
                      "conviction = (conviction + ?3) / 2.0, "
                      "interactions = interactions + 1, "
                      "updated_at = ?4";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_IO;

    sqlite3_bind_text(stmt, 1, topic, (int)topic_len, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, stance, (int)stance_len, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 3, conviction);
    sqlite3_bind_int64(stmt, 4, now_ts);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_IO;
}

/* Find the end of the current sentence (period, newline, or end of string) */
static size_t find_sentence_end(const char *s, size_t start, size_t len) {
    for (size_t i = start; i < len; i++) {
        if (s[i] == '.' || s[i] == '\n' || s[i] == '!' || s[i] == '?')
            return i;
    }
    return len;
}

/* Extract a short topic slug from the first few words after the opinion marker */
static size_t extract_topic(const char *s, size_t start, size_t len, char *out, size_t out_cap) {
    size_t pos = start;
    /* Skip leading whitespace */
    while (pos < len && (s[pos] == ' ' || s[pos] == '\t'))
        pos++;
    /* Skip common filler words: "that", "the", "a", "an" */
    static const char *fillers[] = {"that ", "the ", "a ", "an "};
    for (size_t f = 0; f < 4; f++) {
        size_t fl = strlen(fillers[f]);
        if (pos + fl <= len && strncasecmp(s + pos, fillers[f], fl) == 0) {
            pos += fl;
            break;
        }
    }
    /* Take up to 5 words or 60 chars */
    size_t word_count = 0;
    size_t end = pos;
    while (end < len && word_count < 5 && (end - pos) < 60) {
        if (s[end] == ' ')
            word_count++;
        if (s[end] == '.' || s[end] == ',' || s[end] == '\n')
            break;
        end++;
    }
    /* Trim trailing whitespace */
    while (end > pos && s[end - 1] == ' ')
        end--;
    size_t tlen = end - pos;
    if (tlen == 0 || tlen >= out_cap)
        return 0;
    memcpy(out, s + pos, tlen);
    out[tlen] = '\0';
    return tlen;
}

hu_error_t hu_evolved_opinions_extract_and_store(sqlite3 *db, const char *response,
                                                 size_t response_len, int64_t now_ts) {
    if (!db || !response || response_len == 0)
        return HU_ERR_INVALID_ARGUMENT;

    static const struct {
        const char *phrase;
        size_t len;
    } markers[] = {
        {"I think ", 8},        {"I believe ", 10},
        {"I prefer ", 9},       {"I'd argue ", 10},
        {"I feel like ", 12},   {"my take is ", 11},
        {"in my opinion ", 14}, {"I've come to believe ", 21},
        {"I'm convinced ", 14}, {"honestly, I ", 12},
    };
    static const size_t marker_count = sizeof(markers) / sizeof(markers[0]);

    hu_evolved_opinions_ensure_table(db);
    size_t stored = 0;

    for (size_t i = 0; i + 8 <= response_len && stored < 3; i++) {
        for (size_t m = 0; m < marker_count; m++) {
            if (i + markers[m].len > response_len)
                continue;
            if (strncasecmp(response + i, markers[m].phrase, markers[m].len) != 0)
                continue;

            size_t after = i + markers[m].len;
            size_t sent_end = find_sentence_end(response, after, response_len);

            char topic[64];
            size_t topic_len = extract_topic(response, after, response_len, topic, sizeof(topic));
            if (topic_len < 3) {
                i = sent_end;
                break;
            }

            /* Stance = everything from the marker to end of sentence */
            size_t stance_start = i;
            size_t stance_len = sent_end - stance_start;
            if (stance_len < 10 || stance_len > 300) {
                i = sent_end;
                break;
            }

            hu_evolved_opinion_upsert(db, topic, topic_len, response + stance_start, stance_len,
                                      0.5, now_ts);
            stored++;
            i = sent_end;
            break;
        }
    }

    return HU_OK;
}

hu_error_t hu_evolved_opinions_get(hu_allocator_t *alloc, sqlite3 *db, double min_conviction,
                                   size_t limit, hu_evolved_opinion_t **out, size_t *out_count) {
    if (!alloc || !db || !out || !out_count)
        return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_count = 0;
    if (limit == 0)
        limit = 10;

    const char *sql = "SELECT topic, stance, conviction, formed_at, interactions "
                      "FROM evolved_opinions WHERE conviction >= ?1 "
                      "ORDER BY conviction DESC, interactions DESC LIMIT ?2";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_IO;

    sqlite3_bind_double(stmt, 1, min_conviction);
    sqlite3_bind_int(stmt, 2, (int)limit);

    hu_evolved_opinion_t *opinions = NULL;
    size_t count = 0;
    size_t cap = 0;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (count >= cap) {
            size_t new_cap = cap ? cap * 2 : 4;
            hu_evolved_opinion_t *arr = (hu_evolved_opinion_t *)alloc->alloc(
                alloc->ctx, new_cap * sizeof(hu_evolved_opinion_t));
            if (!arr) {
                sqlite3_finalize(stmt);
                hu_evolved_opinions_free(alloc, opinions, count);
                return HU_ERR_OUT_OF_MEMORY;
            }
            if (opinions) {
                memcpy(arr, opinions, count * sizeof(hu_evolved_opinion_t));
                alloc->free(alloc->ctx, opinions, cap * sizeof(hu_evolved_opinion_t));
            }
            opinions = arr;
            cap = new_cap;
        }

        const char *topic = (const char *)sqlite3_column_text(stmt, 0);
        int topic_len = sqlite3_column_bytes(stmt, 0);
        const char *stance = (const char *)sqlite3_column_text(stmt, 1);
        int stance_len = sqlite3_column_bytes(stmt, 1);

        hu_evolved_opinion_t *op = &opinions[count];
        memset(op, 0, sizeof(*op));
        op->topic = hu_strndup(alloc, topic, (size_t)topic_len);
        op->topic_len = (size_t)topic_len;
        op->stance = hu_strndup(alloc, stance, (size_t)stance_len);
        op->stance_len = (size_t)stance_len;
        op->conviction = sqlite3_column_double(stmt, 2);
        op->formed_at = sqlite3_column_int64(stmt, 3);
        op->interactions = (uint32_t)sqlite3_column_int(stmt, 4);
        count++;
    }

    sqlite3_finalize(stmt);

    /* Shrink to exact size */
    if (count > 0 && count < cap) {
        hu_evolved_opinion_t *shrunk =
            (hu_evolved_opinion_t *)alloc->alloc(alloc->ctx, count * sizeof(hu_evolved_opinion_t));
        if (shrunk) {
            memcpy(shrunk, opinions, count * sizeof(hu_evolved_opinion_t));
            alloc->free(alloc->ctx, opinions, cap * sizeof(hu_evolved_opinion_t));
            opinions = shrunk;
        }
    }

    *out = opinions;
    *out_count = count;
    return HU_OK;
}

void hu_evolved_opinions_free(hu_allocator_t *alloc, hu_evolved_opinion_t *opinions, size_t count) {
    if (!alloc || !opinions)
        return;
    for (size_t i = 0; i < count; i++) {
        if (opinions[i].topic)
            alloc->free(alloc->ctx, opinions[i].topic, opinions[i].topic_len + 1);
        if (opinions[i].stance)
            alloc->free(alloc->ctx, opinions[i].stance, opinions[i].stance_len + 1);
    }
    alloc->free(alloc->ctx, opinions, count * sizeof(hu_evolved_opinion_t));
}

#endif /* HU_ENABLE_SQLITE */
