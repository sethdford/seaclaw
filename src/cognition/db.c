#include "human/cognition/db.h"

#ifdef HU_ENABLE_SQLITE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

hu_error_t hu_cognition_db_open(sqlite3 **out) {
    if (!out) return HU_ERR_INVALID_ARGUMENT;

#ifdef HU_IS_TEST
    const char *path = ":memory:";
#else
    static char path_buf[1024];
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(path_buf, sizeof(path_buf), "%s/.human/cognition.db", home);
    path_buf[sizeof(path_buf) - 1] = '\0';
    const char *path = path_buf;
#endif

    int rc = sqlite3_open(path, out);
    if (rc != SQLITE_OK) {
        if (*out) sqlite3_close(*out);
        *out = NULL;
        return HU_ERR_IO;
    }

    sqlite3_exec(*out, "PRAGMA journal_mode=WAL", NULL, NULL, NULL);
    sqlite3_exec(*out, "PRAGMA synchronous=NORMAL", NULL, NULL, NULL);

    return hu_cognition_db_ensure_schema(*out);
}

hu_error_t hu_cognition_db_ensure_schema(sqlite3 *db) {
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

        "CREATE TABLE IF NOT EXISTS episodic_patterns ("
        "  id TEXT PRIMARY KEY,"
        "  problem_type TEXT NOT NULL,"
        "  approach TEXT NOT NULL,"
        "  skills_used TEXT DEFAULT '',"
        "  outcome_quality REAL DEFAULT 0.5,"
        "  support_count INTEGER DEFAULT 1,"
        "  insight TEXT DEFAULT '',"
        "  session_id TEXT DEFAULT '',"
        "  timestamp TEXT NOT NULL DEFAULT (datetime('now'))"
        ")",

        "CREATE TABLE IF NOT EXISTS metacog_history ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  trace_id TEXT,"
        "  iteration INTEGER DEFAULT 0,"
        "  confidence REAL DEFAULT 0.0,"
        "  coherence REAL DEFAULT 0.0,"
        "  repetition REAL DEFAULT 0.0,"
        "  action TEXT DEFAULT 'none',"
        "  timestamp TEXT NOT NULL DEFAULT (datetime('now'))"
        ")",

        "CREATE INDEX IF NOT EXISTS idx_skill_inv_contact "
        "ON skill_invocations(contact_id, session_id)",

        "CREATE INDEX IF NOT EXISTS idx_episodic_type "
        "ON episodic_patterns(problem_type)",

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

void hu_cognition_db_close(sqlite3 *db) {
    if (db) sqlite3_close(db);
}

#endif /* HU_ENABLE_SQLITE */
