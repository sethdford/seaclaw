#ifndef HU_COGNITION_EVOLVING_H
#define HU_COGNITION_EVOLVING_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>

/* Outcome heuristic values */
#define HU_SKILL_OUTCOME_NEGATIVE (-1)
#define HU_SKILL_OUTCOME_NEUTRAL    0
#define HU_SKILL_OUTCOME_POSITIVE   1

typedef struct hu_skill_invocation {
    const char *skill_name;
    size_t skill_name_len;
    const char *contact_id;
    size_t contact_id_len;
    const char *session_id;
    size_t session_id_len;
    bool explicit_run;      /* true = skill_run tool, false = catalog exposure */
    int outcome;            /* -1/0/+1 */
} hu_skill_invocation_t;

typedef struct hu_skill_profile {
    char *skill_name;
    size_t skill_name_len;
    char *contact_id;
    size_t contact_id_len;
    uint32_t total_invocations;
    uint32_t positive_outcomes;
    uint32_t negative_outcomes;
    double decayed_score;   /* 0.0–1.0, exponential decay toward 0.5 */
} hu_skill_profile_t;

/* Create the skill_invocations and skill_profiles tables. Idempotent. */
hu_error_t hu_evolving_init_schema(sqlite3 *db);

/* Record an invocation (explicit skill_run or implicit catalog exposure). */
hu_error_t hu_evolving_record_invocation(sqlite3 *db,
                                         const hu_skill_invocation_t *inv);

/* Record implicit exposure for all top-k catalog skills shown this turn. */
hu_error_t hu_evolving_record_implicit_exposure(sqlite3 *db,
                                                const char *const *skill_names,
                                                size_t count,
                                                const char *contact_id,
                                                size_t contact_id_len,
                                                const char *session_id,
                                                size_t session_id_len);

/* Attach outcome to the most recent invocation(s) for a contact within the
 * current session. Uses heuristic: correction = -1, positive feedback = +1. */
hu_error_t hu_evolving_collect_outcome(sqlite3 *db,
                                       const char *contact_id, size_t contact_id_len,
                                       const char *session_id, size_t session_id_len,
                                       int outcome);

/* Load aggregated profiles for a contact. Caller owns returned array;
 * free each profile's strings with alloc, then free the array. */
hu_error_t hu_evolving_load_profiles(sqlite3 *db, hu_allocator_t *alloc,
                                     const char *contact_id, size_t contact_id_len,
                                     hu_skill_profile_t **out, size_t *out_count);

/* Free a profile array loaded by hu_evolving_load_profiles. */
void hu_evolving_free_profiles(hu_allocator_t *alloc,
                               hu_skill_profile_t *profiles, size_t count);

/* Compute a weight multiplier from a profile for catalog ranking.
 * Returns 1.0 when no data, >1.0 for positive history, <1.0 for negative. */
double hu_evolving_compute_weight(const hu_skill_profile_t *profile);

/* Rebuild aggregated profiles from raw invocations with exponential decay. */
hu_error_t hu_evolving_rebuild_profiles(sqlite3 *db);

#endif /* HU_ENABLE_SQLITE */

#endif /* HU_COGNITION_EVOLVING_H */
