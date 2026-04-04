#include "human/context/temporal_events.h"
#include <string.h>
#include <strings.h>

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>
#endif

int64_t hu_temporal_resolve_reference(const char *ref, size_t ref_len, int64_t now) {
    if (!ref || ref_len == 0) return 0;
    if (strncasecmp(ref, "tomorrow", ref_len) == 0) return now + 86400;
    if (strncasecmp(ref, "today", ref_len) == 0) return now + 3600;
    if (strncasecmp(ref, "tonight", ref_len) == 0) return now + 28800;
    if (strncasecmp(ref, "next week", ref_len) == 0) return now + 7 * 86400;
    if (strncasecmp(ref, "next month", ref_len) == 0) return now + 30 * 86400;
    if (strncasecmp(ref, "soon", ref_len) == 0) return now + 3600;
    if (ref_len >= 5 && strncasecmp(ref, "in ", 3) == 0) {
        int n = 0;
        const char *p = ref + 3;
        while (*p >= '0' && *p <= '9') { n = n * 10 + (*p - '0'); p++; }
        if (*p == ' ') p++;
        if (strncasecmp(p, "hour", 4) == 0) return now + n * 3600;
        if (strncasecmp(p, "day", 3) == 0) return now + n * 86400;
    }
    return now + 86400;
}

hu_error_t hu_temporal_events_init_table(void *db) {
    if (!db) return HU_ERR_INVALID_ARGUMENT;
#ifdef HU_ENABLE_SQLITE
    const char *sql = "CREATE TABLE IF NOT EXISTS temporal_events ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "contact_id TEXT NOT NULL,"
                      "description TEXT,"
                      "event_time INTEGER,"
                      "store_time INTEGER,"
                      "followed_up INTEGER DEFAULT 0)";
    char *err = NULL;
    if (sqlite3_exec((sqlite3 *)db, sql, NULL, NULL, &err) != SQLITE_OK) {
        sqlite3_free(err);
        return HU_ERR_IO;
    }
#endif
    return HU_OK;
}

hu_error_t hu_temporal_events_store(void *db, const char *contact_id, size_t contact_id_len,
                                    const hu_extracted_event_t *ev, int64_t resolved_time,
                                    int64_t now) {
    if (!db || !contact_id || !ev) return HU_ERR_INVALID_ARGUMENT;
    (void)contact_id_len;
#ifdef HU_ENABLE_SQLITE
    const char *sql = "INSERT INTO temporal_events (contact_id, description, event_time, store_time) "
                      "VALUES (?, ?, ?, ?)";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2((sqlite3 *)db, sql, -1, &stmt, NULL) != SQLITE_OK) return HU_ERR_IO;
    sqlite3_bind_text(stmt, 1, contact_id, (int)contact_id_len, NULL);
    sqlite3_bind_text(stmt, 2, ev->description, ev->description_len ? (int)ev->description_len : -1, NULL);
    sqlite3_bind_int64(stmt, 3, resolved_time);
    sqlite3_bind_int64(stmt, 4, now);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_IO;
#else
    (void)resolved_time; (void)now;
    return HU_ERR_NOT_SUPPORTED;
#endif
}

hu_error_t hu_temporal_events_store_batch(void *db, const char *contact_id,
                                          size_t contact_id_len,
                                          const hu_event_extract_result_t *result,
                                          int64_t now) {
    if (!db || !contact_id || !result) return HU_ERR_INVALID_ARGUMENT;
    for (size_t i = 0; i < result->event_count; i++) {
        int64_t resolved = hu_temporal_resolve_reference(result->events[i].temporal_ref,
                                                         result->events[i].temporal_ref_len, now);
        hu_error_t err = hu_temporal_events_store(db, contact_id, contact_id_len,
                                                   &result->events[i], resolved, now);
        if (err != HU_OK) return err;
    }
    return HU_OK;
}

hu_error_t hu_temporal_events_get_upcoming(void *db, hu_allocator_t *alloc, int64_t now,
                                           int64_t window_secs, hu_temporal_event_t *out,
                                           size_t max_count, size_t *out_count) {
    if (!db || !out_count) return HU_ERR_INVALID_ARGUMENT;
    *out_count = 0;
    (void)alloc;
#ifdef HU_ENABLE_SQLITE
    const char *sql = "SELECT id, contact_id, description, event_time, followed_up "
                      "FROM temporal_events WHERE event_time >= ? AND event_time <= ? "
                      "AND followed_up = 0 ORDER BY event_time LIMIT ?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2((sqlite3 *)db, sql, -1, &stmt, NULL) != SQLITE_OK) return HU_ERR_IO;
    sqlite3_bind_int64(stmt, 1, now);
    sqlite3_bind_int64(stmt, 2, now + window_secs);
    sqlite3_bind_int(stmt, 3, (int)max_count);
    while (sqlite3_step(stmt) == SQLITE_ROW && *out_count < max_count) {
        hu_temporal_event_t *e = &out[*out_count];
        e->id = sqlite3_column_int64(stmt, 0);
        e->contact_id = (const char *)sqlite3_column_text(stmt, 1);
        e->description = (const char *)sqlite3_column_text(stmt, 2);
        e->event_time = sqlite3_column_int64(stmt, 3);
        e->followed_up = sqlite3_column_int(stmt, 4) != 0;
        (*out_count)++;
    }
    sqlite3_finalize(stmt);
#else
    (void)now; (void)window_secs; (void)out; (void)max_count;
#endif
    return HU_OK;
}

hu_error_t hu_temporal_events_mark_followed_up(void *db, int64_t event_id) {
    if (!db) return HU_ERR_INVALID_ARGUMENT;
#ifdef HU_ENABLE_SQLITE
    const char *sql = "UPDATE temporal_events SET followed_up = 1 WHERE id = ?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2((sqlite3 *)db, sql, -1, &stmt, NULL) != SQLITE_OK) return HU_ERR_IO;
    sqlite3_bind_int64(stmt, 1, event_id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_IO;
#else
    (void)event_id;
    return HU_ERR_NOT_SUPPORTED;
#endif
}
