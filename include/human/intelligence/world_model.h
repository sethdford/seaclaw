#ifndef HU_INTELLIGENCE_WORLD_MODEL_H
#define HU_INTELLIGENCE_WORLD_MODEL_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>
#include <stdint.h>

/*
 * World Model — forward simulation and counterfactual reasoning
 * on top of the causal knowledge graph.
 *
 * Given an action and context, predicts likely outcomes by traversing
 * causal links, weighting by confidence, and ranking alternatives.
 */

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>

typedef struct hu_wm_prediction {
    char outcome[1024];
    size_t outcome_len;
    double confidence;   /* 0.0–1.0 */
    char reasoning[2048];
    size_t reasoning_len;
} hu_wm_prediction_t;

typedef struct hu_action_option {
    char action[512];
    size_t action_len;
    hu_wm_prediction_t prediction;
    double score; /* composite ranking score */
} hu_action_option_t;

typedef struct hu_world_model {
    hu_allocator_t *alloc;
    sqlite3 *db;
} hu_world_model_t;

hu_error_t hu_world_model_create(hu_allocator_t *alloc, sqlite3 *db,
                                 hu_world_model_t *out);
void hu_world_model_deinit(hu_world_model_t *model);

hu_error_t hu_world_model_init_tables(hu_world_model_t *model);

/* Predict outcome of an action given context. */
hu_error_t hu_world_simulate(hu_world_model_t *model,
                             const char *action, size_t action_len,
                             const char *context, size_t context_len,
                             hu_wm_prediction_t *out);

/* "What if" — predict outcome of an alternative action. */
hu_error_t hu_world_counterfactual(hu_world_model_t *model,
                                   const char *original_action, size_t orig_len,
                                   const char *alternative, size_t alt_len,
                                   const char *context, size_t ctx_len,
                                   hu_wm_prediction_t *out);

/* Rank multiple action options by predicted outcome quality. */
hu_error_t hu_world_evaluate_options(hu_world_model_t *model,
                                     const char **actions, const size_t *action_lens,
                                     size_t count,
                                     const char *context, size_t ctx_len,
                                     hu_action_option_t *out);

/* Record an observed outcome to strengthen/weaken causal links. */
hu_error_t hu_world_record_outcome(hu_world_model_t *model,
                                   const char *action, size_t action_len,
                                   const char *outcome, size_t outcome_len,
                                   double confidence, int64_t now_ts);

/* Get causal chain depth from an action (how many hops of consequences). */
hu_error_t hu_world_causal_depth(hu_world_model_t *model,
                                 const char *action, size_t action_len,
                                 size_t *out_depth);

#endif /* HU_ENABLE_SQLITE */
#endif /* HU_INTELLIGENCE_WORLD_MODEL_H */
