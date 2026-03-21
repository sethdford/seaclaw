#ifndef HU_COGNITION_DB_H
#define HU_COGNITION_DB_H

#include "human/core/error.h"
#include <stdbool.h>

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>

/* Open (or create) the shared cognition database at ~/.human/cognition.db.
 * Under HU_IS_TEST, uses ":memory:" instead of the filesystem.
 * Caller owns the returned handle; close with hu_cognition_db_close. */
hu_error_t hu_cognition_db_open(sqlite3 **out);

/* Ensure all cognition tables exist (skill_invocations, skill_profiles,
 * episodic_patterns, metacog_history). Idempotent — safe to call repeatedly. */
hu_error_t hu_cognition_db_ensure_schema(sqlite3 *db);

/* Close the cognition database handle. NULL-safe. */
void hu_cognition_db_close(sqlite3 *db);

#endif /* HU_ENABLE_SQLITE */

#endif /* HU_COGNITION_DB_H */
