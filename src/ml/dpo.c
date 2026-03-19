#include "human/ml/dpo.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

hu_error_t hu_dpo_collector_create(hu_allocator_t *alloc,
#ifdef HU_ENABLE_SQLITE
                                   sqlite3 *db,
#else
                                   void *db,
#endif
                                   size_t max_pairs, hu_dpo_collector_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    out->alloc = alloc;
    out->db = db;
    out->max_pairs = max_pairs > 0 ? max_pairs : 10000;
    out->pair_count = 0;
    return HU_OK;
}

void hu_dpo_collector_deinit(hu_dpo_collector_t *collector) {
    if (collector)
        memset(collector, 0, sizeof(*collector));
}

hu_error_t hu_dpo_init_tables(hu_dpo_collector_t *collector) {
    if (!collector)
        return HU_ERR_INVALID_ARGUMENT;
#ifdef HU_ENABLE_SQLITE
    if (!collector->db)
        return HU_OK;
    const char *sql =
        "CREATE TABLE IF NOT EXISTS dpo_pairs("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "prompt TEXT, chosen TEXT, rejected TEXT, "
        "margin REAL, timestamp INTEGER, source TEXT);";
    char *err_msg = NULL;
    int rc = sqlite3_exec(collector->db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg)
            sqlite3_free(err_msg);
        return HU_ERR_IO;
    }
#endif
    return HU_OK;
}

hu_error_t hu_dpo_record_pair(hu_dpo_collector_t *collector,
                              const hu_preference_pair_t *pair) {
    if (!collector || !pair)
        return HU_ERR_INVALID_ARGUMENT;

#ifdef HU_ENABLE_SQLITE
    if (collector->db) {
        sqlite3_stmt *stmt = NULL;
        int rc = sqlite3_prepare_v2(collector->db,
            "INSERT INTO dpo_pairs(prompt, chosen, rejected, margin, timestamp, source) "
            "VALUES(?, ?, ?, ?, ?, ?)",
            -1, &stmt, NULL);
        if (rc != SQLITE_OK)
            return HU_ERR_IO;
        sqlite3_bind_text(stmt, 1, pair->prompt, (int)pair->prompt_len, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, pair->chosen, (int)pair->chosen_len, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, pair->rejected, (int)pair->rejected_len, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 4, pair->margin);
        sqlite3_bind_int64(stmt, 5, pair->timestamp);
        sqlite3_bind_text(stmt, 6, pair->source, (int)pair->source_len, SQLITE_STATIC);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE)
            return HU_ERR_IO;

        /* Ring buffer eviction */
        if (collector->max_pairs > 0) {
            sqlite3_stmt *cnt_stmt = NULL;
            rc = sqlite3_prepare_v2(collector->db, "SELECT COUNT(*) FROM dpo_pairs", -1, &cnt_stmt, NULL);
            if (rc == SQLITE_OK && sqlite3_step(cnt_stmt) == SQLITE_ROW) {
                int64_t total = sqlite3_column_int64(cnt_stmt, 0);
                sqlite3_finalize(cnt_stmt);
                if ((size_t)total > collector->max_pairs) {
                    size_t excess = (size_t)total - collector->max_pairs;
                    char del_sql[128];
                    snprintf(del_sql, sizeof(del_sql),
                             "DELETE FROM dpo_pairs WHERE id IN "
                             "(SELECT id FROM dpo_pairs ORDER BY id LIMIT %zu)", excess);
                    sqlite3_exec(collector->db, del_sql, NULL, NULL, NULL);
                }
            } else if (cnt_stmt) {
                sqlite3_finalize(cnt_stmt);
            }
        }
    }
#endif
    collector->pair_count++;
    return HU_OK;
}

hu_error_t hu_dpo_record_from_feedback(hu_dpo_collector_t *collector,
                                       const char *prompt, size_t prompt_len,
                                       const char *response, size_t response_len,
                                       bool positive) {
    if (!collector || !prompt || !response)
        return HU_ERR_INVALID_ARGUMENT;

    hu_preference_pair_t pair;
    memset(&pair, 0, sizeof(pair));

    size_t plen = prompt_len < sizeof(pair.prompt) - 1 ? prompt_len : sizeof(pair.prompt) - 1;
    memcpy(pair.prompt, prompt, plen);
    pair.prompt_len = plen;

    if (positive) {
        size_t rlen = response_len < sizeof(pair.chosen) - 1 ? response_len : sizeof(pair.chosen) - 1;
        memcpy(pair.chosen, response, rlen);
        pair.chosen_len = rlen;
        pair.rejected_len = 0;
    } else {
        size_t rlen = response_len < sizeof(pair.rejected) - 1 ? response_len : sizeof(pair.rejected) - 1;
        memcpy(pair.rejected, response, rlen);
        pair.rejected_len = rlen;
        pair.chosen_len = 0;
    }

    pair.margin = 0.7;
    pair.timestamp = (int64_t)time(NULL);
    memcpy(pair.source, "user_feedback", 13);
    pair.source_len = 13;

    return hu_dpo_record_pair(collector, &pair);
}

hu_error_t hu_dpo_record_from_retry(hu_dpo_collector_t *collector,
                                    const char *prompt, size_t prompt_len,
                                    const char *rejected, size_t rejected_len,
                                    const char *chosen, size_t chosen_len) {
    if (!collector || !prompt || !rejected || !chosen)
        return HU_ERR_INVALID_ARGUMENT;

    hu_preference_pair_t pair;
    memset(&pair, 0, sizeof(pair));

    size_t plen = prompt_len < sizeof(pair.prompt) - 1 ? prompt_len : sizeof(pair.prompt) - 1;
    memcpy(pair.prompt, prompt, plen);
    pair.prompt_len = plen;

    size_t clen = chosen_len < sizeof(pair.chosen) - 1 ? chosen_len : sizeof(pair.chosen) - 1;
    memcpy(pair.chosen, chosen, clen);
    pair.chosen_len = clen;

    size_t rlen = rejected_len < sizeof(pair.rejected) - 1 ? rejected_len : sizeof(pair.rejected) - 1;
    memcpy(pair.rejected, rejected, rlen);
    pair.rejected_len = rlen;

    pair.margin = 0.8;
    pair.timestamp = (int64_t)time(NULL);
    memcpy(pair.source, "reflection_retry", 16);
    pair.source_len = 16;

    return hu_dpo_record_pair(collector, &pair);
}

hu_error_t hu_dpo_export_jsonl(hu_dpo_collector_t *collector,
                               const char *path, size_t path_len,
                               size_t *exported_count) {
    if (!collector || !exported_count)
        return HU_ERR_INVALID_ARGUMENT;

#ifdef HU_IS_TEST
    *exported_count = collector->pair_count;
    (void)path;
    (void)path_len;
    return HU_OK;
#else
#ifdef HU_ENABLE_SQLITE
    if (!collector->db || !path || path_len == 0) {
        *exported_count = 0;
        return HU_ERR_NOT_SUPPORTED;
    }

    char filepath[512];
    size_t flen = path_len < sizeof(filepath) - 1 ? path_len : sizeof(filepath) - 1;
    memcpy(filepath, path, flen);
    filepath[flen] = '\0';

    FILE *f = fopen(filepath, "w");
    if (!f)
        return HU_ERR_IO;

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(collector->db,
        "SELECT prompt, chosen, rejected, margin, source FROM dpo_pairs ORDER BY id",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fclose(f);
        return HU_ERR_IO;
    }

    size_t count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *prompt = (const char *)sqlite3_column_text(stmt, 0);
        const char *chosen = (const char *)sqlite3_column_text(stmt, 1);
        const char *rejected = (const char *)sqlite3_column_text(stmt, 2);
        double margin = sqlite3_column_double(stmt, 3);
        const char *source = (const char *)sqlite3_column_text(stmt, 4);

        const char *fields[] = {prompt, chosen, rejected, NULL, source};
        const char *keys[] = {"prompt", "chosen", "rejected", NULL, "source"};
        fputc('{', f);
        for (int fi = 0; fi < 5; fi++) {
            if (fi == 3) {
                fprintf(f, "\"margin\":%.2f", margin);
            } else {
                const char *val = fields[fi] ? fields[fi] : "";
                fprintf(f, "\"%s\":\"", keys[fi]);
                for (const char *c = val; *c; c++) {
                    switch (*c) {
                    case '"':  fputs("\\\"", f); break;
                    case '\\': fputs("\\\\", f); break;
                    case '\n': fputs("\\n", f);  break;
                    case '\r': fputs("\\r", f);  break;
                    case '\t': fputs("\\t", f);  break;
                    default:   fputc(*c, f);     break;
                    }
                }
                fputc('"', f);
            }
            if (fi < 4)
                fputc(',', f);
        }
        fputs("}\n", f);
        count++;
    }
    sqlite3_finalize(stmt);
    fclose(f);
    *exported_count = count;
    return HU_OK;
#else
    *exported_count = 0;
    (void)path;
    (void)path_len;
    return HU_ERR_NOT_SUPPORTED;
#endif
#endif
}

hu_error_t hu_dpo_pair_count(hu_dpo_collector_t *collector, size_t *out) {
    if (!collector || !out)
        return HU_ERR_INVALID_ARGUMENT;
    *out = collector->pair_count;
    return HU_OK;
}

hu_error_t hu_dpo_clear(hu_dpo_collector_t *collector) {
    if (!collector)
        return HU_ERR_INVALID_ARGUMENT;
#ifdef HU_ENABLE_SQLITE
    if (collector->db)
        sqlite3_exec(collector->db, "DELETE FROM dpo_pairs", NULL, NULL, NULL);
#endif
    collector->pair_count = 0;
    return HU_OK;
}

void hu_dpo_export_free(hu_allocator_t *alloc, hu_dpo_export_t *export_data) {
    if (!alloc || !export_data)
        return;
    if (export_data->pairs) {
        alloc->free(alloc->ctx, export_data->pairs,
                    export_data->count * sizeof(hu_preference_pair_t));
        export_data->pairs = NULL;
    }
    export_data->count = 0;
}
