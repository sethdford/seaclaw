#ifndef HU_AGENT_ORCHESTRATOR_LLM_H
#define HU_AGENT_ORCHESTRATOR_LLM_H

#include "human/agent/orchestrator.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/provider.h"
#include <stddef.h>

/*
 * LLM-Driven Orchestrator — uses an LLM to decompose complex goals
 * into subtasks with dependency ordering, then assigns to specialized agents.
 * Extends the basic orchestrator with intelligent task splitting.
 */

#define HU_ORCH_LLM_MAX_SUBTASKS 8

typedef struct hu_decomposition {
    hu_orchestrator_task_t tasks[HU_ORCH_LLM_MAX_SUBTASKS];
    char capabilities[HU_ORCH_LLM_MAX_SUBTASKS][64]; /* capability per task for auto_assign */
    size_t task_count;
    char *reasoning;
    size_t reasoning_len;
} hu_decomposition_t;

/* Use LLM to decompose a goal into subtasks.
 * Each subtask includes a description, required capability, and dependencies.
 * Caller must free with hu_decomposition_free. */
hu_error_t hu_orchestrator_decompose_goal(hu_allocator_t *alloc, hu_provider_t *provider,
                                           const char *model, size_t model_len,
                                           const char *goal, size_t goal_len,
                                           const hu_agent_capability_t *capabilities,
                                           size_t capability_count,
                                           hu_decomposition_t *result);

void hu_decomposition_free(hu_allocator_t *alloc, hu_decomposition_t *result);

/* Auto-assign decomposed tasks to the orchestrator based on capabilities. */
hu_error_t hu_orchestrator_auto_assign(hu_orchestrator_t *orch,
                                        const hu_decomposition_t *decomposition);

#endif
