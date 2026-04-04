#include "human/context/temporal_events.h"

#ifdef HU_ENABLE_SQLITE

hu_error_t hu_temporal_events_init_table(sqlite3 *db) {
    if (!db)
        return HU_ERR_INVALID_ARGUMENT;
    const char *sql =
        "CREATE TABLE IF NOT EXISTS temporal_events ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "contact_id TEXT NOT NULL,"
        "event_type TEXT,"
        "description TEXT,"
        "event_timestamp INTEGER NOT NULL,"
        "followed_up INTEGER DEFAULT 0,"
        "created_at INTEGER DEFAULT (strftime('%s','now'))"
        ")";
    return sqlite3_exec(db, sql, NULL, NULL, NULL) == SQLITE_OK ? HU_OK : HU_ERR_IO;
}

hu_error_t hu_temporal_events_store_batch(sqlite3 *db, const char *contact_id,
                                          size_t contact_id_len,
                                          const struct hu_deep_extract_result *result,
                                          int64_t now) {
    (void)db;
    (void)contact_id;
    (void)contact_id_len;
    (void)result;
    (void)now;
    return HU_OK;
}

hu_error_t hu_temporal_events_get_upcoming(sqlite3 *db, hu_allocator_t *alloc,
                                           int64_t now, int64_t window_secs,
                                           hu_temporal_event_t *out, size_t max_count,
                                           size_t *out_count) {
    (void)db;
    (void)alloc;
    (void)now;
    (void)window_secs;
    (void)out;
    (void)max_count;
    if (out_count)
        *out_count = 0;
    return HU_OK;
}

hu_error_t hu_temporal_events_mark_followed_up(sqlite3 *db, int64_t event_id) {
    if (!db)
        return HU_ERR_INVALID_ARGUMENT;
    const char *sql = "UPDATE temporal_events SET followed_up=1 WHERE id=?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return HU_ERR_IO;
    sqlite3_bind_int64(stmt, 1, event_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return HU_OK;
}

#endif
