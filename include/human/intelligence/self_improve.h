#ifndef HU_INTELLIGENCE_SELF_IMPROVE_H
#define HU_INTELLIGENCE_SELF_IMPROVE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Self-Improvement Engine — closes the reflection → behavior feedback loop.
 *
 * Reads recent reflections and feedback, generates prompt patches and tool
 * preference adjustments, and applies them to future agent turns.
 */

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>

typedef struct hu_prompt_patch {
    int64_t id;
    char source[256];
    char patch_text[2048];
    size_t patch_text_len;
    bool active;
    int64_t applied_at;
} hu_prompt_patch_t;

typedef struct hu_tool_pref {
    char tool_name[128];
    double weight;
    int32_t successes;
    int32_t failures;
    int64_t updated_at;
} hu_tool_pref_t;

typedef struct hu_self_improve {
    hu_allocator_t *alloc;
    sqlite3 *db;
} hu_self_improve_t;

hu_error_t hu_self_improve_create(hu_allocator_t *alloc, sqlite3 *db,
                                  hu_self_improve_t *out);
void hu_self_improve_deinit(hu_self_improve_t *engine);

hu_error_t hu_self_improve_init_tables(hu_self_improve_t *engine);

/* Read recent reflections, derive prompt patches, and store them. */
hu_error_t hu_self_improve_apply_reflections(hu_self_improve_t *engine, int64_t now_ts);

/* Record a tool outcome and update preference weights. */
hu_error_t hu_self_improve_record_tool_outcome(hu_self_improve_t *engine,
                                               const char *tool_name, size_t name_len,
                                               bool succeeded, int64_t now_ts);

/* Get current tool preference weight (1.0 = neutral). */
double hu_self_improve_get_tool_weight(hu_self_improve_t *engine,
                                       const char *tool_name, size_t name_len);

/* Build prompt text from active patches. Caller must free *out. */
hu_error_t hu_self_improve_get_prompt_patches(hu_self_improve_t *engine,
                                              char **out, size_t *out_len);

/* Build tool reliability prompt from tool_prefs. Caller must free *out. */
hu_error_t hu_self_improve_get_tool_prefs_prompt(hu_self_improve_t *engine,
                                                 char **out, size_t *out_len);

/* Get count of active patches. */
hu_error_t hu_self_improve_active_patch_count(hu_self_improve_t *engine, size_t *out);

/* --- Assessment-driven closed-loop self-improvement --- */

struct hu_eval_run;
struct hu_eval_suite;

/* Analyze assessment run weaknesses, generate patches, store in eval_patches. */
hu_error_t hu_self_improve_from_assessment(hu_self_improve_t *engine,
                                           const struct hu_eval_run *run,
                                           const struct hu_eval_suite *suite,
                                           int64_t now_ts);

/* Compare before/after pass rate; mark kept=1 if improved, else deactivate. */
hu_error_t hu_self_improve_verify_patch(hu_self_improve_t *engine,
                                        int64_t patch_id, double new_pass_rate);

/* Deactivate an assessment-derived patch in prompt_patches. */
hu_error_t hu_self_improve_rollback_patch(hu_self_improve_t *engine, int64_t patch_id);

/* Get count of assessment-derived patches (kept=1). */
hu_error_t hu_self_improve_kept_patch_count(hu_self_improve_t *engine, size_t *out);

#endif /* HU_ENABLE_SQLITE */
#endif /* HU_INTELLIGENCE_SELF_IMPROVE_H */
