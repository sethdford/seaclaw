#ifndef HU_AGENT_MAR_H
#define HU_AGENT_MAR_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/provider.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Multi-Agent Reflexion (MAR) — structured critique personas.
 * Extends swarm with sequential ACTOR -> DIAGNOSER -> CRITIC -> JUDGE phases.
 * Source: arXiv:2512.20845
 */

typedef enum hu_mar_role {
    HU_MAR_ACTOR = 0, /* Generates initial response */
    HU_MAR_DIAGNOSER, /* Diagnoses failures/weaknesses */
    HU_MAR_CRITIC,    /* Critiques the diagnosis */
    HU_MAR_JUDGE,     /* Aggregates and produces final verdict */
    HU_MAR_ROLE_COUNT,
} hu_mar_role_t;

typedef struct hu_mar_phase_result {
    hu_mar_role_t role;
    char *output; /* heap-allocated */
    size_t output_len;
    int64_t elapsed_ms;
    bool success;
} hu_mar_phase_result_t;

typedef struct hu_mar_config {
    bool enabled;
    uint32_t max_rounds; /* how many ACTOR->JUDGE cycles; 0 = default (2) */
    const char *actor_model;
    size_t actor_model_len;
    const char *judge_model; /* NULL = use actor_model */
    size_t judge_model_len;
    const char *critic_model; /* NULL = use actor_model; smaller model saves cost */
    size_t critic_model_len;
} hu_mar_config_t;

#define HU_MAR_CONFIG_DEFAULT \
    {.enabled = false,        \
     .max_rounds = 2,         \
     .actor_model = NULL,     \
     .actor_model_len = 0,    \
     .judge_model = NULL,     \
     .judge_model_len = 0,    \
     .critic_model = NULL,    \
     .critic_model_len = 0}

typedef struct hu_mar_result {
    hu_mar_phase_result_t phases[HU_MAR_ROLE_COUNT * 4]; /* up to 4 rounds */
    size_t phase_count;
    char *final_output; /* heap-allocated */
    size_t final_output_len;
    uint32_t rounds_completed;
    bool consensus_reached;
} hu_mar_result_t;

hu_error_t hu_mar_execute(hu_allocator_t *alloc, hu_provider_t *provider,
                          const hu_mar_config_t *config, const char *task, size_t task_len,
                          hu_mar_result_t *out);

void hu_mar_phase_result_free(hu_allocator_t *alloc, hu_mar_phase_result_t *phase);
void hu_mar_result_free(hu_allocator_t *alloc, hu_mar_result_t *result);

const char *hu_mar_role_name(hu_mar_role_t role);

#endif /* HU_AGENT_MAR_H */
