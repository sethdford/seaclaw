#ifndef HU_COGNITION_DUAL_PROCESS_H
#define HU_COGNITION_DUAL_PROCESS_H

#include "human/cognition/emotional.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum hu_cognition_mode {
    HU_COGNITION_FAST,       /* System 1: shallow retrieval, small prompt, few tool iters */
    HU_COGNITION_SLOW,       /* System 2: full pipeline */
    HU_COGNITION_EMOTIONAL,  /* Slow variant: empathy-first, relationship context prioritized */
} hu_cognition_mode_t;

typedef struct hu_cognition_budget {
    hu_cognition_mode_t mode;
    size_t max_memory_entries;
    size_t max_memory_chars;
    uint32_t max_tool_iterations;
    bool enable_planning;
    bool enable_tree_of_thought;
    bool enable_mid_turn_retrieval;
    bool enable_reflection;
    bool prioritize_empathy;
} hu_cognition_budget_t;

/* Input signals for mode classification */
typedef struct hu_cognition_dispatch_input {
    const char *message;
    size_t message_len;
    const hu_emotional_cognition_t *emotional;  /* from emotional cognition; NULL ok */
    size_t tools_count;                         /* number of tools available */
    size_t recent_tool_calls;                   /* tool calls in last 3 turns */
    uint32_t agent_max_tool_iterations;         /* agent's configured max */
} hu_cognition_dispatch_input_t;

/* Classify a user turn into a cognition mode. */
hu_cognition_mode_t hu_cognition_dispatch(const hu_cognition_dispatch_input_t *input);

/* Get the budget for a given mode. agent_max_iters is the agent's base max. */
hu_cognition_budget_t hu_cognition_get_budget(hu_cognition_mode_t mode,
                                               uint32_t agent_max_iters);

/* Get mode name string for observer events. */
const char *hu_cognition_mode_name(hu_cognition_mode_t mode);

#endif /* HU_COGNITION_DUAL_PROCESS_H */
