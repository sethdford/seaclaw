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
#define HU_DECOMP_MAX_TASKS 16

typedef enum hu_decomposition_strategy {
    HU_DECOMP_SEQUENTIAL,
    HU_DECOMP_PARALLEL,
    HU_DECOMP_MAP_REDUCE,
    HU_DECOMP_PIPELINE,
} hu_decomposition_strategy_t;

typedef struct hu_decomposition_subtask {
    char description[512];
    size_t description_len;
    uint32_t depends_on; /* 0 = none, 1-based index of dependency */
} hu_decomposition_subtask_t;

typedef struct hu_decomposition_result {
    hu_decomposition_subtask_t tasks[HU_DECOMP_MAX_TASKS];
    size_t task_count;
    hu_decomposition_strategy_t strategy;
} hu_decomposition_result_t;

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

/* Dynamic task decomposition by strategy. When provider is non-NULL, uses LLM;
 * otherwise returns heuristic fallback. Under HU_IS_TEST returns mock
 * decomposition (3 subtasks: 2 parallel + 1 dependent). */
hu_error_t hu_decompose_task(hu_allocator_t *alloc, hu_provider_t *provider,
                             const char *model, size_t model_len,
                             const char *prompt, size_t prompt_len,
                             hu_decomposition_strategy_t strategy,
                             hu_decomposition_result_t *result);

/* Auto-assign decomposed tasks to the orchestrator based on capabilities. */
hu_error_t hu_orchestrator_auto_assign(hu_orchestrator_t *orch,
                                        const hu_decomposition_t *decomposition);

/* Re-decompose with failure context: takes the original prompt, the failed subtask
 * description and failure reason, and produces a new decomposition that works around
 * the failure. Under HU_IS_TEST, returns a mock re-plan. */
hu_error_t hu_decompose_with_replan(hu_allocator_t *alloc, hu_provider_t *provider,
                                     const char *model, size_t model_len,
                                     const char *original_prompt, size_t original_prompt_len,
                                     const char *failed_task, size_t failed_task_len,
                                     const char *failure_reason, size_t failure_reason_len,
                                     hu_decomposition_strategy_t strategy,
                                     hu_decomposition_result_t *result);

/* Check whether the subtasks in a decomposition plausibly cover the original goal.
 * Returns true if coverage is sufficient (word overlap heuristic). */
bool hu_decomposition_check_coverage(const char *goal, size_t goal_len,
                                      const hu_decomposition_result_t *result);

#endif
