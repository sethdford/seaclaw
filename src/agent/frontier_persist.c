#include "human/agent/frontier_persist.h"
#include "human/agent.h"
#include "human/agent/growth_narrative.h"
#include "human/core/string.h"
#include <stdio.h>
#include <string.h>

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>
#include <time.h>

static hu_error_t frontier_schema_exec(sqlite3 *db, const char *sql) {
    char *errmsg = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        if (errmsg)
            sqlite3_free(errmsg);
        return HU_ERR_IO;
    }
    return HU_OK;
}

/* ALTER ADD COLUMN: duplicate column is benign on repeated ensure_table. */
static hu_error_t frontier_schema_exec_add_column(sqlite3 *db, const char *sql) {
    char *errmsg = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (rc == SQLITE_OK)
        return HU_OK;
    if (errmsg && strstr(errmsg, "duplicate column") != NULL) {
        sqlite3_free(errmsg);
        return HU_OK;
    }
    if (errmsg)
        sqlite3_free(errmsg);
    return HU_ERR_IO;
}

hu_error_t hu_frontier_persist_ensure_table(sqlite3 *db) {
    if (!db) return HU_ERR_INVALID_ARGUMENT;
    static const char sql[] =
        "CREATE TABLE IF NOT EXISTS frontier_state ("
        "  contact_id TEXT PRIMARY KEY,"
        "  somatic_energy REAL DEFAULT 0.8,"
        "  somatic_social_battery REAL DEFAULT 0.9,"
        "  somatic_focus REAL DEFAULT 0.85,"
        "  somatic_arousal REAL DEFAULT 0.5,"
        "  somatic_load REAL DEFAULT 0.0,"
        "  attachment_style INTEGER DEFAULT 0,"
        "  attachment_confidence REAL DEFAULT 0.0,"
        "  attachment_proximity REAL DEFAULT 0.5,"
        "  attachment_safe_haven REAL DEFAULT 0.5,"
        "  attachment_secure_base REAL DEFAULT 0.5,"
        "  attachment_separation REAL DEFAULT 0.3,"
        "  novelty_cooldown INTEGER DEFAULT 3,"
        "  novelty_turns_since INTEGER DEFAULT 0,"
        "  novelty_seen_hashes BLOB,"
        "  novelty_seen_count INTEGER DEFAULT 0,"
        "  rupture_stage INTEGER DEFAULT 0,"
        "  rupture_severity REAL DEFAULT 0.0,"
        "  rupture_turns_since INTEGER DEFAULT 0,"
        "  trust_competence REAL DEFAULT 0.5,"
        "  trust_benevolence REAL DEFAULT 0.5,"
        "  trust_integrity REAL DEFAULT 0.5,"
        "  trust_predictability REAL DEFAULT 0.5,"
        "  trust_transparency REAL DEFAULT 0.5,"
        "  trust_composite REAL DEFAULT 0.5,"
        "  trust_level INTEGER DEFAULT 0,"
        "  trust_interactions INTEGER DEFAULT 0,"
        "  rel_stage INTEGER DEFAULT 0,"
        "  rel_session_count INTEGER DEFAULT 0,"
        "  rel_total_turns INTEGER DEFAULT 0,"
        "  updated_at INTEGER DEFAULT 0"
        ")";
    char *errmsg = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        if (errmsg) sqlite3_free(errmsg);
        return HU_ERR_IO;
    }
    /* Migrate older tables */
    if (frontier_schema_exec_add_column(db, "ALTER TABLE frontier_state ADD COLUMN novelty_seen_hashes BLOB") != HU_OK)
        return HU_ERR_IO;
    if (frontier_schema_exec_add_column(db, "ALTER TABLE frontier_state ADD COLUMN novelty_seen_count INTEGER DEFAULT 0") != HU_OK)
        return HU_ERR_IO;
    if (frontier_schema_exec_add_column(db, "ALTER TABLE frontier_state ADD COLUMN rupture_trigger TEXT") != HU_OK)
        return HU_ERR_IO;
    if (frontier_schema_exec_add_column(db, "ALTER TABLE frontier_state ADD COLUMN trust_competence REAL DEFAULT 0.5") != HU_OK)
        return HU_ERR_IO;
    if (frontier_schema_exec_add_column(db, "ALTER TABLE frontier_state ADD COLUMN trust_benevolence REAL DEFAULT 0.5") != HU_OK)
        return HU_ERR_IO;
    if (frontier_schema_exec_add_column(db, "ALTER TABLE frontier_state ADD COLUMN trust_integrity REAL DEFAULT 0.5") != HU_OK)
        return HU_ERR_IO;
    if (frontier_schema_exec_add_column(db, "ALTER TABLE frontier_state ADD COLUMN trust_predictability REAL DEFAULT 0.5") != HU_OK)
        return HU_ERR_IO;
    if (frontier_schema_exec_add_column(db, "ALTER TABLE frontier_state ADD COLUMN trust_transparency REAL DEFAULT 0.5") != HU_OK)
        return HU_ERR_IO;
    if (frontier_schema_exec_add_column(db, "ALTER TABLE frontier_state ADD COLUMN trust_composite REAL DEFAULT 0.5") != HU_OK)
        return HU_ERR_IO;
    if (frontier_schema_exec_add_column(db, "ALTER TABLE frontier_state ADD COLUMN trust_level INTEGER DEFAULT 0") != HU_OK)
        return HU_ERR_IO;
    if (frontier_schema_exec_add_column(db, "ALTER TABLE frontier_state ADD COLUMN trust_interactions INTEGER DEFAULT 0") != HU_OK)
        return HU_ERR_IO;
    if (frontier_schema_exec_add_column(db, "ALTER TABLE frontier_state ADD COLUMN rel_stage INTEGER DEFAULT 0") != HU_OK)
        return HU_ERR_IO;
    if (frontier_schema_exec_add_column(db, "ALTER TABLE frontier_state ADD COLUMN rel_session_count INTEGER DEFAULT 0") != HU_OK)
        return HU_ERR_IO;
    if (frontier_schema_exec_add_column(db, "ALTER TABLE frontier_state ADD COLUMN rel_total_turns INTEGER DEFAULT 0") != HU_OK)
        return HU_ERR_IO;

    static const char growth_sql[] =
        "CREATE TABLE IF NOT EXISTS growth_records ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  contact_id TEXT NOT NULL,"
        "  type TEXT NOT NULL,"  /* 'observation' or 'milestone' */
        "  text TEXT NOT NULL,"
        "  evidence TEXT,"
        "  confidence REAL DEFAULT 0.0,"
        "  significance REAL DEFAULT 0.0,"
        "  surfaced INTEGER DEFAULT 0,"
        "  timestamp INTEGER DEFAULT 0"
        ")";
    if (frontier_schema_exec(db, growth_sql) != HU_OK)
        return HU_ERR_IO;
    return HU_OK;
}

hu_error_t hu_frontier_persist_save(hu_allocator_t *alloc, sqlite3 *db,
                                    const char *contact_id, size_t contact_id_len,
                                    const hu_frontier_state_t *state) {
    (void)alloc;
    if (!db || !contact_id || contact_id_len == 0 || !state)
        return HU_ERR_INVALID_ARGUMENT;
    if (!state->initialized)
        return HU_OK;

    static const char sql[] =
        "INSERT OR REPLACE INTO frontier_state ("
        "  contact_id, somatic_energy, somatic_social_battery, somatic_focus,"
        "  somatic_arousal, somatic_load, attachment_style, attachment_confidence,"
        "  attachment_proximity, attachment_safe_haven, attachment_secure_base,"
        "  attachment_separation, novelty_cooldown, novelty_turns_since,"
        "  novelty_seen_hashes, novelty_seen_count,"
        "  rupture_stage, rupture_severity, rupture_turns_since, rupture_trigger,"
        "  trust_competence, trust_benevolence, trust_integrity,"
        "  trust_predictability, trust_transparency, trust_composite,"
        "  trust_level, trust_interactions,"
        "  rel_stage, rel_session_count, rel_total_turns,"
        "  updated_at"
        ") VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18,?19,?20,"
        "?21,?22,?23,?24,?25,?26,?27,?28,?29,?30,?31,?32)";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return HU_ERR_IO;

    sqlite3_bind_text(stmt, 1, contact_id, (int)contact_id_len, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 2, (double)state->somatic.energy);
    sqlite3_bind_double(stmt, 3, (double)state->somatic.social_battery);
    sqlite3_bind_double(stmt, 4, (double)state->somatic.focus);
    sqlite3_bind_double(stmt, 5, (double)state->somatic.arousal);
    sqlite3_bind_double(stmt, 6, (double)state->somatic.conversation_load_accumulated);
    sqlite3_bind_int(stmt, 7, (int)state->attachment.user_style);
    sqlite3_bind_double(stmt, 8, (double)state->attachment.user_confidence);
    sqlite3_bind_double(stmt, 9, (double)state->attachment.proximity_seeking);
    sqlite3_bind_double(stmt, 10, (double)state->attachment.safe_haven_usage);
    sqlite3_bind_double(stmt, 11, (double)state->attachment.secure_base_behavior);
    sqlite3_bind_double(stmt, 12, (double)state->attachment.separation_distress);
    sqlite3_bind_int(stmt, 13, (int)state->novelty.cooldown_turns);
    sqlite3_bind_int(stmt, 14, (int)state->novelty.turns_since_last_surprise);
    size_t blob_bytes = state->novelty.seen_count * sizeof(uint32_t);
    if (state->novelty.seen_count > 0)
        sqlite3_bind_blob(stmt, 15, state->novelty.seen_hashes, (int)blob_bytes, SQLITE_STATIC);
    else
        sqlite3_bind_null(stmt, 15);
    sqlite3_bind_int(stmt, 16, (int)state->novelty.seen_count);
    sqlite3_bind_int(stmt, 17, (int)state->rupture.stage);
    sqlite3_bind_double(stmt, 18, (double)state->rupture.severity);
    sqlite3_bind_int(stmt, 19, (int)state->rupture.turns_since);
    if (state->rupture.trigger_summary)
        sqlite3_bind_text(stmt, 20, state->rupture.trigger_summary,
                          (int)strlen(state->rupture.trigger_summary), SQLITE_STATIC);
    else
        sqlite3_bind_null(stmt, 20);
    sqlite3_bind_double(stmt, 21, (double)state->trust.dimensions.competence);
    sqlite3_bind_double(stmt, 22, (double)state->trust.dimensions.benevolence);
    sqlite3_bind_double(stmt, 23, (double)state->trust.dimensions.integrity);
    sqlite3_bind_double(stmt, 24, (double)state->trust.dimensions.predictability);
    sqlite3_bind_double(stmt, 25, (double)state->trust.dimensions.transparency);
    sqlite3_bind_double(stmt, 26, (double)state->trust.composite);
    sqlite3_bind_int(stmt, 27, (int)state->trust.level);
    sqlite3_bind_int(stmt, 28, (int)state->trust.interaction_count);
    /* Preserve existing relationship data if the row already exists.
     * Relationship is saved separately via save_relationship, but INSERT OR
     * REPLACE would overwrite the whole row; use COALESCE to keep old values. */
    {
        int old_stage = 0, old_sc = 0, old_tt = 0;
        static const char rsel[] =
            "SELECT rel_stage, rel_session_count, rel_total_turns "
            "FROM frontier_state WHERE contact_id = ?1";
        sqlite3_stmt *rs = NULL;
        if (sqlite3_prepare_v2(db, rsel, -1, &rs, NULL) == SQLITE_OK) {
            sqlite3_bind_text(rs, 1, contact_id, (int)contact_id_len, SQLITE_STATIC);
            if (sqlite3_step(rs) == SQLITE_ROW) {
                old_stage = sqlite3_column_int(rs, 0);
                old_sc = sqlite3_column_int(rs, 1);
                old_tt = sqlite3_column_int(rs, 2);
            }
            sqlite3_finalize(rs);
        }
        sqlite3_bind_int(stmt, 29, old_stage);
        sqlite3_bind_int(stmt, 30, old_sc);
        sqlite3_bind_int(stmt, 31, old_tt);
    }
    sqlite3_bind_int64(stmt, 32, (sqlite3_int64)time(NULL));

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? HU_OK : HU_ERR_IO;
}

hu_error_t hu_frontier_persist_load(hu_allocator_t *alloc, sqlite3 *db,
                                    const char *contact_id, size_t contact_id_len,
                                    hu_frontier_state_t *state) {
    (void)alloc;
    if (!db || !contact_id || contact_id_len == 0 || !state)
        return HU_ERR_INVALID_ARGUMENT;

    static const char sql[] =
        "SELECT somatic_energy, somatic_social_battery, somatic_focus,"
        "  somatic_arousal, somatic_load, attachment_style, attachment_confidence,"
        "  attachment_proximity, attachment_safe_haven, attachment_secure_base,"
        "  attachment_separation, novelty_cooldown, novelty_turns_since,"
        "  rupture_stage, rupture_severity, rupture_turns_since,"
        "  novelty_seen_hashes, novelty_seen_count, rupture_trigger,"
        "  trust_competence, trust_benevolence, trust_integrity,"
        "  trust_predictability, trust_transparency, trust_composite,"
        "  trust_level, trust_interactions"
        " FROM frontier_state WHERE contact_id = ?1";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return HU_ERR_IO;

    sqlite3_bind_text(stmt, 1, contact_id, (int)contact_id_len, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return HU_ERR_NOT_FOUND;
    }

    state->somatic.energy = (float)sqlite3_column_double(stmt, 0);
    state->somatic.social_battery = (float)sqlite3_column_double(stmt, 1);
    state->somatic.focus = (float)sqlite3_column_double(stmt, 2);
    state->somatic.arousal = (float)sqlite3_column_double(stmt, 3);
    state->somatic.conversation_load_accumulated = (float)sqlite3_column_double(stmt, 4);
    state->attachment.user_style = (hu_attachment_style_t)sqlite3_column_int(stmt, 5);
    state->attachment.user_confidence = (float)sqlite3_column_double(stmt, 6);
    state->attachment.proximity_seeking = (float)sqlite3_column_double(stmt, 7);
    state->attachment.safe_haven_usage = (float)sqlite3_column_double(stmt, 8);
    state->attachment.secure_base_behavior = (float)sqlite3_column_double(stmt, 9);
    state->attachment.separation_distress = (float)sqlite3_column_double(stmt, 10);
    state->novelty.cooldown_turns = (uint32_t)sqlite3_column_int(stmt, 11);
    state->novelty.turns_since_last_surprise = (uint32_t)sqlite3_column_int(stmt, 12);
    state->rupture.stage = (hu_rupture_stage_t)sqlite3_column_int(stmt, 13);
    state->rupture.severity = (float)sqlite3_column_double(stmt, 14);
    state->rupture.turns_since = (uint32_t)sqlite3_column_int(stmt, 15);

    size_t seen_n = (size_t)sqlite3_column_int(stmt, 17);
    if (seen_n > HU_NOVELTY_MAX_SEEN) seen_n = HU_NOVELTY_MAX_SEEN;
    const void *blob = sqlite3_column_blob(stmt, 16);
    int blob_len = sqlite3_column_bytes(stmt, 16);
    if (blob && blob_len > 0 && seen_n > 0) {
        size_t copy_n = seen_n;
        if ((size_t)blob_len < copy_n * sizeof(uint32_t))
            copy_n = (size_t)blob_len / sizeof(uint32_t);
        memcpy(state->novelty.seen_hashes, blob, copy_n * sizeof(uint32_t));
        state->novelty.seen_count = copy_n;
    } else {
        state->novelty.seen_count = 0;
    }

    const char *trigger = (const char *)sqlite3_column_text(stmt, 18);
    if (trigger && alloc) {
        hu_str_free(alloc, state->rupture.trigger_summary);
        state->rupture.trigger_summary = hu_strndup(alloc, trigger, strlen(trigger));
    }

    /* Trust calibration — columns 19–25 */
    if (sqlite3_column_type(stmt, 19) != SQLITE_NULL) {
        state->trust.dimensions.competence = (float)sqlite3_column_double(stmt, 19);
        state->trust.dimensions.benevolence = (float)sqlite3_column_double(stmt, 20);
        state->trust.dimensions.integrity = (float)sqlite3_column_double(stmt, 21);
        state->trust.dimensions.predictability = (float)sqlite3_column_double(stmt, 22);
        state->trust.dimensions.transparency = (float)sqlite3_column_double(stmt, 23);
        state->trust.composite = (float)sqlite3_column_double(stmt, 24);
        state->trust.level = (hu_tcal_level_t)sqlite3_column_int(stmt, 25);
        state->trust.interaction_count = (size_t)sqlite3_column_int(stmt, 26);
    }

    sqlite3_finalize(stmt);
    return HU_OK;
}

hu_error_t hu_frontier_persist_save_growth(hu_allocator_t *alloc, sqlite3 *db,
                                           const char *contact_id, size_t contact_id_len,
                                           const hu_frontier_state_t *state) {
    (void)alloc;
    if (!db || !contact_id || contact_id_len == 0 || !state)
        return HU_ERR_INVALID_ARGUMENT;
    if (!state->initialized)
        return HU_OK;

    static const char *del_sql = "DELETE FROM growth_records WHERE contact_id = ?1";
    sqlite3_stmt *del_stmt = NULL;
    int del_rc = sqlite3_prepare_v2(db, del_sql, -1, &del_stmt, NULL);
    if (del_rc != SQLITE_OK)
        return HU_ERR_IO;
    sqlite3_bind_text(del_stmt, 1, contact_id, (int)contact_id_len, SQLITE_STATIC);
    del_rc = sqlite3_step(del_stmt);
    sqlite3_finalize(del_stmt);
    if (del_rc != SQLITE_DONE)
        return HU_ERR_IO;

    static const char ins[] =
        "INSERT INTO growth_records (contact_id, type, text, evidence, confidence, "
        "significance, surfaced, timestamp) VALUES (?1,?2,?3,?4,?5,?6,?7,?8)";

    sqlite3_stmt *ins_stmt = NULL;
    if (sqlite3_prepare_v2(db, ins, -1, &ins_stmt, NULL) != SQLITE_OK)
        return HU_ERR_IO;

    const hu_growth_narrative_t *gn = &state->growth;
    for (size_t i = 0; i < gn->observation_count; i++) {
        const hu_growth_observation_t *o = &gn->observations[i];
        sqlite3_bind_text(ins_stmt, 1, contact_id, (int)contact_id_len, SQLITE_STATIC);
        sqlite3_bind_text(ins_stmt, 2, "observation", 11, SQLITE_STATIC);
        sqlite3_bind_text(ins_stmt, 3, o->observation ? o->observation : "",
                          o->observation ? (int)strlen(o->observation) : 0, SQLITE_STATIC);
        sqlite3_bind_text(ins_stmt, 4, o->evidence ? o->evidence : "",
                          o->evidence ? (int)strlen(o->evidence) : 0, SQLITE_STATIC);
        sqlite3_bind_double(ins_stmt, 5, (double)o->confidence);
        sqlite3_bind_double(ins_stmt, 6, 0.0);
        sqlite3_bind_int(ins_stmt, 7, o->surfaced ? 1 : 0);
        sqlite3_bind_int64(ins_stmt, 8, (sqlite3_int64)o->observed_at);
        int step_rc = sqlite3_step(ins_stmt);
        sqlite3_reset(ins_stmt);
        if (step_rc != SQLITE_DONE) {
            sqlite3_finalize(ins_stmt);
            return HU_ERR_IO;
        }
    }
    for (size_t i = 0; i < gn->milestone_count; i++) {
        const hu_relational_milestone_t *m = &gn->milestones[i];
        sqlite3_bind_text(ins_stmt, 1, contact_id, (int)contact_id_len, SQLITE_STATIC);
        sqlite3_bind_text(ins_stmt, 2, "milestone", 9, SQLITE_STATIC);
        sqlite3_bind_text(ins_stmt, 3, m->description ? m->description : "",
                          m->description ? (int)strlen(m->description) : 0, SQLITE_STATIC);
        sqlite3_bind_null(ins_stmt, 4);
        sqlite3_bind_double(ins_stmt, 5, 0.0);
        sqlite3_bind_double(ins_stmt, 6, (double)m->significance);
        sqlite3_bind_int(ins_stmt, 7, 0);
        sqlite3_bind_int64(ins_stmt, 8, (sqlite3_int64)m->timestamp);
        int step_rc = sqlite3_step(ins_stmt);
        sqlite3_reset(ins_stmt);
        if (step_rc != SQLITE_DONE) {
            sqlite3_finalize(ins_stmt);
            return HU_ERR_IO;
        }
    }
    sqlite3_finalize(ins_stmt);
    return HU_OK;
}

hu_error_t hu_frontier_persist_load_growth(hu_allocator_t *alloc, sqlite3 *db,
                                           const char *contact_id, size_t contact_id_len,
                                           hu_frontier_state_t *state) {
    if (!alloc || !db || !contact_id || contact_id_len == 0 || !state)
        return HU_ERR_INVALID_ARGUMENT;

    static const char sel[] =
        "SELECT type, text, evidence, confidence, significance, surfaced, timestamp "
        "FROM growth_records WHERE contact_id = ?1 ORDER BY timestamp DESC LIMIT 32";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sel, -1, &stmt, NULL) != SQLITE_OK)
        return HU_ERR_IO;
    sqlite3_bind_text(stmt, 1, contact_id, (int)contact_id_len, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *type = (const char *)sqlite3_column_text(stmt, 0);
        const char *text = (const char *)sqlite3_column_text(stmt, 1);
        if (!type || !text) continue;

        if (strcmp(type, "observation") == 0 &&
            state->growth.observation_count < HU_GROWTH_MAX_OBSERVATIONS) {
            const char *ev = (const char *)sqlite3_column_text(stmt, 2);
            float conf = (float)sqlite3_column_double(stmt, 3);
            bool surfaced = sqlite3_column_int(stmt, 5) != 0;
            uint64_t ts = (uint64_t)sqlite3_column_int64(stmt, 6);
            hu_growth_narrative_add_observation(alloc, &state->growth,
                contact_id, text, ev, conf, ts);
            if (surfaced && state->growth.observation_count > 0)
                state->growth.observations[state->growth.observation_count - 1].surfaced = true;
        } else if (strcmp(type, "milestone") == 0 &&
                   state->growth.milestone_count < HU_GROWTH_MAX_MILESTONES) {
            float sig = (float)sqlite3_column_double(stmt, 4);
            uint64_t ts = (uint64_t)sqlite3_column_int64(stmt, 6);
            hu_growth_narrative_add_milestone(alloc, &state->growth,
                contact_id, text, ts, sig);
        }
    }
    sqlite3_finalize(stmt);
    return HU_OK;
}

hu_error_t hu_frontier_persist_save_relationship(sqlite3 *db, const char *contact_id,
                                                  size_t contact_id_len, int stage,
                                                  int session_count, int total_turns) {
    if (!db || !contact_id || contact_id_len == 0)
        return HU_ERR_INVALID_ARGUMENT;
    hu_frontier_persist_ensure_table(db);

    /* UPDATE first — fast path when the row already exists (the common case,
     * since hu_frontier_persist_save runs before this call). */
    static const char upd[] =
        "UPDATE frontier_state SET rel_stage = ?1, rel_session_count = ?2, "
        "rel_total_turns = ?3 WHERE contact_id = ?4";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, upd, -1, &stmt, NULL) != SQLITE_OK)
        return HU_ERR_IO;
    sqlite3_bind_int(stmt, 1, stage);
    sqlite3_bind_int(stmt, 2, session_count);
    sqlite3_bind_int(stmt, 3, total_turns);
    sqlite3_bind_text(stmt, 4, contact_id, (int)contact_id_len, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        return HU_ERR_IO;

    /* If UPDATE touched no rows, INSERT a minimal row with relationship data. */
    if (sqlite3_changes(db) == 0) {
        static const char ins[] =
            "INSERT OR IGNORE INTO frontier_state (contact_id, rel_stage, "
            "rel_session_count, rel_total_turns) VALUES (?1, ?2, ?3, ?4)";
        stmt = NULL;
        if (sqlite3_prepare_v2(db, ins, -1, &stmt, NULL) != SQLITE_OK)
            return HU_ERR_IO;
        sqlite3_bind_text(stmt, 1, contact_id, (int)contact_id_len, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, stage);
        sqlite3_bind_int(stmt, 3, session_count);
        sqlite3_bind_int(stmt, 4, total_turns);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE)
            return HU_ERR_IO;
    }
    return HU_OK;
}

hu_error_t hu_frontier_persist_load_relationship(sqlite3 *db, const char *contact_id,
                                                  size_t contact_id_len, int *stage,
                                                  int *session_count, int *total_turns) {
    if (!db || !contact_id || !stage || !session_count || !total_turns)
        return HU_ERR_INVALID_ARGUMENT;
    *stage = 0;
    *session_count = 0;
    *total_turns = 0;
    static const char sql[] =
        "SELECT rel_stage, rel_session_count, rel_total_turns "
        "FROM frontier_state WHERE contact_id = ?1";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return HU_ERR_IO;
    sqlite3_bind_text(stmt, 1, contact_id, (int)contact_id_len, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *stage = sqlite3_column_int(stmt, 0);
        *session_count = sqlite3_column_int(stmt, 1);
        *total_turns = sqlite3_column_int(stmt, 2);
    }
    sqlite3_finalize(stmt);
    return HU_OK;
}

#else /* !HU_ENABLE_SQLITE */

hu_error_t hu_frontier_persist_ensure_table(struct sqlite3 *db) {
    (void)db;
    return HU_ERR_NOT_SUPPORTED;
}
hu_error_t hu_frontier_persist_save(hu_allocator_t *alloc, struct sqlite3 *db,
                                    const char *contact_id, size_t contact_id_len,
                                    const struct hu_frontier_state *state) {
    (void)alloc; (void)db; (void)contact_id; (void)contact_id_len; (void)state;
    return HU_ERR_NOT_SUPPORTED;
}
hu_error_t hu_frontier_persist_load(hu_allocator_t *alloc, struct sqlite3 *db,
                                    const char *contact_id, size_t contact_id_len,
                                    struct hu_frontier_state *state) {
    (void)alloc; (void)db; (void)contact_id; (void)contact_id_len; (void)state;
    return HU_ERR_NOT_SUPPORTED;
}
hu_error_t hu_frontier_persist_save_growth(hu_allocator_t *alloc, struct sqlite3 *db,
                                           const char *contact_id, size_t contact_id_len,
                                           const struct hu_frontier_state *state) {
    (void)alloc; (void)db; (void)contact_id; (void)contact_id_len; (void)state;
    return HU_ERR_NOT_SUPPORTED;
}
hu_error_t hu_frontier_persist_load_growth(hu_allocator_t *alloc, struct sqlite3 *db,
                                           const char *contact_id, size_t contact_id_len,
                                           struct hu_frontier_state *state) {
    (void)alloc; (void)db; (void)contact_id; (void)contact_id_len; (void)state;
    return HU_ERR_NOT_SUPPORTED;
}
#endif
