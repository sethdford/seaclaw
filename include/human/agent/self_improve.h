#ifndef HU_AGENT_SELF_IMPROVE_H
#define HU_AGENT_SELF_IMPROVE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Self-improvement loop — Ouroboros/autoresearch-inspired bounded experimentation.
 *
 * Fixed eval harness + mutable behavior file + composite Human Fidelity Score.
 * The system experiments with behavior modifications (persona rules, prompt
 * strategies, response patterns) under fixed evaluation, keeping improvements
 * and discarding regressions. L5 autonomy: methodology improves, metrics stay locked.
 *
 * Loop: propose modification → run eval → score → keep/discard → repeat.
 */

#define HU_SELF_IMPROVE_MAX_EXPERIMENTS 100
#define HU_SELF_IMPROVE_MUTATION_MAX_LEN 1024
#define HU_SELF_IMPROVE_MAX_HISTORY 64

typedef struct hu_fidelity_score {
    float personality_consistency;
    float vulnerability_willingness;
    float humor_naturalness;
    float opinion_having;
    float energy_matching;
    float genuine_warmth;
    float composite;             /* weighted average */
} hu_fidelity_score_t;

typedef struct hu_experiment {
    uint32_t id;
    char mutation[HU_SELF_IMPROVE_MUTATION_MAX_LEN];
    size_t mutation_len;
    hu_fidelity_score_t before;
    hu_fidelity_score_t after;
    float delta;                 /* after.composite - before.composite */
    bool kept;
} hu_experiment_t;

typedef struct hu_self_improve_config {
    uint32_t max_experiments;    /* budget cap */
    uint32_t max_seconds;        /* wall-clock budget per experiment */
    float min_improvement;       /* delta threshold to keep (default 0.01) */
    bool dry_run;                /* propose but don't apply mutations */
} hu_self_improve_config_t;

#define HU_SELF_IMPROVE_DEFAULTS \
    ((hu_self_improve_config_t){ \
        .max_experiments = 10, \
        .max_seconds = 300, \
        .min_improvement = 0.01f, \
        .dry_run = false, \
    })

typedef struct hu_self_improve_state {
    hu_self_improve_config_t config;
    hu_experiment_t history[HU_SELF_IMPROVE_MAX_HISTORY];
    size_t history_count;
    hu_fidelity_score_t current_baseline;
    uint32_t experiments_run;
    uint32_t experiments_kept;
    bool running;
} hu_self_improve_state_t;

/* Initialize self-improvement state. */
void hu_self_improve_init(hu_self_improve_state_t *state,
                          const hu_self_improve_config_t *config);

/* Compute composite fidelity score from individual dimensions. */
float hu_fidelity_composite(const hu_fidelity_score_t *score);

/* Set baseline fidelity score (from current eval run). */
void hu_self_improve_set_baseline(hu_self_improve_state_t *state,
                                  const hu_fidelity_score_t *baseline);

/* Propose a behavior mutation (text modification to persona/behavior rules). */
hu_error_t hu_self_improve_propose(hu_allocator_t *alloc,
                                   const hu_self_improve_state_t *state,
                                   char **mutation, size_t *mutation_len);

/* Record experiment outcome and decide keep/discard. */
bool hu_self_improve_record(hu_self_improve_state_t *state,
                            const char *mutation, size_t mutation_len,
                            const hu_fidelity_score_t *after);

/* Check if the improvement budget is exhausted. */
bool hu_self_improve_budget_exhausted(const hu_self_improve_state_t *state);

#endif
