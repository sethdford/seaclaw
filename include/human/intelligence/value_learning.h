#ifndef HU_INTELLIGENCE_VALUE_LEARNING_H
#define HU_INTELLIGENCE_VALUE_LEARNING_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Value Learning — infers user values from corrections, approvals,
 * and rejections. Uses inferred values to guide autonomous decisions
 * and build value-aware prompts.
 */

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>

typedef struct hu_value {
    int64_t id;
    char name[256];
    size_t name_len;
    char description[1024];
    size_t description_len;
    double importance;      /* 0.0–1.0, learned weight */
    int32_t evidence_count;
    int64_t created_at;
    int64_t updated_at;
} hu_value_t;

typedef struct hu_value_engine {
    hu_allocator_t *alloc;
    sqlite3 *db;
} hu_value_engine_t;

hu_error_t hu_value_engine_create(hu_allocator_t *alloc, sqlite3 *db,
                                  hu_value_engine_t *out);
void hu_value_engine_deinit(hu_value_engine_t *engine);

hu_error_t hu_value_init_tables(hu_value_engine_t *engine);

/* Learn a value from a user correction (what was wrong implies what they value). */
hu_error_t hu_value_learn_from_correction(hu_value_engine_t *engine,
                                          const char *value_name, size_t name_len,
                                          const char *description, size_t desc_len,
                                          double strength, int64_t now_ts);

/* Reinforce a value from user approval. */
hu_error_t hu_value_learn_from_approval(hu_value_engine_t *engine,
                                        const char *value_name, size_t name_len,
                                        double strength, int64_t now_ts);

/* Weaken a value (user rejected behavior aligned with it). */
hu_error_t hu_value_weaken(hu_value_engine_t *engine,
                           const char *value_name, size_t name_len,
                           double amount, int64_t now_ts);

/* Get a value by name. */
hu_error_t hu_value_get(hu_value_engine_t *engine,
                        const char *name, size_t name_len,
                        hu_value_t *out, bool *found);

/* List all values sorted by importance. Caller must free *out. */
hu_error_t hu_value_list(hu_value_engine_t *engine,
                         hu_value_t **out, size_t *out_count);

/* Count total values. */
hu_error_t hu_value_count(hu_value_engine_t *engine, size_t *out);

/* Build value-aware prompt text. Caller must free *out. */
hu_error_t hu_value_build_prompt(hu_value_engine_t *engine,
                                 char **out, size_t *out_len);

/* Check if an action aligns with top values. Returns alignment score 0.0–1.0. */
double hu_value_alignment_score(const hu_value_t *values, size_t count,
                                const char *action, size_t action_len);

void hu_value_free(hu_allocator_t *alloc, hu_value_t *values, size_t count);

#endif /* HU_ENABLE_SQLITE */
#endif /* HU_INTELLIGENCE_VALUE_LEARNING_H */
