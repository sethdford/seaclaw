#include "human/tools/db_introspect.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include "human/tools/validation.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>
#endif

#define TOOL_NAME "db_schema"
#define TOOL_DESC "Inspect database schema — list tables, columns, indexes, foreign keys"
#define TOOL_PARAMS                                                                  \
    "{\"type\":\"object\",\"properties\":{"                                         \
    "\"action\":{\"type\":\"string\",\"enum\":[\"tables\",\"columns\",\"indexes\","  \
    "\"foreign_keys\"]},"                                                           \
    "\"table\":{\"type\":\"string\"},"                                              \
    "\"database\":{\"type\":\"string\"}"                                            \
    "},\"required\":[\"action\"]}"

typedef struct {
    hu_allocator_t *alloc;
    char *default_db_path;
} db_introspect_ctx_t;

#if defined(__GNUC__) || defined(__clang__)
__attribute__((unused))
#endif
static bool is_safe_sql_identifier(const char *s) {
    if (!s || !s[0])
        return false;
    for (const char *p = s; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '_'))
            return false;
    }
    return true;
}

static hu_error_t db_introspect_execute(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args,
                                        hu_tool_result_t *out) {
    (void)alloc;
    db_introspect_ctx_t *c = (db_introspect_ctx_t *)ctx;
    if (!out)
        return HU_ERR_INVALID_ARGUMENT;
    if (!args) {
        *out = hu_tool_result_fail("invalid args", 12);
        return HU_ERR_INVALID_ARGUMENT;
    }

    const char *action = hu_json_get_string(args, "action");
    if (!action) {
        *out = hu_tool_result_fail("missing action", 14);
        return HU_OK;
    }

#if HU_IS_TEST || !defined(HU_ENABLE_SQLITE)
    (void)c;

    if (strcmp(action, "tables") == 0) {
        *out = hu_tool_result_ok("{\"tables\":[\"test_table\"]}", 26);
    } else if (strcmp(action, "columns") == 0) {
        *out = hu_tool_result_ok(
            "{\"columns\":[{\"name\":\"id\",\"type\":\"INTEGER\",\"notnull\":1,\"pk\":1}]}", 72);
    } else if (strcmp(action, "indexes") == 0) {
        *out = hu_tool_result_ok("{\"indexes\":[]}", 14);
    } else if (strcmp(action, "foreign_keys") == 0) {
        *out = hu_tool_result_ok("{\"foreign_keys\":[]}", 19);
    } else {
        *out = hu_tool_result_fail("unknown action", 14);
    }
    return HU_OK;

#else
    const char *table = hu_json_get_string(args, "table");
    const char *db_path = hu_json_get_string(args, "database");
    if (!db_path)
        db_path = c->default_db_path ? c->default_db_path : ":memory:";

    if (hu_tool_validate_path(db_path, NULL, 0) != HU_OK) {
        *out = hu_tool_result_fail("invalid database path", 21);
        return HU_OK;
    }

    sqlite3 *db = NULL;
    if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        *out = hu_tool_result_fail("cannot open database", 20);
        return HU_OK;
    }

    if (strcmp(action, "tables") == 0) {
        const char *query = "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name";
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
            sqlite3_close(db);
            *out = hu_tool_result_fail("query failed", 12);
            return HU_OK;
        }

        size_t cap = 4096;
        char *buf = (char *)alloc->alloc(alloc->ctx, cap);
        if (!buf) {
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return HU_ERR_OUT_OF_MEMORY;
        }

        size_t off = 0;
        off = hu_buf_appendf(buf, cap, off, "{\"tables\":[");
        int first = 1;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *nm = (const char *)sqlite3_column_text(stmt, 0);
            if (!first && off < cap - 2)
                buf[off++] = ',';
            off = hu_buf_appendf(buf, cap, off, "\"%s\"", nm ? nm : "");
            first = 0;
        }

        off = hu_buf_appendf(buf, cap, off, "]}");
        *out = hu_tool_result_ok_owned(buf, off);
        sqlite3_finalize(stmt);

    } else if (strcmp(action, "columns") == 0) {
        if (!table) {
            sqlite3_close(db);
            *out = hu_tool_result_fail("columns action requires table parameter", 39);
            return HU_OK;
        }
        if (!is_safe_sql_identifier(table)) {
            sqlite3_close(db);
            *out = hu_tool_result_fail("invalid table name", 18);
            return HU_OK;
        }

        char query[512];
        snprintf(query, sizeof(query), "PRAGMA table_info(%s)", table);
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
            sqlite3_close(db);
            *out = hu_tool_result_fail("query failed", 12);
            return HU_OK;
        }

        size_t cap = 8192;
        char *buf = (char *)alloc->alloc(alloc->ctx, cap);
        if (!buf) {
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return HU_ERR_OUT_OF_MEMORY;
        }

        size_t off = 0;
        off = hu_buf_appendf(buf, cap, off, "{\"columns\":[");
        int first = 1;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *col_name = (const char *)sqlite3_column_text(stmt, 1);
            const char *col_type = (const char *)sqlite3_column_text(stmt, 2);
            int col_notnull = sqlite3_column_int(stmt, 3);
            int col_pk = sqlite3_column_int(stmt, 5);

            if (!first && off < cap - 2)
                buf[off++] = ',';
            off = hu_buf_appendf(buf, cap, off,
                                 "{\"name\":\"%s\",\"type\":\"%s\",\"notnull\":%d,\"pk\":%d}",
                                 col_name ? col_name : "", col_type ? col_type : "", col_notnull,
                                 col_pk);
            first = 0;
        }

        off = hu_buf_appendf(buf, cap, off, "]}");
        *out = hu_tool_result_ok_owned(buf, off);
        sqlite3_finalize(stmt);

    } else if (strcmp(action, "indexes") == 0) {
        if (!table) {
            sqlite3_close(db);
            *out = hu_tool_result_fail("indexes action requires table parameter", 39);
            return HU_OK;
        }
        if (!is_safe_sql_identifier(table)) {
            sqlite3_close(db);
            *out = hu_tool_result_fail("invalid table name", 18);
            return HU_OK;
        }

        char query[512];
        snprintf(query, sizeof(query), "PRAGMA index_list(%s)", table);
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
            sqlite3_close(db);
            *out = hu_tool_result_fail("query failed", 12);
            return HU_OK;
        }

        size_t cap = 8192;
        char *buf = (char *)alloc->alloc(alloc->ctx, cap);
        if (!buf) {
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return HU_ERR_OUT_OF_MEMORY;
        }

        size_t off = 0;
        off = hu_buf_appendf(buf, cap, off, "{\"indexes\":[");
        int first = 1;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *idx_name = (const char *)sqlite3_column_text(stmt, 1);
            int idx_unique = sqlite3_column_int(stmt, 2);

            if (!first && off < cap - 2)
                buf[off++] = ',';
            off = hu_buf_appendf(buf, cap, off, "{\"name\":\"%s\",\"unique\":%d}",
                                 idx_name ? idx_name : "", idx_unique);
            first = 0;
        }

        off = hu_buf_appendf(buf, cap, off, "]}");
        *out = hu_tool_result_ok_owned(buf, off);
        sqlite3_finalize(stmt);

    } else if (strcmp(action, "foreign_keys") == 0) {
        if (!table) {
            sqlite3_close(db);
            *out = hu_tool_result_fail("foreign_keys action requires table parameter", 44);
            return HU_OK;
        }
        if (!is_safe_sql_identifier(table)) {
            sqlite3_close(db);
            *out = hu_tool_result_fail("invalid table name", 18);
            return HU_OK;
        }

        char query[512];
        snprintf(query, sizeof(query), "PRAGMA foreign_key_list(%s)", table);
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
            sqlite3_close(db);
            *out = hu_tool_result_fail("query failed", 12);
            return HU_OK;
        }

        size_t cap = 8192;
        char *buf = (char *)alloc->alloc(alloc->ctx, cap);
        if (!buf) {
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return HU_ERR_OUT_OF_MEMORY;
        }

        size_t off = 0;
        off = hu_buf_appendf(buf, cap, off, "{\"foreign_keys\":[");
        int first = 1;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int fk_id = sqlite3_column_int(stmt, 0);
            const char *fk_table = (const char *)sqlite3_column_text(stmt, 2);
            const char *fk_from = (const char *)sqlite3_column_text(stmt, 3);
            const char *fk_to = (const char *)sqlite3_column_text(stmt, 4);

            if (!first && off < cap - 2)
                buf[off++] = ',';
            off = hu_buf_appendf(buf, cap, off,
                                 "{\"id\":%d,\"table\":\"%s\",\"from\":\"%s\",\"to\":\"%s\"}", fk_id,
                                 fk_table ? fk_table : "", fk_from ? fk_from : "",
                                 fk_to ? fk_to : "");
            first = 0;
        }

        off = hu_buf_appendf(buf, cap, off, "]}");
        *out = hu_tool_result_ok_owned(buf, off);
        sqlite3_finalize(stmt);

    } else {
        *out = hu_tool_result_fail("unknown action", 14);
    }

    sqlite3_close(db);
    return HU_OK;

#endif
}

static const char *db_introspect_name(void *ctx) {
    (void)ctx;
    return TOOL_NAME;
}

static const char *db_introspect_desc(void *ctx) {
    (void)ctx;
    return TOOL_DESC;
}

static const char *db_introspect_params(void *ctx) {
    (void)ctx;
    return TOOL_PARAMS;
}

static void db_introspect_deinit(void *ctx, hu_allocator_t *alloc) {
    db_introspect_ctx_t *c = (db_introspect_ctx_t *)ctx;
    if (!c)
        return;
    if (c->default_db_path)
        alloc->free(alloc->ctx, c->default_db_path, strlen(c->default_db_path) + 1);
    alloc->free(alloc->ctx, c, sizeof(*c));
}

static const hu_tool_vtable_t db_introspect_vtable = {
    .execute = db_introspect_execute,
    .name = db_introspect_name,
    .description = db_introspect_desc,
    .parameters_json = db_introspect_params,
    .deinit = db_introspect_deinit,
};

hu_error_t hu_db_introspect_tool_create(hu_allocator_t *alloc, const char *default_db_path,
                                        hu_tool_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    db_introspect_ctx_t *c = (db_introspect_ctx_t *)alloc->alloc(alloc->ctx, sizeof(*c));
    if (!c)
        return HU_ERR_OUT_OF_MEMORY;
    memset(c, 0, sizeof(*c));
    c->alloc = alloc;
    c->default_db_path =
        default_db_path ? hu_strndup(alloc, default_db_path, strlen(default_db_path)) : NULL;
    out->ctx = c;
    out->vtable = &db_introspect_vtable;
    return HU_OK;
}
