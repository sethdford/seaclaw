/*
 * Value Learning Engine — infers user values from corrections, approvals,
 * and rejections. Uses inferred values to guide autonomous decisions
 * and build value-aware prompts.
 */

#ifdef HU_ENABLE_SQLITE

#include "human/intelligence/value_learning.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/string.h"
#include <sqlite3.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#define VALUE_NAME_MAX 255
#define VALUE_DESC_MAX 1023

static size_t safe_name_len(const char *name, size_t name_len) {
    if (name_len == 0 && name)
        return strlen(name);
    return name_len > VALUE_NAME_MAX ? VALUE_NAME_MAX : name_len;
}

static void copy_to_name_buf(const char *name, size_t name_len, char *buf) {
    size_t n = safe_name_len(name, name_len);
    if (n > 0 && name)
        memcpy(buf, name, n);
    buf[n] = '\0';
}

hu_error_t hu_value_engine_create(hu_allocator_t *alloc, sqlite3 *db,
                                  hu_value_engine_t *out) {
    if (!alloc || !db || !out)
        return HU_ERR_INVALID_ARGUMENT;
    out->alloc = alloc;
    out->db = db;
    return HU_OK;
}

void hu_value_engine_deinit(hu_value_engine_t *engine) {
    (void)engine;
    /* Caller owns db; no-op */
}

hu_error_t hu_value_init_tables(hu_value_engine_t *engine) {
    if (!engine || !engine->db)
        return HU_ERR_INVALID_ARGUMENT;

    const char *sql =
        "CREATE TABLE IF NOT EXISTS inferred_values("
        "id INTEGER PRIMARY KEY, name TEXT UNIQUE, description TEXT DEFAULT '', "
        "importance REAL DEFAULT 0.5, evidence_count INTEGER DEFAULT 0, "
        "created_at INTEGER, updated_at INTEGER)";
    int rc = sqlite3_exec(engine->db, sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_MEMORY_STORE;
    return HU_OK;
}

hu_error_t hu_value_learn_from_correction(hu_value_engine_t *engine,
                                          const char *value_name, size_t name_len,
                                          const char *description, size_t desc_len,
                                          double strength, int64_t now_ts) {
    if (!engine || !engine->db || !value_name)
        return HU_ERR_INVALID_ARGUMENT;

    char name_buf[256];
    copy_to_name_buf(value_name, name_len, name_buf);

    const char *sel = "SELECT id, importance, evidence_count, description FROM inferred_values WHERE name = ?1";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(engine->db, sel, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_MEMORY_STORE;
    sqlite3_bind_text(stmt, 1, name_buf, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int64_t id = sqlite3_column_int64(stmt, 0);
        double imp = sqlite3_column_double(stmt, 1);
        int32_t ev = sqlite3_column_int(stmt, 2);
        const char *old_desc = (const char *)sqlite3_column_text(stmt, 3);
        size_t old_desc_len = old_desc ? (size_t)sqlite3_column_bytes(stmt, 3) : 0;

        char desc_buf[1024];
        const char *use_desc;
        size_t use_desc_len;
        if (description && desc_len > 0 && desc_len > old_desc_len) {
            size_t copy = desc_len < VALUE_DESC_MAX ? desc_len : VALUE_DESC_MAX;
            memcpy(desc_buf, description, copy);
            desc_buf[copy] = '\0';
            use_desc = desc_buf;
            use_desc_len = copy;
        } else if (old_desc && old_desc_len > 0) {
            size_t copy = old_desc_len < VALUE_DESC_MAX ? old_desc_len : VALUE_DESC_MAX;
            memcpy(desc_buf, old_desc, copy);
            desc_buf[copy] = '\0';
            use_desc = desc_buf;
            use_desc_len = copy;
        } else if (description && desc_len > 0) {
            size_t copy = desc_len < VALUE_DESC_MAX ? desc_len : VALUE_DESC_MAX;
            memcpy(desc_buf, description, copy);
            desc_buf[copy] = '\0';
            use_desc = desc_buf;
            use_desc_len = copy;
        } else {
            use_desc = "";
            use_desc_len = 0;
        }
        sqlite3_finalize(stmt);

        double delta = strength * 0.1;
        imp += delta;
        if (imp > 1.0)
            imp = 1.0;
        ev++;

        const char *upd =
            "UPDATE inferred_values SET importance = ?1, evidence_count = ?2, "
            "description = ?3, updated_at = ?4 WHERE id = ?5";
        rc = sqlite3_prepare_v2(engine->db, upd, -1, &stmt, NULL);
        if (rc != SQLITE_OK)
            return HU_ERR_MEMORY_STORE;
        sqlite3_bind_double(stmt, 1, imp);
        sqlite3_bind_int(stmt, 2, ev);
        sqlite3_bind_text(stmt, 3, use_desc, (int)use_desc_len, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 4, now_ts);
        sqlite3_bind_int64(stmt, 5, id);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_MEMORY_STORE;
    }
    sqlite3_finalize(stmt);

    double imp = strength * 0.5;
    if (imp > 1.0)
        imp = 1.0;
    if (imp < 0.0)
        imp = 0.0;

    char desc_buf[1024];
    const char *ins_desc = "";
    int ins_desc_len = 0;
    if (description && desc_len > 0) {
        size_t copy = desc_len < VALUE_DESC_MAX ? desc_len : VALUE_DESC_MAX;
        memcpy(desc_buf, description, copy);
        desc_buf[copy] = '\0';
        ins_desc = desc_buf;
        ins_desc_len = (int)copy;
    }

    const char *ins =
        "INSERT INTO inferred_values (name, description, importance, evidence_count, created_at, updated_at) "
        "VALUES (?1, ?2, ?3, 1, ?4, ?4)";
    rc = sqlite3_prepare_v2(engine->db, ins, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_MEMORY_STORE;
    sqlite3_bind_text(stmt, 1, name_buf, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, ins_desc, ins_desc_len, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 3, imp);
    sqlite3_bind_int64(stmt, 4, now_ts);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_MEMORY_STORE;
}

hu_error_t hu_value_learn_from_approval(hu_value_engine_t *engine,
                                        const char *value_name, size_t name_len,
                                        double strength, int64_t now_ts) {
    if (!engine || !engine->db || !value_name)
        return HU_ERR_INVALID_ARGUMENT;

    char name_buf[256];
    copy_to_name_buf(value_name, name_len, name_buf);

    const char *sel = "SELECT id, importance, evidence_count FROM inferred_values WHERE name = ?1";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(engine->db, sel, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_MEMORY_STORE;
    sqlite3_bind_text(stmt, 1, name_buf, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int64_t id = sqlite3_column_int64(stmt, 0);
        double imp = sqlite3_column_double(stmt, 1);
        int32_t ev = sqlite3_column_int(stmt, 2);
        sqlite3_finalize(stmt);

        double delta = strength * 0.05;
        imp += delta;
        if (imp > 1.0)
            imp = 1.0;
        ev++;

        const char *upd =
            "UPDATE inferred_values SET importance = ?1, evidence_count = ?2, updated_at = ?3 WHERE id = ?4";
        rc = sqlite3_prepare_v2(engine->db, upd, -1, &stmt, NULL);
        if (rc != SQLITE_OK)
            return HU_ERR_MEMORY_STORE;
        sqlite3_bind_double(stmt, 1, imp);
        sqlite3_bind_int(stmt, 2, ev);
        sqlite3_bind_int64(stmt, 3, now_ts);
        sqlite3_bind_int64(stmt, 4, id);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_MEMORY_STORE;
    }
    sqlite3_finalize(stmt);

    double imp = strength * 0.3;
    if (imp > 1.0)
        imp = 1.0;
    if (imp < 0.0)
        imp = 0.0;

    const char *ins =
        "INSERT INTO inferred_values (name, importance, evidence_count, created_at, updated_at) "
        "VALUES (?1, ?2, 1, ?3, ?3)";
    rc = sqlite3_prepare_v2(engine->db, ins, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_MEMORY_STORE;
    sqlite3_bind_text(stmt, 1, name_buf, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 2, imp);
    sqlite3_bind_int64(stmt, 3, now_ts);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_MEMORY_STORE;
}

hu_error_t hu_value_weaken(hu_value_engine_t *engine,
                           const char *value_name, size_t name_len,
                           double amount, int64_t now_ts) {
    if (!engine || !engine->db || !value_name)
        return HU_ERR_INVALID_ARGUMENT;

    char name_buf[256];
    copy_to_name_buf(value_name, name_len, name_buf);

    const char *sel = "SELECT id, importance FROM inferred_values WHERE name = ?1";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(engine->db, sel, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_MEMORY_STORE;
    sqlite3_bind_text(stmt, 1, name_buf, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return HU_OK;
    }

    int64_t id = sqlite3_column_int64(stmt, 0);
    double imp = sqlite3_column_double(stmt, 1);
    sqlite3_finalize(stmt);

    imp -= amount;
    if (imp < 0.0)
        imp = 0.0;

    if (imp < 0.05) {
        const char *del = "DELETE FROM inferred_values WHERE id = ?1";
        rc = sqlite3_prepare_v2(engine->db, del, -1, &stmt, NULL);
        if (rc != SQLITE_OK)
            return HU_ERR_MEMORY_STORE;
        sqlite3_bind_int64(stmt, 1, id);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_MEMORY_STORE;
    }

    const char *upd = "UPDATE inferred_values SET importance = ?1, updated_at = ?2 WHERE id = ?3";
    rc = sqlite3_prepare_v2(engine->db, upd, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_MEMORY_STORE;
    sqlite3_bind_double(stmt, 1, imp);
    sqlite3_bind_int64(stmt, 2, now_ts);
    sqlite3_bind_int64(stmt, 3, id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_MEMORY_STORE;
}

hu_error_t hu_value_get(hu_value_engine_t *engine,
                        const char *name, size_t name_len,
                        hu_value_t *out, bool *found) {
    if (!engine || !engine->db || !name || !out || !found)
        return HU_ERR_INVALID_ARGUMENT;
    *found = false;

    char name_buf[256];
    copy_to_name_buf(name, name_len, name_buf);

    const char *sel = "SELECT id, name, description, importance, evidence_count, created_at, updated_at "
                     "FROM inferred_values WHERE name = ?1";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(engine->db, sel, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_MEMORY_STORE;
    sqlite3_bind_text(stmt, 1, name_buf, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return HU_OK;
    }
    *found = true;

    out->id = sqlite3_column_int64(stmt, 0);
    const char *n = (const char *)sqlite3_column_text(stmt, 1);
    size_t nlen = n ? (size_t)sqlite3_column_bytes(stmt, 1) : 0;
    if (nlen >= sizeof(out->name))
        nlen = sizeof(out->name) - 1;
    if (n && nlen > 0)
        memcpy(out->name, n, nlen);
    out->name[nlen] = '\0';
    out->name_len = nlen;

    const char *d = (const char *)sqlite3_column_text(stmt, 2);
    size_t dlen = d ? (size_t)sqlite3_column_bytes(stmt, 2) : 0;
    if (dlen >= sizeof(out->description))
        dlen = sizeof(out->description) - 1;
    if (d && dlen > 0)
        memcpy(out->description, d, dlen);
    out->description[dlen] = '\0';
    out->description_len = dlen;

    out->importance = sqlite3_column_double(stmt, 3);
    out->evidence_count = sqlite3_column_int(stmt, 4);
    out->created_at = sqlite3_column_int64(stmt, 5);
    out->updated_at = sqlite3_column_int64(stmt, 6);

    sqlite3_finalize(stmt);
    return HU_OK;
}

hu_error_t hu_value_list(hu_value_engine_t *engine,
                         hu_value_t **out, size_t *out_count) {
    if (!engine || !engine->db || !out || !out_count)
        return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_count = 0;

    const char *sel = "SELECT id, name, description, importance, evidence_count, created_at, updated_at "
                     "FROM inferred_values ORDER BY importance DESC";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(engine->db, sel, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_MEMORY_STORE;

    size_t cap = 64;
    size_t count = 0;
    hu_value_t *arr = (hu_value_t *)engine->alloc->alloc(engine->alloc->ctx, cap * sizeof(hu_value_t));
    if (!arr) {
        sqlite3_finalize(stmt);
        return HU_ERR_OUT_OF_MEMORY;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= cap) {
            size_t new_cap = cap * 2;
            hu_value_t *narr = (hu_value_t *)engine->alloc->realloc(engine->alloc->ctx, arr,
                                                                     cap * sizeof(hu_value_t),
                                                                     new_cap * sizeof(hu_value_t));
            if (!narr) {
                engine->alloc->free(engine->alloc->ctx, arr, cap * sizeof(hu_value_t));
                sqlite3_finalize(stmt);
                return HU_ERR_OUT_OF_MEMORY;
            }
            arr = narr;
            cap = new_cap;
        }

        hu_value_t *v = &arr[count];
        v->id = sqlite3_column_int64(stmt, 0);
        const char *n = (const char *)sqlite3_column_text(stmt, 1);
        size_t nlen = n ? (size_t)sqlite3_column_bytes(stmt, 1) : 0;
        if (nlen >= sizeof(v->name))
            nlen = sizeof(v->name) - 1;
        if (n && nlen > 0)
            memcpy(v->name, n, nlen);
        v->name[nlen] = '\0';
        v->name_len = nlen;

        const char *d = (const char *)sqlite3_column_text(stmt, 2);
        size_t dlen = d ? (size_t)sqlite3_column_bytes(stmt, 2) : 0;
        if (dlen >= sizeof(v->description))
            dlen = sizeof(v->description) - 1;
        if (d && dlen > 0)
            memcpy(v->description, d, dlen);
        v->description[dlen] = '\0';
        v->description_len = dlen;

        v->importance = sqlite3_column_double(stmt, 3);
        v->evidence_count = sqlite3_column_int(stmt, 4);
        v->created_at = sqlite3_column_int64(stmt, 5);
        v->updated_at = sqlite3_column_int64(stmt, 6);
        count++;
    }
    sqlite3_finalize(stmt);

    *out = arr;
    *out_count = count;
    return HU_OK;
}

hu_error_t hu_value_count(hu_value_engine_t *engine, size_t *out) {
    if (!engine || !engine->db || !out)
        return HU_ERR_INVALID_ARGUMENT;
    *out = 0;

    const char *sql = "SELECT COUNT(*) FROM inferred_values";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(engine->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_MEMORY_STORE;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        *out = (size_t)sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return HU_OK;
}

hu_error_t hu_value_build_prompt(hu_value_engine_t *engine,
                                 char **out, size_t *out_len) {
    if (!engine || !engine->db || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_len = 0;

    const char *sel = "SELECT name, importance, description FROM inferred_values "
                     "ORDER BY importance DESC LIMIT 10";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(engine->db, sel, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_MEMORY_STORE;

    hu_str_t parts[10];
    size_t count = 0;
    char line_buf[1400];
    while (sqlite3_step(stmt) == SQLITE_ROW && count < 10) {
        const char *n = (const char *)sqlite3_column_text(stmt, 0);
        double imp = sqlite3_column_double(stmt, 1);
        const char *d = (const char *)sqlite3_column_text(stmt, 2);
        if (!n)
            n = "";
        if (!d)
            d = "";
        int nw = snprintf(line_buf, sizeof(line_buf), "- %s (%.2f): %s\n", n, imp, d);
        if (nw > 0 && (size_t)nw < sizeof(line_buf)) {
            parts[count].ptr = (const char *)engine->alloc->alloc(engine->alloc->ctx, (size_t)nw + 1);
            if (!parts[count].ptr) {
                for (size_t i = 0; i < count; i++)
                    engine->alloc->free(engine->alloc->ctx, (void *)parts[i].ptr, parts[i].len + 1);
                sqlite3_finalize(stmt);
                return HU_ERR_OUT_OF_MEMORY;
            }
            memcpy((void *)parts[count].ptr, line_buf, (size_t)nw + 1);
            parts[count].len = (size_t)nw;
            count++;
        }
    }
    sqlite3_finalize(stmt);

    if (count == 0) {
        char *empty = (char *)engine->alloc->alloc(engine->alloc->ctx, 1);
        if (!empty)
            return HU_ERR_OUT_OF_MEMORY;
        empty[0] = '\0';
        *out = empty;
        *out_len = 0;
        return HU_OK;
    }

    size_t prefix_len = 15;
    const char *prefix = "The user values:\n";
    size_t total = prefix_len;
    for (size_t i = 0; i < count; i++)
        total += parts[i].len;
    total += 1;

    char *result = (char *)engine->alloc->alloc(engine->alloc->ctx, total);
    if (!result) {
        for (size_t i = 0; i < count; i++)
            engine->alloc->free(engine->alloc->ctx, (void *)parts[i].ptr, parts[i].len + 1);
        return HU_ERR_OUT_OF_MEMORY;
    }
    memcpy(result, prefix, prefix_len);
    size_t pos = prefix_len;
    for (size_t i = 0; i < count; i++) {
        memcpy(result + pos, parts[i].ptr, parts[i].len);
        pos += parts[i].len;
        engine->alloc->free(engine->alloc->ctx, (void *)parts[i].ptr, parts[i].len + 1);
    }
    result[pos] = '\0';

    *out = result;
    *out_len = pos;
    return HU_OK;
}

static bool action_contains_value(const char *action, size_t action_len,
                                  const char *val_name, size_t val_name_len) {
    if (!action || !val_name || val_name_len == 0 || action_len < val_name_len)
        return false;
    for (size_t i = 0; i + val_name_len <= action_len; i++) {
        if (strncasecmp(action + i, val_name, val_name_len) == 0)
            return true;
    }
    return false;
}

double hu_value_alignment_score(const hu_value_t *values, size_t count,
                                const char *action, size_t action_len) {
    if (!values || count == 0 || !action)
        return 0.0;
    if (action_len == 0)
        action_len = strlen(action);

    double sum_all = 0.0;
    double sum_matching = 0.0;
    for (size_t i = 0; i < count; i++) {
        double imp = values[i].importance;
        sum_all += imp;
        if (action_contains_value(action, action_len, values[i].name, values[i].name_len))
            sum_matching += imp;
    }
    if (sum_all <= 0.0)
        return 0.0;
    double score = sum_matching / sum_all;
    if (score < 0.0)
        score = 0.0;
    if (score > 1.0)
        score = 1.0;
    return score;
}

void hu_value_free(hu_allocator_t *alloc, hu_value_t *values, size_t count) {
    if (!alloc || !values)
        return;
    alloc->free(alloc->ctx, values, count * sizeof(hu_value_t));
}

#endif /* HU_ENABLE_SQLITE */
