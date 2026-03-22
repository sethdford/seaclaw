#ifndef HU_COGNITION_DB_H
#define HU_COGNITION_DB_H

#include "human/core/error.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>

/* Optional telemetry columns for metacog_history (NULL-safe insert). */
typedef struct hu_metacog_history_extra {
    uint64_t prompt_tokens;
    uint64_t completion_tokens;
    float logprob_mean; /* use < 0 (e.g. -1) when unset */
    float risk_score;
} hu_metacog_history_extra_t;

/* Open (or create) the shared cognition database at ~/.human/cognition.db.
 * Under HU_IS_TEST, uses ":memory:" instead of the filesystem.
 * Caller owns the returned handle; close with hu_cognition_db_close. */
hu_error_t hu_cognition_db_open(sqlite3 **out);

/* Ensure all cognition tables exist (skill_invocations, skill_profiles,
 * episodic_patterns, metacog_history). Idempotent — safe to call repeatedly. */
hu_error_t hu_cognition_db_ensure_schema(sqlite3 *db);

/* Close the cognition database handle. NULL-safe. */
void hu_cognition_db_close(sqlite3 *db);

/* Persist one metacognition snapshot (best-effort; ignores failures). */
hu_error_t hu_metacog_history_insert(
    sqlite3 *db, const char *trace_id, int iteration, float confidence, float coherence,
    float repetition, float stuck_score, float satisfaction_proxy, float trajectory_confidence,
    const char *action, const char *difficulty, int regen_applied,
    const hu_metacog_history_extra_t *extra_opt);

/* Set outcome_proxy for the latest row matching trace_id (follow-up labeling). */
hu_error_t hu_metacog_history_update_outcome(sqlite3 *db, const char *trace_id,
                                              float outcome_proxy);

#endif /* HU_ENABLE_SQLITE */

#endif /* HU_COGNITION_DB_H */
