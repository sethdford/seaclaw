#ifndef HU_MEMORY_POLICY_H
#define HU_MEMORY_POLICY_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Policy-learned memory management (AgeMem-style).
 * Memory actions are exposed as agent-callable tools; the agent decides
 * what to store/retrieve/update/discard. Collects (state, action, outcome)
 * tuples for offline RL optimization.
 * Source: arXiv:2601.01885
 */

typedef enum hu_mem_action_type {
    HU_MEM_STORE = 0,
    HU_MEM_RETRIEVE,
    HU_MEM_UPDATE,
    HU_MEM_SUMMARIZE,
    HU_MEM_DISCARD,
    HU_MEM_ACTION_COUNT,
} hu_mem_action_type_t;

typedef struct hu_mem_action {
    hu_mem_action_type_t type;
    const char *key;
    size_t key_len;
    const char *value;
    size_t value_len;
    double relevance_score; /* 0.0-1.0; set by policy */
} hu_mem_action_t;

typedef struct hu_mem_state {
    size_t total_memories;
    size_t context_tokens;
    size_t stm_entries;
    size_t ltm_entries;
    double avg_relevance;
    double context_pressure; /* 0.0-1.0 */
} hu_mem_state_t;

typedef struct hu_mem_experience {
    hu_mem_state_t state;
    hu_mem_action_t action;
    double reward; /* -1.0 to 1.0 */
    hu_mem_state_t next_state;
} hu_mem_experience_t;

#define HU_MEM_EXPERIENCE_BUFFER_SIZE 1024

typedef struct hu_mem_policy {
    bool enabled;

    hu_mem_experience_t buffer[HU_MEM_EXPERIENCE_BUFFER_SIZE];
    size_t buffer_count;

    /* Heuristic weights (pre-RL baseline) */
    double recency_weight;
    double relevance_weight;
    double frequency_weight;

    /* Stats */
    size_t total_actions;
    size_t store_count;
    size_t retrieve_count;
    size_t discard_count;
} hu_mem_policy_t;

void hu_mem_policy_init(hu_mem_policy_t *p);

hu_mem_action_type_t hu_mem_policy_decide(const hu_mem_policy_t *p, const hu_mem_state_t *state,
                                          const char *content, size_t content_len);

double hu_mem_policy_score(const hu_mem_policy_t *p, double recency, double relevance,
                           double frequency);

hu_error_t hu_mem_policy_record(hu_mem_policy_t *p, const hu_mem_experience_t *exp);

size_t hu_mem_policy_report(const hu_mem_policy_t *p, char *buf, size_t buf_size);

const char *hu_mem_action_type_name(hu_mem_action_type_t type);

#endif /* HU_MEMORY_POLICY_H */
