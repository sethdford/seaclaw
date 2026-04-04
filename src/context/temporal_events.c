#include "human/context/temporal_events.h"
#include <string.h>

#ifdef HU_ENABLE_SQLITE

hu_error_t hu_temporal_events_init_table(sqlite3 *db) {
    if (!db) return HU_ERR_INVALID_ARGUMENT;
    const char *sql = "CREATE TABLE IF NOT EXISTS temporal_events ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "contact_id TEXT NOT NULL,"
        "description TEXT,"
        "temporal_ref TEXT,"
        "resolved_timestamp INTEGER,"
        "extracted_at INTEGER,"
        "confidence REAL DEFAULT 1.0,"
        "followed_up INTEGER DEFAULT 0)";
    return sqlite3_exec(db, sql, NULL, NULL, NULL) == SQLITE_OK ? HU_OK : HU_ERR_IO;
}

hu_error_t hu_temporal_events_store(sqlite3 *db, const char *contact_id,
                                    size_t contact_id_len,
                                    const hu_extracted_event_t *event,
                                    int64_t resolved_ts, int64_t now_ts) {
    (void)db; (void)contact_id; (void)contact_id_len;
    (void)event; (void)resolved_ts; (void)now_ts;
    return HU_OK;
}

hu_error_t hu_temporal_events_store_batch(sqlite3 *db, const char *contact_id,
                                          size_t contact_id_len,
                                          const hu_event_extract_result_t *result,
                                          int64_t now_ts) {
    (void)db; (void)contact_id; (void)contact_id_len; (void)result; (void)now_ts;
    return HU_OK;
}

hu_error_t hu_temporal_events_get_upcoming(sqlite3 *db, hu_allocator_t *alloc,
                                           int64_t now_ts, int64_t window_secs,
                                           hu_temporal_event_t *out,
                                           size_t max_events, size_t *out_count) {
    (void)db; (void)alloc; (void)now_ts; (void)window_secs;
    (void)out; (void)max_events;
    if (out_count) *out_count = 0;
    return HU_OK;
}

hu_error_t hu_temporal_events_mark_followed_up(sqlite3 *db, int64_t event_id) {
    if (!db) return HU_ERR_INVALID_ARGUMENT;
    const char *sql = "UPDATE temporal_events SET followed_up=1 WHERE id=?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return HU_ERR_IO;
    sqlite3_bind_int64(stmt, 1, event_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return HU_OK;
}

int64_t hu_temporal_resolve_reference(const char *temporal_ref, size_t ref_len,
                                      int64_t now_ts) {
    if (!temporal_ref || ref_len == 0) return now_ts;
    if (ref_len >= 8 && strncmp(temporal_ref, "tomorrow", 8) == 0) return now_ts + 86400;
    if (ref_len >= 9 && strncmp(temporal_ref, "next week", 9) == 0) return now_ts + 7 * 86400;
    if (ref_len >= 7 && strncmp(temporal_ref, "tonight", 7) == 0) {
        int64_t day_start = (now_ts / 86400) * 86400;
        return day_start + 20 * 3600;
    }
    return now_ts;
}

#endif
