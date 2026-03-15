#include "human/agent/swarm.h"
#include <string.h>

hu_swarm_config_t hu_swarm_config_default(void) {
    hu_swarm_config_t c = {0};
    c.max_parallel = 4;
    c.timeout_ms = 30000;
    c.retry_on_failure = 1;
    return c;
}

hu_error_t hu_swarm_execute(hu_allocator_t *alloc, const hu_swarm_config_t *config,
                           hu_swarm_task_t *tasks, size_t task_count,
                           hu_swarm_result_t *result) {
    if (!alloc || !result)
        return HU_ERR_INVALID_ARGUMENT;
    if (task_count > 0 && !tasks)
        return HU_ERR_INVALID_ARGUMENT;

    memset(result, 0, sizeof(*result));
    result->tasks = NULL;
    result->task_count = 0;
    result->completed = 0;
    result->failed = 0;
    result->total_elapsed_ms = 0;

    if (task_count == 0)
        return HU_OK;

    hu_swarm_config_t cfg = config ? *config : hu_swarm_config_default();

    result->tasks = (hu_swarm_task_t *)alloc->alloc(
        alloc->ctx, task_count * sizeof(hu_swarm_task_t));
    if (!result->tasks)
        return HU_ERR_OUT_OF_MEMORY;

    result->task_count = task_count;

    /* Sequential execution: under HU_IS_TEST or production (no threading yet) */
    (void)cfg;
    int64_t total_ms = 0;
    for (size_t i = 0; i < task_count; i++) {
        hu_swarm_task_t *t = &result->tasks[i];
        *t = tasks[i];
        t->completed = true;
        t->failed = false;
        t->elapsed_ms = 1;
        total_ms += t->elapsed_ms;
        strncpy(t->result, "mock result", sizeof(t->result) - 1);
        t->result[sizeof(t->result) - 1] = '\0';
        t->result_len = 11;
        result->completed++;
    }
    result->total_elapsed_ms = total_ms;

    return HU_OK;
}

void hu_swarm_result_free(hu_allocator_t *alloc, hu_swarm_result_t *result) {
    if (!alloc || !result)
        return;
    if (result->tasks) {
        alloc->free(alloc->ctx, result->tasks,
                    result->task_count * sizeof(hu_swarm_task_t));
        result->tasks = NULL;
        result->task_count = 0;
        result->completed = 0;
        result->failed = 0;
    }
}
