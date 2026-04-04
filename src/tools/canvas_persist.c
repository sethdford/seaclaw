#include "human/tools/canvas_store.h"
#include "human/core/string.h"
#include <string.h>

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>

static const char CANVAS_SCHEMA[] =
    "CREATE TABLE IF NOT EXISTS canvases ("
    "  canvas_id TEXT PRIMARY KEY,"
    "  format TEXT,"
    "  imports TEXT,"
    "  language TEXT,"
    "  title TEXT,"
    "  content TEXT"
    ");"
    "CREATE TABLE IF NOT EXISTS canvas_versions ("
    "  canvas_id TEXT,"
    "  version_seq INTEGER,"
    "  content TEXT,"
    "  PRIMARY KEY (canvas_id, version_seq)"
    ");";

static hu_error_t canvas_db_ensure_schema(sqlite3 *db) {
    char *err_msg = NULL;
    if (sqlite3_exec(db, CANVAS_SCHEMA, NULL, NULL, &err_msg) != SQLITE_OK) {
        sqlite3_free(err_msg);
        return HU_ERR_IO;
    }
    return HU_OK;
}

hu_error_t hu_canvas_store_set_db(hu_canvas_store_t *store, void *db) {
    if (!store || !db)
        return HU_ERR_INVALID_ARGUMENT;
    hu_error_t err = canvas_db_ensure_schema((sqlite3 *)db);
    if (err != HU_OK)
        return err;
    hu_canvas_store_set_db_internal(store, db);
    return HU_OK;
}

hu_error_t hu_canvas_persist_save(void *db, const char *canvas_id, const char *format,
                                  const char *imports, const char *language, const char *title,
                                  const char *content) {
    if (!db || !canvas_id)
        return HU_ERR_INVALID_ARGUMENT;
    sqlite3 *sdb = (sqlite3 *)db;
    const char *sql = "INSERT OR REPLACE INTO canvases (canvas_id, format, imports, language, "
                      "title, content) VALUES (?, ?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(sdb, sql, -1, &stmt, NULL) != SQLITE_OK)
        return HU_ERR_IO;
    sqlite3_bind_text(stmt, 1, canvas_id, -1, NULL);
    sqlite3_bind_text(stmt, 2, format, -1, NULL);
    sqlite3_bind_text(stmt, 3, imports, -1, NULL);
    sqlite3_bind_text(stmt, 4, language, -1, NULL);
    sqlite3_bind_text(stmt, 5, title, -1, NULL);
    sqlite3_bind_text(stmt, 6, content, -1, NULL);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_IO;
}

hu_error_t hu_canvas_persist_save_version(void *db, const char *canvas_id, uint32_t version_seq,
                                          const char *content) {
    if (!db || !canvas_id)
        return HU_ERR_INVALID_ARGUMENT;
    sqlite3 *sdb = (sqlite3 *)db;
    const char *sql = "INSERT OR REPLACE INTO canvas_versions (canvas_id, version_seq, content) "
                      "VALUES (?, ?, ?)";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(sdb, sql, -1, &stmt, NULL) != SQLITE_OK)
        return HU_ERR_IO;
    sqlite3_bind_text(stmt, 1, canvas_id, -1, NULL);
    sqlite3_bind_int(stmt, 2, (int)version_seq);
    sqlite3_bind_text(stmt, 3, content, -1, NULL);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_IO;
}

hu_error_t hu_canvas_persist_delete(void *db, const char *canvas_id) {
    if (!db || !canvas_id)
        return HU_ERR_INVALID_ARGUMENT;
    sqlite3 *sdb = (sqlite3 *)db;
    const char *sql1 = "DELETE FROM canvas_versions WHERE canvas_id = ?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(sdb, sql1, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, canvas_id, -1, NULL);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    const char *sql2 = "DELETE FROM canvases WHERE canvas_id = ?";
    if (sqlite3_prepare_v2(sdb, sql2, -1, &stmt, NULL) != SQLITE_OK)
        return HU_ERR_IO;
    sqlite3_bind_text(stmt, 1, canvas_id, -1, NULL);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_IO;
}

hu_error_t hu_canvas_persist_load_all(void *db, hu_canvas_store_t *store) {
    if (!db || !store)
        return HU_ERR_INVALID_ARGUMENT;
    sqlite3 *sdb = (sqlite3 *)db;
    const char *sql = "SELECT canvas_id, format, imports, language, title, content FROM canvases";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(sdb, sql, -1, &stmt, NULL) != SQLITE_OK)
        return HU_ERR_IO;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *cid = (const char *)sqlite3_column_text(stmt, 0);
        const char *fmt = (const char *)sqlite3_column_text(stmt, 1);
        const char *imp = (const char *)sqlite3_column_text(stmt, 2);
        const char *lang = (const char *)sqlite3_column_text(stmt, 3);
        const char *ttl = (const char *)sqlite3_column_text(stmt, 4);
        const char *cnt = (const char *)sqlite3_column_text(stmt, 5);
        hu_canvas_store_put_canvas(store, cid, fmt, imp, lang, ttl, cnt);
    }
    sqlite3_finalize(stmt);
    return HU_OK;
}

#else /* !HU_ENABLE_SQLITE */

hu_error_t hu_canvas_store_set_db(hu_canvas_store_t *store, void *db) {
    if (!store || !db)
        return HU_ERR_INVALID_ARGUMENT;
    return HU_ERR_NOT_SUPPORTED;
}

hu_error_t hu_canvas_persist_save(void *db, const char *canvas_id, const char *format,
                                  const char *imports, const char *language, const char *title,
                                  const char *content) {
    if (!db || !canvas_id)
        return HU_ERR_INVALID_ARGUMENT;
    (void)format; (void)imports; (void)language; (void)title; (void)content;
    return HU_ERR_NOT_SUPPORTED;
}

hu_error_t hu_canvas_persist_save_version(void *db, const char *canvas_id, uint32_t version_seq,
                                          const char *content) {
    if (!db || !canvas_id)
        return HU_ERR_INVALID_ARGUMENT;
    (void)version_seq; (void)content;
    return HU_ERR_NOT_SUPPORTED;
}

hu_error_t hu_canvas_persist_delete(void *db, const char *canvas_id) {
    if (!db || !canvas_id)
        return HU_ERR_INVALID_ARGUMENT;
    return HU_ERR_NOT_SUPPORTED;
}

hu_error_t hu_canvas_persist_load_all(void *db, hu_canvas_store_t *store) {
    if (!db || !store)
        return HU_ERR_INVALID_ARGUMENT;
    return HU_ERR_NOT_SUPPORTED;
}

#endif
