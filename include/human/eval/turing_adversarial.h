#ifndef HU_EVAL_TURING_ADVERSARIAL_H
#define HU_EVAL_TURING_ADVERSARIAL_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/eval/turing_score.h"
#include "human/agent/self_improve.h"
#include <stddef.h>

/*
 * Adversarial Turing testing: generate dimension-targeted scenarios,
 * score responses, map weaknesses to persona mutations, and feed
 * results back into the self-improvement loop.
 *
 * Flow: weak dimensions -> targeted probes -> agent response ->
 *       Turing score -> weakness extraction -> mutation proposal
 */

#define HU_TURING_ADV_PROMPT_MAX    512
#define HU_TURING_ADV_INTENT_MAX    256
#define HU_TURING_ADV_MUTATION_MAX  512
#define HU_TURING_ADV_WEAK_THRESHOLD 7

typedef struct hu_turing_scenario {
    hu_turing_dimension_t target_dim;
    char prompt[HU_TURING_ADV_PROMPT_MAX];
    size_t prompt_len;
    char adversarial_intent[HU_TURING_ADV_INTENT_MAX];
    size_t intent_len;
} hu_turing_scenario_t;

/* Total scenarios in the built-in bank. */
size_t hu_turing_adversarial_bank_size(void);

/* Get a scenario from the built-in bank by index.
 * Returns HU_ERR_OUT_OF_RANGE if idx >= bank_size. */
hu_error_t hu_turing_adversarial_bank_get(size_t idx, hu_turing_scenario_t *out);

/* Generate scenarios targeting dimensions with averages below threshold.
 * weak_dimensions: array of HU_TURING_DIM_COUNT dimension averages (1-10).
 * Emits scenarios for each dimension scoring below HU_TURING_ADV_WEAK_THRESHOLD.
 * Caller owns *scenarios (allocated via alloc). */
hu_error_t hu_turing_adversarial_generate(
    hu_allocator_t *alloc,
    const int *weak_dimensions,
    hu_turing_scenario_t **scenarios,
    size_t *scenario_count);

/* Map a Turing score's weak dimensions to a targeted self-improve mutation string.
 * Caller owns *mutation (allocated via alloc). */
hu_error_t hu_turing_adversarial_to_mutation(
    hu_allocator_t *alloc,
    const hu_turing_score_t *score,
    char **mutation,
    size_t *mutation_len);

/* Run one adversarial improvement cycle:
 * 1. Generate scenarios from weak_dimensions
 * 2. For each scenario, score the template "ideal bad response" with heuristics
 * 3. Extract mutations from weakest scores
 * 4. Record mutations in self-improve state
 * Returns the number of mutations applied via *mutations_applied. */
hu_error_t hu_turing_adversarial_run_cycle(
    hu_allocator_t *alloc,
    hu_self_improve_state_t *state,
    const int *weak_dimensions,
    size_t *mutations_applied);

#endif
