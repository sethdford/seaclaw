#ifndef HU_AGENT_PLAN_EXECUTOR_H
#define HU_AGENT_PLAN_EXECUTOR_H

#include "human/agent/planner.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/provider.h"
#include "human/tool.h"
#include <stdbool.h>
#include <stddef.h>

#define HU_PLAN_EXEC_MAX_REPLANS 3

typedef struct hu_plan_exec_result {
    size_t steps_completed;
    size_t steps_failed;
    size_t replans;
    bool goal_achieved;
    char summary[2048];
    size_t summary_len;
} hu_plan_exec_result_t;

typedef struct hu_plan_executor {
    hu_allocator_t *alloc;
    hu_provider_t *provider;
    const char *model;
    size_t model_len;
    hu_tool_t *tools;
    size_t tools_count;
    uint32_t max_replans;
} hu_plan_executor_t;

void hu_plan_executor_init(hu_plan_executor_t *exec, hu_allocator_t *alloc,
                           hu_provider_t *provider, const char *model, size_t model_len,
                           hu_tool_t *tools, size_t tools_count);

hu_error_t hu_plan_executor_run(hu_plan_executor_t *exec, hu_plan_t *plan,
                                const char *goal, size_t goal_len,
                                hu_plan_exec_result_t *result);

hu_error_t hu_plan_executor_run_goal(hu_plan_executor_t *exec,
                                     const char *goal, size_t goal_len,
                                     hu_plan_exec_result_t *result);

#endif /* HU_AGENT_PLAN_EXECUTOR_H */
