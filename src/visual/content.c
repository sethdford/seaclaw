#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/string.h"
#include "human/visual/content.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>
#endif

#define HU_VISUAL_ESCAPE_BUF 1024
#define HU_VISUAL_SQL_BUF 4096

static void escape_sql_string(const char *s, size_t len, char *buf, size_t cap, size_t *out_len) {
    size_t pos = 0;
    for (size_t i = 0; i < len && pos + 2 < cap; i++) {
        if (s[i] == '\'') {
            buf[pos++] = '\'';
            buf[pos++] = '\'';
        } else {
            buf[pos++] = s[i];
        }
    }
    buf[pos] = '\0';
    *out_len = pos;
}

hu_error_t hu_visual_create_table_sql(char *buf, size_t cap, size_t *out_len) {
    if (!buf || !out_len || cap < 512)
        return HU_ERR_INVALID_ARGUMENT;
    static const char sql[] =
        "CREATE TABLE IF NOT EXISTS visual_content (\n"
        "    id INTEGER PRIMARY KEY,\n"
        "    source TEXT NOT NULL,\n"
        "    path TEXT NOT NULL,\n"
        "    description TEXT,\n"
        "    tags TEXT,\n"
        "    location TEXT,\n"
        "    captured_at INTEGER NOT NULL,\n"
        "    indexed_at INTEGER NOT NULL,\n"
        "    shared_with TEXT,\n"
        "    share_count INTEGER DEFAULT 0\n"
        ")";
    size_t len = sizeof(sql) - 1;
    if (len >= cap)
        return HU_ERR_INVALID_ARGUMENT;
    memcpy(buf, sql, len + 1);
    *out_len = len;
    return HU_OK;
}

hu_error_t hu_visual_insert_sql(const hu_visual_candidate_t *candidate, const char *source,
                                size_t source_len, char *buf, size_t cap, size_t *out_len) {
    if (!candidate || !buf || !out_len || cap < 256)
        return HU_ERR_INVALID_ARGUMENT;
    if (!candidate->path || !source)
        return HU_ERR_INVALID_ARGUMENT;

    char source_esc[HU_VISUAL_ESCAPE_BUF];
    char path_esc[HU_VISUAL_ESCAPE_BUF];
    char desc_esc[HU_VISUAL_ESCAPE_BUF];

    size_t se_len, pe_len, de_len;
    escape_sql_string(source, source_len, source_esc, sizeof(source_esc), &se_len);
    escape_sql_string(candidate->path, candidate->path_len, path_esc, sizeof(path_esc), &pe_len);
    if (candidate->description && candidate->description_len > 0) {
        escape_sql_string(candidate->description, candidate->description_len, desc_esc,
                          sizeof(desc_esc), &de_len);
    } else {
        desc_esc[0] = '\0';
        de_len = 0;
    }

    uint64_t idx_at = candidate->captured_at > 0 ? candidate->captured_at : 0;

    int n;
    if (de_len > 0) {
        n = snprintf(buf, cap,
                     "INSERT INTO visual_content (source, path, description, captured_at, "
                     "indexed_at, share_count) VALUES ('%s', '%s', '%s', %llu, %llu, 0)",
                     source_esc, path_esc, desc_esc, (unsigned long long)candidate->captured_at,
                     (unsigned long long)idx_at);
    } else {
        n = snprintf(buf, cap,
                     "INSERT INTO visual_content (source, path, description, captured_at, "
                     "indexed_at, share_count) VALUES ('%s', '%s', NULL, %llu, %llu, 0)",
                     source_esc, path_esc, (unsigned long long)candidate->captured_at,
                     (unsigned long long)idx_at);
    }
    if (n < 0 || (size_t)n >= cap)
        return HU_ERR_INVALID_ARGUMENT;
    *out_len = (size_t)n;
    return HU_OK;
}

hu_error_t hu_visual_query_recent_sql(uint64_t since_ms, char *buf, size_t cap, size_t *out_len) {
    if (!buf || !out_len || cap < 256)
        return HU_ERR_INVALID_ARGUMENT;
    int n = snprintf(buf, cap,
                     "SELECT id, source, path, description, captured_at, indexed_at, share_count "
                     "FROM visual_content WHERE captured_at > %llu ORDER BY captured_at DESC LIMIT "
                     "50",
                     (unsigned long long)since_ms);
    if (n < 0 || (size_t)n >= cap)
        return HU_ERR_INVALID_ARGUMENT;
    *out_len = (size_t)n;
    return HU_OK;
}

hu_error_t hu_visual_record_share_sql(int64_t content_id, const char *contact_id,
                                     size_t contact_id_len, char *buf, size_t cap,
                                     size_t *out_len) {
    (void)contact_id;
    (void)contact_id_len;
    if (!buf || !out_len || cap < 256)
        return HU_ERR_INVALID_ARGUMENT;

    int n = snprintf(buf, cap,
                     "UPDATE visual_content SET share_count = share_count + 1 WHERE id = %lld",
                     (long long)content_id);
    if (n < 0 || (size_t)n >= cap)
        return HU_ERR_INVALID_ARGUMENT;
    *out_len = (size_t)n;
    return HU_OK;
}

hu_error_t hu_visual_count_shares_today_sql(const char *contact_id, size_t contact_id_len,
                                            uint64_t today_start_ms, char *buf, size_t cap,
                                            size_t *out_len) {
    if (!buf || !out_len || cap < 256)
        return HU_ERR_INVALID_ARGUMENT;

    char contact_esc[HU_VISUAL_ESCAPE_BUF];
    size_t ce_len;
    escape_sql_string(contact_id ? contact_id : "", contact_id_len, contact_esc,
                      sizeof(contact_esc), &ce_len);

    int n = snprintf(buf, cap,
                     "SELECT COUNT(*) FROM visual_content WHERE shared_with LIKE '%%%s%%' AND "
                     "indexed_at > %llu",
                     contact_esc, (unsigned long long)today_start_ms);
    if (n < 0 || (size_t)n >= cap)
        return HU_ERR_INVALID_ARGUMENT;
    *out_len = (size_t)n;
    return HU_OK;
}

hu_visual_type_t hu_visual_decide(double closeness, uint32_t hour_of_day, uint32_t shares_today,
                                  uint32_t max_shares_day, bool contact_sends_photos,
                                  double share_probability, uint32_t seed) {
    if (shares_today >= max_shares_day)
        return HU_VISUAL_NONE;

    double prob = share_probability;
    if (closeness > 0.7)
        prob *= 2.0;
    if (hour_of_day >= 23 || hour_of_day <= 6)
        prob *= 0.5;
    if (!contact_sends_photos)
        prob *= 0.3;
    if (prob > 1.0)
        prob = 1.0;

    /* LCG: seed * 1103515245 + 12345 */
    uint32_t s = (uint32_t)((uint64_t)seed * 1103515245ULL + 12345ULL);
    double r = (double)(s % 65536) / 65536.0;

    if (r >= prob)
        return HU_VISUAL_NONE;

    /* Type selection: 50% PHOTO_WITH_TEXT, 30% LINK_WITH_TEXT, 20% PHOTO */
    uint32_t s2 = (uint32_t)((uint64_t)s * 1103515245ULL + 12345ULL);
    double r2 = (double)(s2 % 65536) / 65536.0;

    if (r2 < 0.5)
        return HU_VISUAL_PHOTO_WITH_TEXT;
    if (r2 < 0.8)
        return HU_VISUAL_LINK_WITH_TEXT;
    return HU_VISUAL_PHOTO;
}

static int tolower_char(unsigned char c) {
    return tolower(c);
}

static size_t next_word(const char *s, size_t len, size_t *start) {
    while (*start < len && !isalnum((unsigned char)s[*start]))
        (*start)++;
    size_t wstart = *start;
    while (*start < len && (isalnum((unsigned char)s[*start]) || s[*start] == '\''))
        (*start)++;
    return *start - wstart;
}

double hu_visual_score_relevance(const char *description, size_t desc_len, const char *topic,
                                 size_t topic_len) {
    if (!description || !topic || topic_len == 0)
        return 0.0;

    /* Tokenize topic into words, count matches in description (case-insensitive) */
    size_t topic_words = 0;
    size_t matches = 0;
    size_t tpos = 0;

    while (tpos < topic_len) {
        size_t wlen = next_word(topic, topic_len, &tpos);
        if (wlen == 0)
            break;
        topic_words++;
        /* Check if this word appears in description */
        const char *tw = topic + tpos - wlen;
        for (size_t d = 0; d + wlen <= desc_len; d++) {
            bool eq = true;
            for (size_t k = 0; k < wlen; k++) {
                if (tolower_char((unsigned char)description[d + k]) !=
                    tolower_char((unsigned char)tw[k])) {
                    eq = false;
                    break;
                }
            }
            if (eq) {
                matches++;
                break;
            }
        }
    }

    if (topic_words == 0)
        return 0.0;
    double score = (double)matches / (double)topic_words;
    return score > 1.0 ? 1.0 : score;
}

hu_error_t hu_visual_build_sharing_context(hu_allocator_t *alloc, hu_visual_type_t type,
                                          const char *description, size_t desc_len, char **out,
                                          size_t *out_len) {
    if (!alloc || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    (void)description;

    const char *ctx = NULL;
    switch (type) {
    case HU_VISUAL_PHOTO:
    case HU_VISUAL_PHOTO_WITH_TEXT:
        ctx = (desc_len > 0) ? "took this recently" : "check this out";
        break;
    case HU_VISUAL_LINK:
    case HU_VISUAL_LINK_WITH_TEXT:
        ctx = (desc_len > 0) ? "saw this and thought of you" : "thought you'd like this";
        break;
    case HU_VISUAL_SCREENSHOT:
        ctx = "look at this";
        break;
    default:
        ctx = "check this out";
        break;
    }

    size_t len = strlen(ctx);
    char *s = hu_strndup(alloc, ctx, len);
    if (!s)
        return HU_ERR_OUT_OF_MEMORY;
    *out = s;
    *out_len = len;
    return HU_OK;
}

hu_error_t hu_visual_build_prompt(hu_allocator_t *alloc,
                                  const hu_visual_candidate_t *candidates, size_t count, char **out,
                                  size_t *out_len) {
    if (!alloc || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

    if (count == 0) {
        char *s = hu_strndup(alloc, "[No visual content available to share]", 37);
        if (!s)
            return HU_ERR_OUT_OF_MEMORY;
        *out = s;
        *out_len = 37;
        return HU_OK;
    }

    char buf[4096];
    size_t pos = 0;
    pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "[VISUAL CONTENT AVAILABLE]:\n");

    for (size_t i = 0; i < count && pos < sizeof(buf) - 256; i++) {
        const hu_visual_candidate_t *c = &candidates[i];
        const char *type_str = hu_visual_type_str(c->type);
        const char *desc = c->description && c->description_len > 0 ? c->description : "(no desc)";
        size_t desc_len = c->description_len > 0 ? c->description_len : 7;
        const char *ctx =
            c->sharing_context && c->sharing_context_len > 0 ? c->sharing_context : "";
        size_t ctx_len = c->sharing_context_len;

        int n = snprintf(buf + pos, sizeof(buf) - pos, "%zu. %s: %.*s (relevance: %.2f) — \"%.*s\"\n",
                        i + 1, type_str, (int)desc_len, desc, c->relevance_score,
                        (int)ctx_len, ctx);
        if (n > 0 && pos + (size_t)n < sizeof(buf))
            pos += (size_t)n;
    }

    if (pos + 50 < sizeof(buf))
        pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos,
                                "\nYou may include one of these naturally if relevant.");

    char *s = hu_strndup(alloc, buf, pos);
    if (!s)
        return HU_ERR_OUT_OF_MEMORY;
    *out = s;
    *out_len = pos;
    return HU_OK;
}

const char *hu_visual_type_str(hu_visual_type_t type) {
    switch (type) {
    case HU_VISUAL_NONE:
        return "none";
    case HU_VISUAL_PHOTO:
        return "Photo";
    case HU_VISUAL_LINK:
        return "Link";
    case HU_VISUAL_SCREENSHOT:
        return "Screenshot";
    case HU_VISUAL_PHOTO_WITH_TEXT:
        return "Photo";
    case HU_VISUAL_LINK_WITH_TEXT:
        return "Link";
    default:
        return "unknown";
    }
}

void hu_visual_candidate_deinit(hu_allocator_t *alloc, hu_visual_candidate_t *c) {
    if (!alloc || !c)
        return;
    if (c->path) {
        alloc->free(alloc->ctx, c->path, c->path_len + 1);
        c->path = NULL;
        c->path_len = 0;
    }
    if (c->description) {
        alloc->free(alloc->ctx, c->description, c->description_len + 1);
        c->description = NULL;
        c->description_len = 0;
    }
    if (c->sharing_context) {
        alloc->free(alloc->ctx, c->sharing_context, c->sharing_context_len + 1);
        c->sharing_context = NULL;
        c->sharing_context_len = 0;
    }
}

void hu_visual_should_share(const hu_visual_entry_t *entry, const char *context,
                            size_t context_len, bool *should_share, double *confidence) {
    if (!entry || !should_share || !confidence) {
        if (should_share)
            *should_share = false;
        if (confidence)
            *confidence = 0.0;
        return;
    }
    double rel = 0.0;
    if (context && context_len > 0) {
        rel = hu_visual_score_relevance(entry->description,
                                         (size_t)strnlen(entry->description, sizeof(entry->description)),
                                         context, context_len);
        if (entry->tags[0] != '\0') {
            double tag_rel = hu_visual_score_relevance(entry->tags,
                                                       (size_t)strnlen(entry->tags, sizeof(entry->tags)),
                                                       context, context_len);
            if (tag_rel > rel)
                rel = tag_rel;
        }
    } else {
        rel = entry->relevance;
    }
    *confidence = rel;
    *should_share = (rel >= 0.3);
}

void hu_visual_entries_free(hu_allocator_t *alloc, hu_visual_entry_t *entries,
                            size_t count) {
    if (!alloc || !entries)
        return;
    alloc->free(alloc->ctx, entries, count * sizeof(hu_visual_entry_t));
}

#ifdef HU_ENABLE_SQLITE
static void copy_str_to_field(char *dst, size_t cap, const char *src, size_t src_len) {
    if (!src || src_len == 0) {
        dst[0] = '\0';
        return;
    }
    size_t copy_len = src_len;
    if (copy_len >= cap)
        copy_len = cap - 1;
    memcpy(dst, src, copy_len);
    dst[copy_len] = '\0';
}

hu_error_t hu_visual_scan_recent(hu_allocator_t *alloc, sqlite3 *db, uint64_t since_ms,
                                 hu_visual_entry_t **out, size_t *out_count) {
    if (!alloc || !db || !out || !out_count)
        return HU_ERR_INVALID_ARGUMENT;

    *out = NULL;
    *out_count = 0;

    static const char sql[] =
        "SELECT id, path, description, tags, captured_at FROM visual_content "
        "WHERE captured_at > ? ORDER BY captured_at DESC LIMIT 50";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return HU_ERR_IO;

    sqlite3_bind_int64(stmt, 1, (int64_t)since_ms);

    hu_visual_entry_t *entries = NULL;
    size_t count = 0;
    size_t cap = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= cap) {
            size_t new_cap = cap == 0 ? 8 : cap * 2;
            hu_visual_entry_t *tmp =
                (hu_visual_entry_t *)alloc->alloc(alloc->ctx, new_cap * sizeof(hu_visual_entry_t));
            if (!tmp) {
                if (entries)
                    alloc->free(alloc->ctx, entries, cap * sizeof(hu_visual_entry_t));
                sqlite3_finalize(stmt);
                return HU_ERR_OUT_OF_MEMORY;
            }
            if (entries) {
                memcpy(tmp, entries, count * sizeof(hu_visual_entry_t));
                alloc->free(alloc->ctx, entries, cap * sizeof(hu_visual_entry_t));
            }
            entries = tmp;
            cap = new_cap;
        }
        hu_visual_entry_t *e = &entries[count];
        memset(e, 0, sizeof(*e));

        e->rowid = sqlite3_column_int64(stmt, 0);
        const char *p = (const char *)sqlite3_column_text(stmt, 1);
        size_t pl = p ? (size_t)sqlite3_column_bytes(stmt, 1) : 0;
        copy_str_to_field(e->path, sizeof(e->path), p, pl);

        p = (const char *)sqlite3_column_text(stmt, 2);
        pl = p ? (size_t)sqlite3_column_bytes(stmt, 2) : 0;
        copy_str_to_field(e->description, sizeof(e->description), p, pl);

        p = (const char *)sqlite3_column_text(stmt, 3);
        pl = p ? (size_t)sqlite3_column_bytes(stmt, 3) : 0;
        copy_str_to_field(e->tags, sizeof(e->tags), p, pl);

        e->timestamp_ms = (uint64_t)sqlite3_column_int64(stmt, 4);
        e->relevance = 1.0; /* recency-only; no topic scoring in scan */
        count++;
    }
    sqlite3_finalize(stmt);

    *out = entries;
    *out_count = count;
    return HU_OK;
}

hu_error_t hu_visual_match_for_contact(hu_allocator_t *alloc, sqlite3 *db,
                                      const char *contact_id, size_t contact_id_len,
                                      const char *context, size_t context_len,
                                      hu_visual_entry_t **out, size_t *out_count) {
    (void)contact_id;
    (void)contact_id_len;
    if (!alloc || !db || !out || !out_count)
        return HU_ERR_INVALID_ARGUMENT;

    *out = NULL;
    *out_count = 0;

    /* Fetch recent entries (last 7 days). Time window: 7 * 24 * 60 * 60 * 1000 ms */
    uint64_t seven_days_ms = 7ULL * 24ULL * 60ULL * 60ULL * 1000ULL;
    uint64_t since_ms = 0;
    if (context && context_len > 0) {
        /* Use a fixed recent window; in production could use conversation timestamp */
        uint64_t now_ms = (uint64_t)time(NULL) * 1000ULL;
        if (now_ms > seven_days_ms)
            since_ms = now_ms - seven_days_ms;
    }

    hu_visual_entry_t *candidates = NULL;
    size_t cand_count = 0;
    hu_error_t err = hu_visual_scan_recent(alloc, db, since_ms, &candidates, &cand_count);
    if (err != HU_OK || cand_count == 0) {
        if (candidates)
            hu_visual_entries_free(alloc, candidates, cand_count);
        return HU_OK;
    }

    if (!context || context_len == 0) {
        *out = candidates;
        *out_count = cand_count;
        return HU_OK;
    }

    /* Score each by relevance to context (description + tags) */
    hu_visual_entry_t *matched = NULL;
    size_t cap = 0;
    size_t n = 0;

    for (size_t i = 0; i < cand_count; i++) {
        hu_visual_entry_t *e = &candidates[i];
        double desc_rel = hu_visual_score_relevance(
            e->description, (size_t)strnlen(e->description, sizeof(e->description)),
            context, context_len);
        double tag_rel = 0.0;
        if (e->tags[0] != '\0') {
            tag_rel = hu_visual_score_relevance(
                e->tags, (size_t)strnlen(e->tags, sizeof(e->tags)),
                context, context_len);
        }
        double rel = (desc_rel > tag_rel) ? desc_rel : tag_rel;
        if (rel < 0.1)
            continue;

        e->relevance = rel;
        if (n >= cap) {
            size_t new_cap = cap == 0 ? 4 : cap * 2;
            hu_visual_entry_t *tmp =
                (hu_visual_entry_t *)alloc->alloc(alloc->ctx, new_cap * sizeof(hu_visual_entry_t));
            if (!tmp) {
                hu_visual_entries_free(alloc, candidates, cand_count);
                if (matched)
                    hu_visual_entries_free(alloc, matched, cap);
                return HU_ERR_OUT_OF_MEMORY;
            }
            if (matched) {
                memcpy(tmp, matched, n * sizeof(hu_visual_entry_t));
                alloc->free(alloc->ctx, matched, cap * sizeof(hu_visual_entry_t));
            }
            matched = tmp;
            cap = new_cap;
        }
        matched[n++] = *e;
    }

    hu_visual_entries_free(alloc, candidates, cand_count);

    if (n == 0) {
        *out = NULL;
        *out_count = 0;
        return HU_OK;
    }

    /* Sort by relevance descending */
    for (size_t i = 0; i + 1 < n; i++) {
        for (size_t j = i + 1; j < n; j++) {
            if (matched[j].relevance > matched[i].relevance) {
                hu_visual_entry_t tmp = matched[i];
                matched[i] = matched[j];
                matched[j] = tmp;
            }
        }
    }

    *out = matched;
    *out_count = n;
    return HU_OK;
}
#endif /* HU_ENABLE_SQLITE */
