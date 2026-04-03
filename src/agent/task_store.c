#include "human/agent/task_store.h"
#include "human/core/string.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(HU_ENABLE_SQLITE) && !HU_IS_TEST
#include <sqlite3.h>
#endif

struct hu_task_store {
    hu_allocator_t *alloc;
#if defined(HU_ENABLE_SQLITE) && !HU_IS_TEST
    sqlite3 *db;
#endif
    hu_task_record_t *mem_rows;
    size_t mem_count;
    size_t mem_cap;
    uint64_t mem_next_id;
    bool use_sqlite;
};

static bool task_store_use_sqlite(void *db) {
#if defined(HU_ENABLE_SQLITE) && !HU_IS_TEST
    return db != NULL;
#else
    (void)db;
    return false;
#endif
}

#if defined(HU_ENABLE_SQLITE) && !HU_IS_TEST

static hu_error_t sqlite_ensure_schema(sqlite3 *db) {
    static const char sql[] = "CREATE TABLE IF NOT EXISTS hula_tasks ("
                              "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                              "name TEXT, "
                              "status INTEGER DEFAULT 0, "
                              "program_json TEXT, "
                              "trace_json TEXT, "
                              "created_at INTEGER, "
                              "updated_at INTEGER, "
                              "parent_task_id INTEGER DEFAULT 0)";
    char *errmsg = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        if (errmsg)
            sqlite3_free(errmsg);
        return HU_ERR_IO;
    }
    return HU_OK;
}

#endif

static hu_error_t mem_ensure_cap(hu_task_store_t *s, hu_allocator_t *alloc, size_t need) {
    if (need <= s->mem_cap)
        return HU_OK;
    size_t ncap = s->mem_cap ? s->mem_cap * 2 : 8;
    while (ncap < need)
        ncap *= 2;
    void *nr = alloc->realloc(alloc->ctx, s->mem_rows, s->mem_cap * sizeof(hu_task_record_t),
                              ncap * sizeof(hu_task_record_t));
    if (!nr)
        return HU_ERR_OUT_OF_MEMORY;
    s->mem_rows = (hu_task_record_t *)nr;
    s->mem_cap = ncap;
    return HU_OK;
}

static int mem_find_index(hu_task_store_t *s, uint64_t id) {
    for (size_t i = 0; i < s->mem_count; i++) {
        if (s->mem_rows[i].id == id)
            return (int)i;
    }
    return -1;
}

hu_error_t hu_task_store_create(hu_allocator_t *alloc, void *db, hu_task_store_t **out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    hu_task_store_t *st =
        (hu_task_store_t *)alloc->alloc(alloc->ctx, sizeof(hu_task_store_t));
    if (!st)
        return HU_ERR_OUT_OF_MEMORY;
    memset(st, 0, sizeof(*st));
    st->alloc = alloc;
    st->mem_next_id = 1;
    st->use_sqlite = task_store_use_sqlite(db);

#if defined(HU_ENABLE_SQLITE) && !HU_IS_TEST
    if (st->use_sqlite) {
        st->db = (sqlite3 *)db;
        hu_error_t se = sqlite_ensure_schema(st->db);
        if (se != HU_OK) {
            alloc->free(alloc->ctx, st, sizeof(*st));
            return se;
        }
    }
#else
    (void)db;
#endif

    *out = st;
    return HU_OK;
}

void hu_task_store_destroy(hu_task_store_t *store, hu_allocator_t *alloc) {
    if (!store || !alloc)
        return;
    for (size_t i = 0; i < store->mem_count; i++)
        hu_task_record_free(alloc, &store->mem_rows[i]);
    if (store->mem_rows)
        alloc->free(alloc->ctx, store->mem_rows, store->mem_cap * sizeof(hu_task_record_t));
    alloc->free(alloc->ctx, store, sizeof(*store));
}

const char *hu_task_status_string(hu_task_status_t s) {
    switch (s) {
    case HU_TASK_STATUS_PENDING:
        return "pending";
    case HU_TASK_STATUS_RUNNING:
        return "running";
    case HU_TASK_STATUS_COMPLETED:
        return "completed";
    case HU_TASK_STATUS_FAILED:
        return "failed";
    case HU_TASK_STATUS_CANCELLED:
        return "cancelled";
    default:
        return "unknown";
    }
}

void hu_task_record_free(hu_allocator_t *alloc, hu_task_record_t *r) {
    if (!alloc || !r)
        return;
    if (r->name)
        hu_str_free(alloc, r->name);
    if (r->program_json)
        hu_str_free(alloc, r->program_json);
    if (r->trace_json)
        hu_str_free(alloc, r->trace_json);
    memset(r, 0, sizeof(*r));
}

void hu_task_records_free(hu_allocator_t *alloc, hu_task_record_t *records, size_t count) {
    if (!alloc || !records)
        return;
    for (size_t i = 0; i < count; i++)
        hu_task_record_free(alloc, &records[i]);
    alloc->free(alloc->ctx, records, count * sizeof(hu_task_record_t));
}

hu_error_t hu_task_store_save(hu_task_store_t *store, hu_allocator_t *alloc,
                              const hu_task_record_t *task, uint64_t *out_id) {
    if (!store || !alloc || !task || !out_id)
        return HU_ERR_INVALID_ARGUMENT;

    int64_t ts = (task->updated_at != 0) ? task->updated_at : (int64_t)time(NULL);
    int64_t cts = (task->created_at != 0) ? task->created_at : ts;

#if defined(HU_ENABLE_SQLITE) && !HU_IS_TEST
    if (store->use_sqlite && store->db) {
        if (task->id == 0) {
            static const char ins[] =
                "INSERT INTO hula_tasks (name, status, program_json, trace_json, created_at, "
                "updated_at, parent_task_id) VALUES (?,?,?,?,?,?,?)";
            sqlite3_stmt *stmt = NULL;
            if (sqlite3_prepare_v2(store->db, ins, -1, &stmt, NULL) != SQLITE_OK)
                return HU_ERR_IO;

            char *name_b = hu_strdup(alloc, task->name ? task->name : "");
            char *prog_b = task->program_json ? hu_strdup(alloc, task->program_json) : NULL;
            char *trace_b = task->trace_json ? hu_strdup(alloc, task->trace_json) : NULL;
            if (!name_b) {
                sqlite3_finalize(stmt);
                return HU_ERR_OUT_OF_MEMORY;
            }

            sqlite3_bind_text(stmt, 1, name_b, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, (int)task->status);
            if (prog_b)
                sqlite3_bind_text(stmt, 3, prog_b, -1, SQLITE_STATIC);
            else
                sqlite3_bind_null(stmt, 3);
            if (trace_b)
                sqlite3_bind_text(stmt, 4, trace_b, -1, SQLITE_STATIC);
            else
                sqlite3_bind_null(stmt, 4);
            sqlite3_bind_int64(stmt, 5, (sqlite3_int64)cts);
            sqlite3_bind_int64(stmt, 6, (sqlite3_int64)ts);
            sqlite3_bind_int64(stmt, 7, (sqlite3_int64)task->parent_task_id);

            int step = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            hu_str_free(alloc, name_b);
            if (prog_b)
                hu_str_free(alloc, prog_b);
            if (trace_b)
                hu_str_free(alloc, trace_b);
            if (step != SQLITE_DONE)
                return HU_ERR_IO;
            *out_id = (uint64_t)sqlite3_last_insert_rowid(store->db);
            return HU_OK;
        }

        static const char upd[] = "UPDATE hula_tasks SET name=?, status=?, program_json=?, "
                                  "trace_json=?, updated_at=?, parent_task_id=? WHERE id=?";
        sqlite3_stmt *ust = NULL;
        if (sqlite3_prepare_v2(store->db, upd, -1, &ust, NULL) != SQLITE_OK)
            return HU_ERR_IO;

        char *name_b = hu_strdup(alloc, task->name ? task->name : "");
        char *prog_b = task->program_json ? hu_strdup(alloc, task->program_json) : NULL;
        char *trace_b = task->trace_json ? hu_strdup(alloc, task->trace_json) : NULL;
        if (!name_b) {
            sqlite3_finalize(ust);
            return HU_ERR_OUT_OF_MEMORY;
        }
        sqlite3_bind_text(ust, 1, name_b, -1, SQLITE_STATIC);
        sqlite3_bind_int(ust, 2, (int)task->status);
        if (prog_b)
            sqlite3_bind_text(ust, 3, prog_b, -1, SQLITE_STATIC);
        else
            sqlite3_bind_null(ust, 3);
        if (trace_b)
            sqlite3_bind_text(ust, 4, trace_b, -1, SQLITE_STATIC);
        else
            sqlite3_bind_null(ust, 4);
        sqlite3_bind_int64(ust, 5, (sqlite3_int64)ts);
        sqlite3_bind_int64(ust, 6, (sqlite3_int64)task->parent_task_id);
        sqlite3_bind_int64(ust, 7, (sqlite3_int64)task->id);

        int stp = sqlite3_step(ust);
        sqlite3_finalize(ust);
        hu_str_free(alloc, name_b);
        if (prog_b)
            hu_str_free(alloc, prog_b);
        if (trace_b)
            hu_str_free(alloc, trace_b);
        if (stp != SQLITE_DONE || sqlite3_changes(store->db) == 0)
            return HU_ERR_NOT_FOUND;
        *out_id = task->id;
        return HU_OK;
    }
#endif

    /* In-memory backend */
    if (task->id == 0) {
        hu_error_t ce = mem_ensure_cap(store, alloc, store->mem_count + 1);
        if (ce != HU_OK)
            return ce;
        hu_task_record_t *row = &store->mem_rows[store->mem_count];
        memset(row, 0, sizeof(*row));
        row->id = store->mem_next_id++;
        row->name = hu_strdup(alloc, task->name ? task->name : "");
        row->status = task->status;
        row->program_json =
            task->program_json ? hu_strdup(alloc, task->program_json) : NULL;
        row->trace_json = task->trace_json ? hu_strdup(alloc, task->trace_json) : NULL;
        row->created_at = cts;
        row->updated_at = ts;
        row->parent_task_id = task->parent_task_id;
        if (!row->name || (task->program_json && !row->program_json) ||
            (task->trace_json && !row->trace_json)) {
            hu_task_record_free(alloc, row);
            memset(row, 0, sizeof(*row));
            return HU_ERR_OUT_OF_MEMORY;
        }
        store->mem_count++;
        *out_id = row->id;
        return HU_OK;
    }

    int ix = mem_find_index(store, task->id);
    if (ix < 0)
        return HU_ERR_NOT_FOUND;
    hu_task_record_t *row = &store->mem_rows[(size_t)ix];
    int64_t prev_created = row->created_at;
    hu_task_record_free(alloc, row);
    memset(row, 0, sizeof(*row));
    row->id = task->id;
    row->name = hu_strdup(alloc, task->name ? task->name : "");
    row->status = task->status;
    row->program_json = task->program_json ? hu_strdup(alloc, task->program_json) : NULL;
    row->trace_json = task->trace_json ? hu_strdup(alloc, task->trace_json) : NULL;
    row->created_at = task->created_at != 0 ? task->created_at : prev_created;
    row->updated_at = ts;
    row->parent_task_id = task->parent_task_id;
    if (!row->name || (task->program_json && !row->program_json) ||
        (task->trace_json && !row->trace_json)) {
        hu_task_record_free(alloc, row);
        memset(row, 0, sizeof(*row));
        return HU_ERR_OUT_OF_MEMORY;
    }
    *out_id = task->id;
    return HU_OK;
}

hu_error_t hu_task_store_load(hu_task_store_t *store, hu_allocator_t *alloc, uint64_t id,
                              hu_task_record_t *out) {
    if (!store || !alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));

#if defined(HU_ENABLE_SQLITE) && !HU_IS_TEST
    if (store->use_sqlite && store->db) {
        static const char q[] =
            "SELECT id, name, status, program_json, trace_json, created_at, updated_at, "
            "parent_task_id FROM hula_tasks WHERE id=?";
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(store->db, q, -1, &stmt, NULL) != SQLITE_OK)
            return HU_ERR_IO;
        sqlite3_bind_int64(stmt, 1, (sqlite3_int64)id);
        int r = sqlite3_step(stmt);
        if (r != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            return HU_ERR_NOT_FOUND;
        }

        out->id = (uint64_t)sqlite3_column_int64(stmt, 0);
        const unsigned char *nm = sqlite3_column_text(stmt, 1);
        int nm_len = sqlite3_column_bytes(stmt, 1);
        out->name = nm ? hu_strndup(alloc, (const char *)nm, (size_t)nm_len) : hu_strdup(alloc, "");
        out->status = (hu_task_status_t)sqlite3_column_int(stmt, 2);
        const unsigned char *pj = sqlite3_column_text(stmt, 3);
        int pj_len = sqlite3_column_bytes(stmt, 3);
        if (pj)
            out->program_json = hu_strndup(alloc, (const char *)pj, (size_t)pj_len);
        const unsigned char *tj = sqlite3_column_text(stmt, 4);
        int tj_len = sqlite3_column_bytes(stmt, 4);
        if (tj)
            out->trace_json = hu_strndup(alloc, (const char *)tj, (size_t)tj_len);
        out->created_at = (int64_t)sqlite3_column_int64(stmt, 5);
        out->updated_at = (int64_t)sqlite3_column_int64(stmt, 6);
        out->parent_task_id = (uint64_t)sqlite3_column_int64(stmt, 7);
        sqlite3_finalize(stmt);
        if (!out->name)
            return HU_ERR_OUT_OF_MEMORY;
        return HU_OK;
    }
#endif

    int ix = mem_find_index(store, id);
    if (ix < 0)
        return HU_ERR_NOT_FOUND;
    const hu_task_record_t *src = &store->mem_rows[(size_t)ix];
    out->id = src->id;
    out->name = hu_strdup(alloc, src->name ? src->name : "");
    out->status = src->status;
    out->program_json =
        src->program_json ? hu_strdup(alloc, src->program_json) : NULL;
    out->trace_json = src->trace_json ? hu_strdup(alloc, src->trace_json) : NULL;
    out->created_at = src->created_at;
    out->updated_at = src->updated_at;
    out->parent_task_id = src->parent_task_id;
    if (!out->name || (src->program_json && !out->program_json) ||
        (src->trace_json && !out->trace_json)) {
        hu_task_record_free(alloc, out);
        memset(out, 0, sizeof(*out));
        return HU_ERR_OUT_OF_MEMORY;
    }
    return HU_OK;
}

hu_error_t hu_task_store_update_status(hu_task_store_t *store, uint64_t id, hu_task_status_t status) {
    if (!store)
        return HU_ERR_INVALID_ARGUMENT;
    int64_t ts = (int64_t)time(NULL);

#if defined(HU_ENABLE_SQLITE) && !HU_IS_TEST
    if (store->use_sqlite && store->db) {
        static const char q[] = "UPDATE hula_tasks SET status=?, updated_at=? WHERE id=?";
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(store->db, q, -1, &stmt, NULL) != SQLITE_OK)
            return HU_ERR_IO;
        sqlite3_bind_int(stmt, 1, (int)status);
        sqlite3_bind_int64(stmt, 2, (sqlite3_int64)ts);
        sqlite3_bind_int64(stmt, 3, (sqlite3_int64)id);
        int r = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (r != SQLITE_DONE)
            return HU_ERR_IO;
        if (sqlite3_changes(store->db) == 0)
            return HU_ERR_NOT_FOUND;
        return HU_OK;
    }
#endif

    int ix = mem_find_index(store, id);
    if (ix < 0)
        return HU_ERR_NOT_FOUND;
    store->mem_rows[(size_t)ix].status = status;
    store->mem_rows[(size_t)ix].updated_at = ts;
    return HU_OK;
}

hu_error_t hu_task_store_list(hu_task_store_t *store, hu_allocator_t *alloc,
                              hu_task_status_t *filter_status, hu_task_record_t **out,
                              size_t *out_count) {
    if (!store || !alloc || !out || !out_count)
        return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_count = 0;

#if defined(HU_ENABLE_SQLITE) && !HU_IS_TEST
    if (store->use_sqlite && store->db) {
        const char *qbase =
            "SELECT id, name, status, program_json, trace_json, created_at, updated_at, "
            "parent_task_id FROM hula_tasks";
        char qbuf[256];
        sqlite3_stmt *stmt = NULL;
        if (filter_status) {
            snprintf(qbuf, sizeof(qbuf), "%s WHERE status=? ORDER BY id DESC", qbase);
            if (sqlite3_prepare_v2(store->db, qbuf, -1, &stmt, NULL) != SQLITE_OK)
                return HU_ERR_IO;
            sqlite3_bind_int(stmt, 1, (int)*filter_status);
        } else {
            snprintf(qbuf, sizeof(qbuf), "%s ORDER BY id DESC", qbase);
            if (sqlite3_prepare_v2(store->db, qbuf, -1, &stmt, NULL) != SQLITE_OK)
                return HU_ERR_IO;
        }

        size_t cap = 0;
        hu_task_record_t *rows = NULL;
        size_t n = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            if (n >= cap) {
                size_t ncap = cap ? cap * 2 : 8;
                void *nr = alloc->realloc(alloc->ctx, rows, cap * sizeof(hu_task_record_t),
                                          ncap * sizeof(hu_task_record_t));
                if (!nr) {
                    sqlite3_finalize(stmt);
                    hu_task_records_free(alloc, rows, n);
                    return HU_ERR_OUT_OF_MEMORY;
                }
                rows = (hu_task_record_t *)nr;
                cap = ncap;
            }
            hu_task_record_t *row = &rows[n];
            memset(row, 0, sizeof(*row));
            row->id = (uint64_t)sqlite3_column_int64(stmt, 0);
            const unsigned char *nm = sqlite3_column_text(stmt, 1);
            int nm_len = sqlite3_column_bytes(stmt, 1);
            row->name = nm ? hu_strndup(alloc, (const char *)nm, (size_t)nm_len) : hu_strdup(alloc, "");
            row->status = (hu_task_status_t)sqlite3_column_int(stmt, 2);
            const unsigned char *pj = sqlite3_column_text(stmt, 3);
            int pj_len = sqlite3_column_bytes(stmt, 3);
            if (pj)
                row->program_json = hu_strndup(alloc, (const char *)pj, (size_t)pj_len);
            const unsigned char *tj = sqlite3_column_text(stmt, 4);
            int tj_len = sqlite3_column_bytes(stmt, 4);
            if (tj)
                row->trace_json = hu_strndup(alloc, (const char *)tj, (size_t)tj_len);
            row->created_at = (int64_t)sqlite3_column_int64(stmt, 5);
            row->updated_at = (int64_t)sqlite3_column_int64(stmt, 6);
            row->parent_task_id = (uint64_t)sqlite3_column_int64(stmt, 7);
            if (!row->name) {
                sqlite3_finalize(stmt);
                hu_task_records_free(alloc, rows, n + 1);
                return HU_ERR_OUT_OF_MEMORY;
            }
            n++;
        }
        sqlite3_finalize(stmt);
        *out = rows;
        *out_count = n;
        return HU_OK;
    }
#endif

    size_t match = 0;
    for (size_t i = 0; i < store->mem_count; i++) {
        if (filter_status && store->mem_rows[i].status != *filter_status)
            continue;
        match++;
    }
    if (match == 0)
        return HU_OK;

    hu_task_record_t *rows =
        (hu_task_record_t *)alloc->alloc(alloc->ctx, match * sizeof(hu_task_record_t));
    if (!rows)
        return HU_ERR_OUT_OF_MEMORY;
    size_t w = 0;
    for (int i = (int)store->mem_count - 1; i >= 0; i--) {
        const hu_task_record_t *src = &store->mem_rows[(size_t)i];
        if (filter_status && src->status != *filter_status)
            continue;
        hu_task_record_t *row = &rows[w++];
        memset(row, 0, sizeof(*row));
        row->id = src->id;
        row->name = hu_strdup(alloc, src->name ? src->name : "");
        row->status = src->status;
        row->program_json =
            src->program_json ? hu_strdup(alloc, src->program_json) : NULL;
        row->trace_json = src->trace_json ? hu_strdup(alloc, src->trace_json) : NULL;
        row->created_at = src->created_at;
        row->updated_at = src->updated_at;
        row->parent_task_id = src->parent_task_id;
        if (!row->name || (src->program_json && !row->program_json) ||
            (src->trace_json && !row->trace_json)) {
            hu_task_records_free(alloc, rows, w);
            return HU_ERR_OUT_OF_MEMORY;
        }
    }
    *out = rows;
    *out_count = match;
    return HU_OK;
}
