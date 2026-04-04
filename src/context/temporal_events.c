#include "human/context/temporal_events.h"
#include <string.h>

#ifdef HU_ENABLE_SQLITE
#include <stdio.h>
#include <time.h>

hu_error_t hu_temporal_events_init_table(sqlite3 *db) {
    if (!db) return HU_ERR_INVALID_ARGUMENT;
    const char *sql =
        "CREATE TABLE IF NOT EXISTS temporal_events ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  contact_id TEXT NOT NULL,"
        "  description TEXT NOT NULL,"
        "  temporal_ref TEXT,"
        "  resolved_timestamp INTEGER,"
        "  extracted_at INTEGER NOT NULL,"
        "  confidence REAL DEFAULT 0.0,"
        "  followed_up INTEGER DEFAULT 0"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_temporal_upcoming "
        "ON temporal_events(followed_up, resolved_timestamp);";
    char *err_msg = NULL;
    if (sqlite3_exec(db, sql, NULL, NULL, &err_msg) != SQLITE_OK) {
        sqlite3_free(err_msg);
        return HU_ERR_IO;
    }
    return HU_OK;
}

hu_error_t hu_temporal_events_store(sqlite3 *db, const char *contact_id, size_t contact_id_len,
                                    const hu_extracted_event_t *event, int64_t resolved_ts,
                                    int64_t now_ts) {
    if (!db || !contact_id || !event) return HU_ERR_INVALID_ARGUMENT;
    const char *sql = "INSERT INTO temporal_events "
                      "(contact_id, description, temporal_ref, resolved_timestamp, "
                      "extracted_at, confidence) VALUES (?1, ?2, ?3, ?4, ?5, ?6)";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return HU_ERR_IO;
    sqlite3_bind_text(stmt, 1, contact_id, (int)contact_id_len, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, event->description,
                      event->description_len > 0 ? (int)event->description_len : -1, SQLITE_STATIC);
    if (event->temporal_ref && event->temporal_ref_len > 0)
        sqlite3_bind_text(stmt, 3, event->temporal_ref, (int)event->temporal_ref_len, SQLITE_STATIC);
    else
        sqlite3_bind_null(stmt, 3);
    sqlite3_bind_int64(stmt, 4, resolved_ts);
    sqlite3_bind_int64(stmt, 5, now_ts);
    sqlite3_bind_double(stmt, 6, event->confidence);
    hu_error_t err = (sqlite3_step(stmt) == SQLITE_DONE) ? HU_OK : HU_ERR_IO;
    sqlite3_finalize(stmt);
    return err;
}

hu_error_t hu_temporal_events_store_batch(sqlite3 *db, const char *contact_id,
                                          size_t contact_id_len,
                                          const hu_event_extract_result_t *result,
                                          int64_t now_ts) {
    if (!db || !contact_id || !result) return HU_ERR_INVALID_ARGUMENT;
    hu_error_t first_err = HU_OK;
    for (size_t i = 0; i < result->event_count; i++) {
        const hu_extracted_event_t *ev = &result->events[i];
        if (ev->confidence < 0.3) continue;
        int64_t resolved = 0;
        if (ev->temporal_ref && ev->temporal_ref_len > 0)
            resolved = hu_temporal_resolve_reference(ev->temporal_ref, ev->temporal_ref_len, now_ts);
        hu_error_t err = hu_temporal_events_store(db, contact_id, contact_id_len, ev, resolved, now_ts);
        if (err != HU_OK && first_err == HU_OK)
            first_err = err;
    }
    return first_err;
}

hu_error_t hu_temporal_events_get_upcoming(sqlite3 *db, hu_allocator_t *alloc, int64_t now_ts,
                                           int64_t window_secs, hu_temporal_event_t *out,
                                           size_t max_events, size_t *out_count) {
    (void)alloc;
    if (!db || !out || !out_count) return HU_ERR_INVALID_ARGUMENT;
    *out_count = 0;
    const char *sql = "SELECT id, contact_id, description, temporal_ref, "
                      "resolved_timestamp, extracted_at, confidence "
                      "FROM temporal_events "
                      "WHERE followed_up = 0 AND resolved_timestamp > 0 "
                      "AND resolved_timestamp >= ?1 AND resolved_timestamp <= ?2 "
                      "ORDER BY resolved_timestamp ASC LIMIT ?3";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return HU_ERR_IO;
    sqlite3_bind_int64(stmt, 1, now_ts);
    sqlite3_bind_int64(stmt, 2, now_ts + window_secs);
    sqlite3_bind_int(stmt, 3, (int)max_events);
    while (sqlite3_step(stmt) == SQLITE_ROW && *out_count < max_events) {
        hu_temporal_event_t *ev = &out[*out_count];
        memset(ev, 0, sizeof(*ev));
        ev->id = sqlite3_column_int64(stmt, 0);
        const char *cid = (const char *)sqlite3_column_text(stmt, 1);
        if (cid) snprintf(ev->contact_id, sizeof(ev->contact_id), "%s", cid);
        const char *desc = (const char *)sqlite3_column_text(stmt, 2);
        if (desc) snprintf(ev->description, sizeof(ev->description), "%s", desc);
        const char *tref = (const char *)sqlite3_column_text(stmt, 3);
        if (tref) snprintf(ev->temporal_ref, sizeof(ev->temporal_ref), "%s", tref);
        ev->resolved_timestamp = sqlite3_column_int64(stmt, 4);
        ev->extracted_at = sqlite3_column_int64(stmt, 5);
        ev->confidence = sqlite3_column_double(stmt, 6);
        ev->followed_up = false;
        (*out_count)++;
    }
    sqlite3_finalize(stmt);
    return HU_OK;
}

hu_error_t hu_temporal_events_mark_followed_up(sqlite3 *db, int64_t event_id) {
    if (!db) return HU_ERR_INVALID_ARGUMENT;
    const char *sql = "UPDATE temporal_events SET followed_up = 1 WHERE id = ?1";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return HU_ERR_IO;
    sqlite3_bind_int64(stmt, 1, event_id);
    hu_error_t err = (sqlite3_step(stmt) == SQLITE_DONE) ? HU_OK : HU_ERR_IO;
    sqlite3_finalize(stmt);
    return err;
}

#endif /* HU_ENABLE_SQLITE */

int64_t hu_temporal_resolve_reference(const char *temporal_ref, size_t ref_len, int64_t now_ts) {
    if (!temporal_ref || ref_len == 0) return 0;
    char lower[128];
    size_t clen = ref_len < 127 ? ref_len : 127;
    for (size_t i = 0; i < clen; i++)
        lower[i] = (char)(temporal_ref[i] >= 'A' && temporal_ref[i] <= 'Z'
                          ? temporal_ref[i] + 32 : temporal_ref[i]);
    lower[clen] = '\0';
    if (strstr(lower, "tomorrow")) return now_ts + 86400;
    if (strstr(lower, "tonight")) return now_ts + 43200;
    if (strstr(lower, "today")) return now_ts + 3600;
    if (strstr(lower, "soon")) return now_ts + 3 * 86400;
    if (strstr(lower, "next week")) return now_ts + 7 * 86400;
    if (strstr(lower, "next month")) return now_ts + 30 * 86400;
    {
        const char *in_ptr = strstr(lower, "in ");
        if (in_ptr) {
            const char *p = in_ptr + 3;
            int num = 0;
            while (p < lower + clen && *p == ' ') p++;
            while (p < lower + clen && *p >= '0' && *p <= '9') {
                num = num * 10 + (int)(*p - '0');
                p++;
            }
            if (num > 0) {
                if (strstr(lower, "minute")) return now_ts + num * 60;
                if (strstr(lower, "hour")) return now_ts + num * 3600;
                if (strstr(lower, "day")) return now_ts + num * 86400;
                if (strstr(lower, "week")) return now_ts + num * 7 * 86400;
                if (strstr(lower, "month")) return now_ts + num * 30 * 86400;
                return now_ts + num * 3600;
            }
        }
    }
    return now_ts + 7 * 86400;
}
