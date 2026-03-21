#include "human/cognition/evolving.h"

#ifdef HU_ENABLE_SQLITE

#include <math.h>
#include <stdio.h>
#include <string.h>

hu_error_t hu_evolving_init_schema(sqlite3 *db) {
    if (!db) return HU_ERR_INVALID_ARGUMENT;

    static const char *ddl[] = {
        "CREATE TABLE IF NOT EXISTS skill_invocations ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  skill_name TEXT NOT NULL,"
        "  contact_id TEXT DEFAULT '',"
        "  session_id TEXT DEFAULT '',"
        "  timestamp TEXT NOT NULL DEFAULT (datetime('now')),"
        "  explicit INTEGER NOT NULL DEFAULT 0,"
        "  outcome INTEGER DEFAULT 0,"
        "  turn_count INTEGER DEFAULT 0"
        ")",

        "CREATE TABLE IF NOT EXISTS skill_profiles ("
        "  skill_name TEXT NOT NULL,"
        "  contact_id TEXT NOT NULL DEFAULT '',"
        "  total_invocations INTEGER DEFAULT 0,"
        "  positive_outcomes INTEGER DEFAULT 0,"
        "  negative_outcomes INTEGER DEFAULT 0,"
        "  decayed_score REAL DEFAULT 0.5,"
        "  last_updated TEXT DEFAULT (datetime('now')),"
        "  PRIMARY KEY (skill_name, contact_id)"
        ")",
        NULL
    };

    for (size_t i = 0; ddl[i]; i++) {
        char *err_msg = NULL;
        int rc = sqlite3_exec(db, ddl[i], NULL, NULL, &err_msg);
        if (rc != SQLITE_OK) {
            if (err_msg) sqlite3_free(err_msg);
            return HU_ERR_IO;
        }
    }
    return HU_OK;
}

hu_error_t hu_evolving_record_invocation(sqlite3 *db,
                                         const hu_skill_invocation_t *inv) {
    if (!db || !inv || !inv->skill_name) return HU_ERR_INVALID_ARGUMENT;

    const char *sql =
        "INSERT INTO skill_invocations "
        "(skill_name, contact_id, session_id, explicit, outcome) "
        "VALUES (?, ?, ?, ?, ?)";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return HU_ERR_IO;

    sqlite3_bind_text(stmt, 1, inv->skill_name, (int)inv->skill_name_len, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, inv->contact_id ? inv->contact_id : "",
                      inv->contact_id ? (int)inv->contact_id_len : 0, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, inv->session_id ? inv->session_id : "",
                      inv->session_id ? (int)inv->session_id_len : 0, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, inv->explicit_run ? 1 : 0);
    sqlite3_bind_int(stmt, 5, inv->outcome);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return HU_ERR_IO;

    /* Upsert profile aggregate */
    const char *upsert =
        "INSERT INTO skill_profiles (skill_name, contact_id, total_invocations, "
        "positive_outcomes, negative_outcomes, decayed_score, last_updated) "
        "VALUES (?, ?, 1, CASE WHEN ?3 > 0 THEN 1 ELSE 0 END, "
        "CASE WHEN ?3 < 0 THEN 1 ELSE 0 END, 0.5, datetime('now')) "
        "ON CONFLICT(skill_name, contact_id) DO UPDATE SET "
        "total_invocations = total_invocations + 1, "
        "positive_outcomes = positive_outcomes + CASE WHEN ?3 > 0 THEN 1 ELSE 0 END, "
        "negative_outcomes = negative_outcomes + CASE WHEN ?3 < 0 THEN 1 ELSE 0 END, "
        "last_updated = datetime('now')";

    rc = sqlite3_prepare_v2(db, upsert, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return HU_ERR_IO;

    sqlite3_bind_text(stmt, 1, inv->skill_name, (int)inv->skill_name_len, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, inv->contact_id ? inv->contact_id : "",
                      inv->contact_id ? (int)inv->contact_id_len : 0, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, inv->outcome);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_IO;
}

hu_error_t hu_evolving_record_implicit_exposure(sqlite3 *db,
                                                const char *const *skill_names,
                                                size_t count,
                                                const char *contact_id,
                                                size_t contact_id_len,
                                                const char *session_id,
                                                size_t session_id_len) {
    if (!db || !skill_names) return HU_ERR_INVALID_ARGUMENT;

    for (size_t i = 0; i < count; i++) {
        if (!skill_names[i]) continue;
        hu_skill_invocation_t inv = {
            .skill_name = skill_names[i],
            .skill_name_len = strlen(skill_names[i]),
            .contact_id = contact_id,
            .contact_id_len = contact_id_len,
            .session_id = session_id,
            .session_id_len = session_id_len,
            .explicit_run = false,
            .outcome = HU_SKILL_OUTCOME_NEUTRAL,
        };
        hu_error_t err = hu_evolving_record_invocation(db, &inv);
        if (err != HU_OK) return err;
    }
    return HU_OK;
}

hu_error_t hu_evolving_collect_outcome(sqlite3 *db,
                                       const char *contact_id, size_t contact_id_len,
                                       const char *session_id, size_t session_id_len,
                                       int outcome) {
    if (!db) return HU_ERR_INVALID_ARGUMENT;
    if (outcome == 0) return HU_OK;

    const char *sql =
        "UPDATE skill_invocations SET outcome = ? "
        "WHERE contact_id = ? AND session_id = ? "
        "AND outcome = 0 "
        "AND id IN (SELECT id FROM skill_invocations "
        "           WHERE contact_id = ? AND session_id = ? "
        "           ORDER BY id DESC LIMIT 20)";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return HU_ERR_IO;

    sqlite3_bind_int(stmt, 1, outcome);
    sqlite3_bind_text(stmt, 2, contact_id ? contact_id : "",
                      contact_id ? (int)contact_id_len : 0, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, session_id ? session_id : "",
                      session_id ? (int)session_id_len : 0, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, contact_id ? contact_id : "",
                      contact_id ? (int)contact_id_len : 0, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, session_id ? session_id : "",
                      session_id ? (int)session_id_len : 0, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_IO;
}

hu_error_t hu_evolving_load_profiles(sqlite3 *db, hu_allocator_t *alloc,
                                     const char *contact_id, size_t contact_id_len,
                                     hu_skill_profile_t **out, size_t *out_count) {
    if (!db || !alloc || !out || !out_count) return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_count = 0;

    const char *sql =
        "SELECT skill_name, contact_id, total_invocations, "
        "positive_outcomes, negative_outcomes, decayed_score "
        "FROM skill_profiles WHERE contact_id = ? "
        "ORDER BY total_invocations DESC";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return HU_ERR_IO;

    sqlite3_bind_text(stmt, 1, contact_id ? contact_id : "",
                      contact_id ? (int)contact_id_len : 0, SQLITE_STATIC);

    size_t cap = 32;
    hu_skill_profile_t *arr = alloc->alloc(alloc->ctx, cap * sizeof(hu_skill_profile_t));
    if (!arr) { sqlite3_finalize(stmt); return HU_ERR_OUT_OF_MEMORY; }
    size_t count = 0;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (count >= cap) {
            size_t new_cap = cap * 2;
            hu_skill_profile_t *new_arr = alloc->alloc(alloc->ctx, new_cap * sizeof(hu_skill_profile_t));
            if (!new_arr) {
                hu_evolving_free_profiles(alloc, arr, count);
                sqlite3_finalize(stmt);
                return HU_ERR_OUT_OF_MEMORY;
            }
            memcpy(new_arr, arr, count * sizeof(hu_skill_profile_t));
            alloc->free(alloc->ctx, arr, cap * sizeof(hu_skill_profile_t));
            arr = new_arr;
            cap = new_cap;
        }

        hu_skill_profile_t *p = &arr[count];
        const char *sn = (const char *)sqlite3_column_text(stmt, 0);
        int sn_len = sqlite3_column_bytes(stmt, 0);
        const char *ci = (const char *)sqlite3_column_text(stmt, 1);
        int ci_len = sqlite3_column_bytes(stmt, 1);

        p->skill_name = alloc->alloc(alloc->ctx, (size_t)sn_len + 1);
        if (!p->skill_name) {
            hu_evolving_free_profiles(alloc, arr, count);
            sqlite3_finalize(stmt);
            return HU_ERR_OUT_OF_MEMORY;
        }
        memcpy(p->skill_name, sn, (size_t)sn_len);
        p->skill_name[sn_len] = '\0';
        p->skill_name_len = (size_t)sn_len;

        p->contact_id = alloc->alloc(alloc->ctx, (size_t)ci_len + 1);
        if (!p->contact_id) {
            alloc->free(alloc->ctx, p->skill_name, (size_t)sn_len + 1);
            hu_evolving_free_profiles(alloc, arr, count);
            sqlite3_finalize(stmt);
            return HU_ERR_OUT_OF_MEMORY;
        }
        memcpy(p->contact_id, ci, (size_t)ci_len);
        p->contact_id[ci_len] = '\0';
        p->contact_id_len = (size_t)ci_len;

        p->total_invocations = (uint32_t)sqlite3_column_int(stmt, 2);
        p->positive_outcomes = (uint32_t)sqlite3_column_int(stmt, 3);
        p->negative_outcomes = (uint32_t)sqlite3_column_int(stmt, 4);
        p->decayed_score = sqlite3_column_double(stmt, 5);
        count++;
    }
    sqlite3_finalize(stmt);

    *out = arr;
    *out_count = count;
    return HU_OK;
}

void hu_evolving_free_profiles(hu_allocator_t *alloc,
                               hu_skill_profile_t *profiles, size_t count) {
    if (!alloc || !profiles) return;
    for (size_t i = 0; i < count; i++) {
        if (profiles[i].skill_name)
            alloc->free(alloc->ctx, profiles[i].skill_name, profiles[i].skill_name_len + 1);
        if (profiles[i].contact_id)
            alloc->free(alloc->ctx, profiles[i].contact_id, profiles[i].contact_id_len + 1);
    }
    alloc->free(alloc->ctx, profiles, count * sizeof(hu_skill_profile_t));
}

double hu_evolving_compute_weight(const hu_skill_profile_t *profile) {
    if (!profile || profile->total_invocations == 0) return 1.0;

    double pos = (double)profile->positive_outcomes;
    double neg = (double)profile->negative_outcomes;
    double total = (double)profile->total_invocations;

    /*
     * Net success rate centered on 1.0:
     *   - All positive (1.0 net) -> weight ~1.5
     *   - All negative (0.0 net) -> weight ~0.5
     *   - Balanced                -> weight ~1.0
     * Shrink toward 1.0 with low sample size (Bayesian prior).
     */
    double prior_weight = 2.0;
    double smoothed_pos = (pos + prior_weight * 0.5) / (total + prior_weight);
    double weight = 0.5 + smoothed_pos;

    if (weight < 0.3) weight = 0.3;
    if (weight > 1.8) weight = 1.8;

    if (neg > pos && total >= 3) {
        weight *= 0.8;
    }

    return weight;
}

hu_error_t hu_evolving_rebuild_profiles(sqlite3 *db) {
    if (!db) return HU_ERR_INVALID_ARGUMENT;

    const char *sql =
        "INSERT OR REPLACE INTO skill_profiles "
        "(skill_name, contact_id, total_invocations, positive_outcomes, "
        "negative_outcomes, decayed_score, last_updated) "
        "SELECT skill_name, contact_id, "
        "COUNT(*), "
        "SUM(CASE WHEN outcome > 0 THEN 1 ELSE 0 END), "
        "SUM(CASE WHEN outcome < 0 THEN 1 ELSE 0 END), "
        "0.5, datetime('now') "
        "FROM skill_invocations "
        "GROUP BY skill_name, contact_id";

    char *err_msg = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg) sqlite3_free(err_msg);
        return HU_ERR_IO;
    }
    return HU_OK;
}

#endif /* HU_ENABLE_SQLITE */
