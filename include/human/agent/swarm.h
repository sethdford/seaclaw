#ifndef HU_AGENT_SWARM_H
#define HU_AGENT_SWARM_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Swarm — parallel agent execution for decomposed tasks.
 * Under HU_IS_TEST: executes sequentially with mock results.
 * Production: sequential for now (threading requires careful design).
 */

typedef struct hu_swarm_config {
    int max_parallel;        /* default 4 */
    int64_t timeout_ms;     /* per-agent timeout, default 30000 */
    int retry_on_failure;    /* default 1 */
} hu_swarm_config_t;

typedef struct hu_swarm_task {
    char description[512];
    size_t description_len;
    bool completed;
    bool failed;
    char result[2048];
    size_t result_len;
    int64_t elapsed_ms;
} hu_swarm_task_t;

typedef struct hu_swarm_result {
    hu_swarm_task_t *tasks;
    size_t task_count;
    size_t completed;
    size_t failed;
    int64_t total_elapsed_ms;
} hu_swarm_result_t;

hu_swarm_config_t hu_swarm_config_default(void);

hu_error_t hu_swarm_execute(hu_allocator_t *alloc, const hu_swarm_config_t *config,
                           hu_swarm_task_t *tasks, size_t task_count,
                           hu_swarm_result_t *result);

void hu_swarm_result_free(hu_allocator_t *alloc, hu_swarm_result_t *result);

#endif /* HU_AGENT_SWARM_H */
