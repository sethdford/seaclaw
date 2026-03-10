#ifdef HU_ENABLE_SQLITE

#include "human/memory/comfort_patterns.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/memory.h"
#include <sqlite3.h>
#include <string.h>

hu_error_t hu_comfort_pattern_record(hu_memory_t *memory, const char *contact_id,
                                     size_t contact_id_len, const char *emotion, size_t emotion_len,
                                     const char *response_type, size_t response_type_len,
                                     float engagement_score) {
    if (!memory || !contact_id || contact_id_len == 0 || !emotion || emotion_len == 0 ||
        !response_type || response_type_len == 0)
        return HU_ERR_INVALID_ARGUMENT;

    sqlite3 *db = hu_sqlite_memory_get_db(memory);
    if (!db)
        return HU_ERR_NOT_SUPPORTED;

    /* Fetch existing row for running average */
    sqlite3_stmt *sel = NULL;
    int rc = sqlite3_prepare_v2(db,
                                "SELECT engagement_score, sample_count FROM comfort_patterns "
                                "WHERE contact_id=? AND emotion=? AND response_type=?",
                                -1, &sel, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_MEMORY_BACKEND;

    sqlite3_bind_text(sel, 1, contact_id, (int)contact_id_len, SQLITE_STATIC);
    sqlite3_bind_text(sel, 2, emotion, (int)emotion_len, SQLITE_STATIC);
    sqlite3_bind_text(sel, 3, response_type, (int)response_type_len, SQLITE_STATIC);

    double old_score = 0.0;
    int old_count = 0;
    rc = sqlite3_step(sel);
    if (rc == SQLITE_ROW) {
        old_score = sqlite3_column_double(sel, 0);
        old_count = sqlite3_column_int(sel, 1);
    }
    sqlite3_finalize(sel);

    /* Running average: new_score = (old_score * old_count + engagement_score) / (old_count + 1) */
    int new_count = old_count + 1;
    double new_score =
        (old_score * (double)old_count + (double)engagement_score) / (double)new_count;

    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(db,
                            "INSERT OR REPLACE INTO comfort_patterns(contact_id,emotion,"
                            "response_type,engagement_score,sample_count) VALUES(?,?,?,?,?)",
                            -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_MEMORY_BACKEND;

    sqlite3_bind_text(stmt, 1, contact_id, (int)contact_id_len, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, emotion, (int)emotion_len, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, response_type, (int)response_type_len, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 4, new_score);
    sqlite3_bind_int(stmt, 5, new_count);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_MEMORY_BACKEND;
}

hu_error_t hu_comfort_pattern_get_preferred(hu_allocator_t *alloc, hu_memory_t *memory,
                                            const char *contact_id, size_t contact_id_len,
                                            const char *emotion, size_t emotion_len, char *out_type,
                                            size_t out_cap, size_t *out_len) {
    (void)alloc;
    if (!memory || !contact_id || contact_id_len == 0 || !emotion || emotion_len == 0 ||
        !out_type || out_cap == 0 || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    *out_len = 0;

    sqlite3 *db = hu_sqlite_memory_get_db(memory);
    if (!db)
        return HU_ERR_NOT_SUPPORTED;

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db,
                                "SELECT response_type FROM comfort_patterns "
                                "WHERE contact_id=? AND emotion=? AND sample_count>=2 "
                                "ORDER BY engagement_score DESC LIMIT 1",
                                -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return HU_ERR_MEMORY_BACKEND;

    sqlite3_bind_text(stmt, 1, contact_id, (int)contact_id_len, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, emotion, (int)emotion_len, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char *type = (const char *)sqlite3_column_text(stmt, 0);
        size_t type_len = type ? (size_t)sqlite3_column_bytes(stmt, 0) : 0;
        if (type && type_len > 0) {
            size_t copy = type_len < out_cap - 1 ? type_len : out_cap - 1;
            memcpy(out_type, type, copy);
            out_type[copy] = '\0';
            *out_len = copy;
        }
    }
    sqlite3_finalize(stmt);
    return HU_OK;
}

#else /* !HU_ENABLE_SQLITE */

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/memory.h"
#include "human/memory/comfort_patterns.h"

hu_error_t hu_comfort_pattern_record(hu_memory_t *memory, const char *contact_id,
                                     size_t contact_id_len, const char *emotion, size_t emotion_len,
                                     const char *response_type, size_t response_type_len,
                                     float engagement_score) {
    (void)memory;
    (void)contact_id;
    (void)contact_id_len;
    (void)emotion;
    (void)emotion_len;
    (void)response_type;
    (void)response_type_len;
    (void)engagement_score;
    return HU_ERR_NOT_SUPPORTED;
}

hu_error_t hu_comfort_pattern_get_preferred(hu_allocator_t *alloc, hu_memory_t *memory,
                                            const char *contact_id, size_t contact_id_len,
                                            const char *emotion, size_t emotion_len, char *out_type,
                                            size_t out_cap, size_t *out_len) {
    (void)alloc;
    (void)memory;
    (void)contact_id;
    (void)contact_id_len;
    (void)emotion;
    (void)emotion_len;
    (void)out_type;
    (void)out_cap;
    (void)out_len;
    return HU_ERR_NOT_SUPPORTED;
}

#endif /* HU_ENABLE_SQLITE */
