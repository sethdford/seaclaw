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
#include "human/provider.h"
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

/* Closed loop: eval → weakness → patch → re-run → keep or rollback (db is sqlite3 *). */
hu_error_t hu_self_improve_closed_loop(hu_allocator_t *alloc, void *db,
                                       hu_provider_t *provider, const char *model, size_t model_len,
                                       const char *eval_suite_path);

/* Structured patch types for real config/persona mutations. */
typedef enum hu_patch_type {
    HU_PATCH_TEMPERATURE = 0,
    HU_PATCH_MAX_TOKENS,
    HU_PATCH_PERSONA_TRAIT_ADD,
    HU_PATCH_PERSONA_TRAIT_REMOVE,
    HU_PATCH_STYLE_RULE,
    HU_PATCH_TOOL_PREF,
    HU_PATCH_TEXT_HINT,
} hu_patch_type_t;

typedef struct hu_structured_patch {
    hu_patch_type_t type;
    char key[128];
    char value[512];
    double numeric_value;
    bool parsed;
} hu_structured_patch_t;

/* Parse a patch_text string into a structured patch. Returns true if parseable. */
bool hu_self_improve_parse_patch(const char *patch_text, size_t patch_text_len,
                                 hu_structured_patch_t *out);

/* Apply a structured patch to the self-improve engine's patch store.
 * Records in prompt_patches with a structured prefix for later retrieval. */
hu_error_t hu_self_improve_apply_structured_patch(hu_self_improve_t *engine,
                                                  const hu_structured_patch_t *patch);

/* Get all active structured patches of a given type. Caller frees *out with alloc->free(ctx, ptr, count * sizeof(hu_structured_patch_t)). */
hu_error_t hu_self_improve_get_structured_patches(hu_self_improve_t *engine,
                                                  hu_allocator_t *alloc, hu_patch_type_t type,
                                                  hu_structured_patch_t **out, size_t *out_count);

/* --- Eval delta gate (fast subset before/after structured patch) --- */

typedef struct hu_self_improve_delta {
    double score_before;
    double score_after;
    double delta;         /* score_after - score_before */
    bool should_rollback; /* delta < -threshold (see implementation) */
    char patch_id[64];    /* prompt_patches row id as decimal string after apply (non-test) */
} hu_self_improve_delta_t;

/* Run a fast eval subset, apply patch, re-eval, compare. Persists row to self_improve_deltas. */
hu_error_t hu_self_improve_eval_and_apply(hu_allocator_t *alloc, sqlite3 *db,
                                          const hu_structured_patch_t *patch,
                                          hu_self_improve_delta_t *out_delta);

/* Deactivate the structured patch row if delta.should_rollback; marks self_improve_deltas.rolled_back. */
hu_error_t hu_self_improve_rollback_if_negative(hu_allocator_t *alloc, sqlite3 *db,
                                                const hu_self_improve_delta_t *delta);

#endif /* HU_ENABLE_SQLITE */
#endif /* HU_INTELLIGENCE_SELF_IMPROVE_H */
