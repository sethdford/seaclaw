#include "human/cognition/episodic.h"

#ifdef HU_ENABLE_SQLITE

/* Max rows we materialize in memory per retrieve (SQL LIMIT must match allocation). */
#define HU_EPISODIC_MAX_RETRIEVE 16

#include "human/core/string.h"
#include "human/core/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

hu_error_t hu_episodic_init_schema(sqlite3 *db) {
    if (!db) return HU_ERR_INVALID_ARGUMENT;

    const char *ddl =
        "CREATE TABLE IF NOT EXISTS episodic_patterns ("
        "  id TEXT PRIMARY KEY,"
        "  problem_type TEXT NOT NULL,"
        "  approach TEXT NOT NULL,"
        "  skills_used TEXT DEFAULT '',"
        "  outcome_quality REAL DEFAULT 0.5,"
        "  support_count INTEGER DEFAULT 1,"
        "  insight TEXT DEFAULT '',"
        "  session_id TEXT DEFAULT '',"
        "  timestamp TEXT NOT NULL DEFAULT (datetime('now'))"
        ")";

    char *err_msg = NULL;
    int rc = sqlite3_exec(db, ddl, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg) sqlite3_free(err_msg);
        return HU_ERR_IO;
    }
    return HU_OK;
}

/* Generate a simple pseudo-UUID for pattern IDs. Not crypto-safe. */
static void generate_id(char *buf, size_t buf_size) {
#ifdef HU_IS_TEST
    static unsigned int test_counter = 0;
    (void)snprintf(buf, buf_size, "ep-test-%08u", test_counter++);
#else
    static unsigned int seed = 0;
    if (seed == 0) seed = (unsigned int)time(NULL);

    snprintf(buf, buf_size, "ep-%08x-%04x-%04x",
             (unsigned int)(rand_r(&seed)),
             (unsigned int)(rand_r(&seed) & 0xFFFF),
             (unsigned int)(rand_r(&seed) & 0xFFFF));
#endif
}

hu_error_t hu_episodic_store_pattern(sqlite3 *db, hu_allocator_t *alloc,
                                      const hu_episodic_pattern_t *p) {
    (void)alloc;
    if (!db || !p || !p->problem_type || !p->approach)
        return HU_ERR_INVALID_ARGUMENT;

    const char *sql =
        "INSERT OR REPLACE INTO episodic_patterns "
        "(id, problem_type, approach, skills_used, outcome_quality, "
        "support_count, insight, session_id) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return HU_ERR_IO;

    char id_buf[64];
    const char *id = p->id;
    if (!id || id[0] == '\0') {
        generate_id(id_buf, sizeof(id_buf));
        id = id_buf;
    }

    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, p->problem_type, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, p->approach, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, p->skills_used ? p->skills_used : "", -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 5, (double)p->outcome_quality);
    sqlite3_bind_int(stmt, 6, (int)p->support_count);
    sqlite3_bind_text(stmt, 7, p->insight ? p->insight : "", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, p->session_id ? p->session_id : "", -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_IO;
}

hu_error_t hu_episodic_extract_and_store(sqlite3 *db, hu_allocator_t *alloc,
                                          const hu_episodic_session_summary_t *summary) {
    if (!db || !alloc || !summary) return HU_ERR_INVALID_ARGUMENT;

    /* Store a pattern even for pure conversation turns when there was
     * meaningful emotional engagement (positive feedback or corrections).
     * This ensures emotional encounters persist as episodic memory. */
    if (summary->skill_count == 0 && summary->tool_count == 0 &&
        !summary->had_positive_feedback && !summary->had_correction)
        return HU_OK;

    /* Build skills_used string */
    char skills_buf[512] = "";
    size_t spos = 0;
    for (size_t i = 0; i < summary->skill_count && i < 10; i++) {
        if (i > 0) spos = hu_buf_appendf(skills_buf, sizeof(skills_buf), spos, ",");
        spos = hu_buf_appendf(skills_buf, sizeof(skills_buf), spos,
                              "%s", summary->skill_names[i]);
    }

    /* Build approach from tool names */
    char approach_buf[512] = "";
    size_t apos = 0;
    if (summary->tool_count > 0) {
        apos = hu_buf_appendf(approach_buf, sizeof(approach_buf), apos,
                              "Used tools: ");
        for (size_t i = 0; i < summary->tool_count && i < 8; i++) {
            if (i > 0) apos = hu_buf_appendf(approach_buf, sizeof(approach_buf), apos, ", ");
            apos = hu_buf_appendf(approach_buf, sizeof(approach_buf), apos,
                                  "%s", summary->tool_names[i]);
        }
    }
    if (summary->skill_count > 0) {
        if (apos > 0) apos = hu_buf_appendf(approach_buf, sizeof(approach_buf), apos, "; ");
        apos = hu_buf_appendf(approach_buf, sizeof(approach_buf), apos,
                              "Applied skills: %s", skills_buf);
    }
    if (summary->tool_count == 0 && summary->skill_count == 0) {
        if (summary->had_positive_feedback)
            apos = hu_buf_appendf(approach_buf, sizeof(approach_buf), apos,
                                  "Emotional encounter (positive connection)");
        else if (summary->had_correction)
            apos = hu_buf_appendf(approach_buf, sizeof(approach_buf), apos,
                                  "Emotional encounter (correction/growth)");
    }

    float quality = 0.5f;
    if (summary->had_positive_feedback) quality = 0.8f;
    if (summary->had_correction) quality = 0.3f;
    if (summary->had_positive_feedback && summary->had_correction) quality = 0.5f;

    hu_episodic_pattern_t pattern = {
        .id = NULL,
        .problem_type = (char *)(summary->topic ? summary->topic : "general"),
        .approach = approach_buf,
        .skills_used = skills_buf,
        .outcome_quality = quality,
        .support_count = 1,
        .insight = NULL,
        .session_id = (char *)(summary->session_id ? summary->session_id : ""),
        .timestamp = NULL,
    };

    return hu_episodic_store_pattern(db, alloc, &pattern);
}

static char *dup_col_text(hu_allocator_t *alloc, sqlite3_stmt *stmt, int col) {
    const char *text = (const char *)sqlite3_column_text(stmt, col);
    int len = sqlite3_column_bytes(stmt, col);
    if (!text || len <= 0) {
        char *empty = alloc->alloc(alloc->ctx, 1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    char *dup = alloc->alloc(alloc->ctx, (size_t)len + 1);
    if (dup) {
        memcpy(dup, text, (size_t)len);
        dup[len] = '\0';
    }
    return dup;
}

hu_error_t hu_episodic_retrieve(sqlite3 *db, hu_allocator_t *alloc,
                                 const char *query, size_t query_len,
                                 size_t max_results,
                                 hu_episodic_pattern_t **out, size_t *out_count) {
    if (!db || !alloc || !out || !out_count) return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_count = 0;

    if (!query || query_len == 0 || max_results == 0) return HU_OK;

    /* Use LIKE matching on problem_type and approach for keyword relevance */
    const char *sql =
        "SELECT id, problem_type, approach, skills_used, outcome_quality, "
        "support_count, insight, session_id, timestamp "
        "FROM episodic_patterns "
        "WHERE problem_type LIKE '%' || ? || '%' "
        "   OR approach LIKE '%' || ? || '%' "
        "   OR skills_used LIKE '%' || ? || '%' "
        "ORDER BY outcome_quality * support_count DESC "
        "LIMIT ?";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return HU_ERR_IO;

    /* Extract a keyword (first word) from query */
    char keyword[128];
    size_t klen = 0;
    for (size_t i = 0; i < query_len && klen < sizeof(keyword) - 1; i++) {
        if (query[i] == ' ' && klen > 0) break;
        keyword[klen++] = query[i];
    }
    keyword[klen] = '\0';

    size_t cap = max_results < HU_EPISODIC_MAX_RETRIEVE ? max_results : HU_EPISODIC_MAX_RETRIEVE;
    if (max_results > HU_EPISODIC_MAX_RETRIEVE)
        hu_log_warn("episodic", NULL, "retrieve capped max_results=%zu to %d", max_results,
                    HU_EPISODIC_MAX_RETRIEVE);

    sqlite3_bind_text(stmt, 1, keyword, (int)klen, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, keyword, (int)klen, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, keyword, (int)klen, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, (int)cap);
    hu_episodic_pattern_t *arr = alloc->alloc(alloc->ctx, cap * sizeof(hu_episodic_pattern_t));
    if (!arr) { sqlite3_finalize(stmt); return HU_ERR_OUT_OF_MEMORY; }
    size_t count = 0;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && count < cap) {
        hu_episodic_pattern_t *p = &arr[count];
        p->id = dup_col_text(alloc, stmt, 0);
        p->problem_type = dup_col_text(alloc, stmt, 1);
        p->approach = dup_col_text(alloc, stmt, 2);
        p->skills_used = dup_col_text(alloc, stmt, 3);
        p->outcome_quality = (float)sqlite3_column_double(stmt, 4);
        p->support_count = (uint32_t)sqlite3_column_int(stmt, 5);
        p->insight = dup_col_text(alloc, stmt, 6);
        p->session_id = dup_col_text(alloc, stmt, 7);
        p->timestamp = dup_col_text(alloc, stmt, 8);

        if (!p->id || !p->problem_type || !p->approach) {
            hu_episodic_free_patterns(alloc, arr, count + 1);
            sqlite3_finalize(stmt);
            return HU_ERR_OUT_OF_MEMORY;
        }
        count++;
    }
    sqlite3_finalize(stmt);

    if (count == 0) {
        alloc->free(alloc->ctx, arr, cap * sizeof(hu_episodic_pattern_t));
        *out = NULL;
        *out_count = 0;
        return HU_OK;
    }

    *out = arr;
    *out_count = count;
    return HU_OK;
}

hu_error_t hu_episodic_build_replay(hu_allocator_t *alloc,
                                     const hu_episodic_pattern_t *patterns,
                                     size_t count,
                                     char **out, size_t *out_len) {
    if (!alloc || !out || !out_len) return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_len = 0;

    if (!patterns || count == 0) return HU_OK;

    char buf[4096];
    size_t pos = 0;
    pos = hu_buf_appendf(buf, sizeof(buf), pos,
                         "### Cognitive Replay\n\n"
                         "*Prior approaches that worked for similar problems (use as hints, not facts):*\n\n");

    for (size_t i = 0; i < count && pos < sizeof(buf) - 1; i++) {
        const hu_episodic_pattern_t *p = &patterns[i];
        pos = hu_buf_appendf(buf, sizeof(buf), pos,
                             "- **%s** (quality: %.0f%%, seen %u time%s): %s",
                             p->problem_type,
                             (double)(p->outcome_quality * 100.0f),
                             p->support_count,
                             p->support_count == 1 ? "" : "s",
                             p->approach);
        if (p->insight && p->insight[0])
            pos = hu_buf_appendf(buf, sizeof(buf), pos, " — *%s*", p->insight);
        pos = hu_buf_appendf(buf, sizeof(buf), pos, "\n");
    }
    pos = hu_buf_appendf(buf, sizeof(buf), pos, "\n");

    size_t len = pos;
    char *result = alloc->alloc(alloc->ctx, len + 1);
    if (!result) return HU_ERR_OUT_OF_MEMORY;
    memcpy(result, buf, len);
    result[len] = '\0';
    *out = result;
    *out_len = len;
    return HU_OK;
}

hu_error_t hu_episodic_compress(sqlite3 *db, hu_allocator_t *alloc) {
    if (!db || !alloc) return HU_ERR_INVALID_ARGUMENT;

    /* Find problem_types with 3+ patterns */
    const char *find_sql =
        "SELECT problem_type, COUNT(*) as cnt, "
        "AVG(outcome_quality) as avg_q, SUM(support_count) as total_support, "
        "GROUP_CONCAT(DISTINCT skills_used) as all_skills, "
        "GROUP_CONCAT(approach, ' | ') as all_approaches "
        "FROM episodic_patterns "
        "GROUP BY problem_type "
        "HAVING cnt >= 3";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, find_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return HU_ERR_IO;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const char *ptype = (const char *)sqlite3_column_text(stmt, 0);
        float avg_quality = (float)sqlite3_column_double(stmt, 2);
        uint32_t total_support = (uint32_t)sqlite3_column_int(stmt, 3);
        const char *all_skills = (const char *)sqlite3_column_text(stmt, 4);
        const char *all_approaches = (const char *)sqlite3_column_text(stmt, 5);

        if (!ptype) continue;

        if (sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK)
            continue;

        /* Delete individual patterns (rolled back if store fails) */
        const char *del_sql =
            "DELETE FROM episodic_patterns WHERE problem_type = ?";
        sqlite3_stmt *del_stmt = NULL;
        if (sqlite3_prepare_v2(db, del_sql, -1, &del_stmt, NULL) != SQLITE_OK) {
            sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
            continue;
        }
        sqlite3_bind_text(del_stmt, 1, ptype, -1, SQLITE_STATIC);
        sqlite3_step(del_stmt);
        sqlite3_finalize(del_stmt);

        /* Build compressed approach (truncated) */
        char approach[512];
        snprintf(approach, sizeof(approach), "Compressed from %u episodes: %.200s",
                 total_support, all_approaches ? all_approaches : "");

        hu_episodic_pattern_t compressed = {
            .id = NULL,
            .problem_type = (char *)ptype,
            .approach = approach,
            .skills_used = (char *)(all_skills ? all_skills : ""),
            .outcome_quality = avg_quality,
            .support_count = total_support,
            .insight = "Recurring pattern (compressed)",
            .session_id = "",
            .timestamp = NULL,
        };

        hu_error_t store_err = hu_episodic_store_pattern(db, alloc, &compressed);
        if (store_err != HU_OK) {
            hu_log_warn("episodic", NULL, "Failed to store compressed pattern: %s",
                        hu_error_string(store_err));
            sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
            continue;
        }
        if (sqlite3_exec(db, "COMMIT", NULL, NULL, NULL) != SQLITE_OK)
            sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
    }
    sqlite3_finalize(stmt);

    return HU_OK;
}

static void free_pattern_strings(hu_allocator_t *alloc, hu_episodic_pattern_t *p) {
    if (p->id)           alloc->free(alloc->ctx, p->id, strlen(p->id) + 1);
    if (p->problem_type) alloc->free(alloc->ctx, p->problem_type, strlen(p->problem_type) + 1);
    if (p->approach)     alloc->free(alloc->ctx, p->approach, strlen(p->approach) + 1);
    if (p->skills_used)  alloc->free(alloc->ctx, p->skills_used, strlen(p->skills_used) + 1);
    if (p->insight)      alloc->free(alloc->ctx, p->insight, strlen(p->insight) + 1);
    if (p->session_id)   alloc->free(alloc->ctx, p->session_id, strlen(p->session_id) + 1);
    if (p->timestamp)    alloc->free(alloc->ctx, p->timestamp, strlen(p->timestamp) + 1);
}

void hu_episodic_free_patterns(hu_allocator_t *alloc,
                                hu_episodic_pattern_t *patterns, size_t count) {
    if (!alloc || !patterns) return;
    for (size_t i = 0; i < count; i++)
        free_pattern_strings(alloc, &patterns[i]);
    alloc->free(alloc->ctx, patterns, count * sizeof(hu_episodic_pattern_t));
}

#endif /* HU_ENABLE_SQLITE */
