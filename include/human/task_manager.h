#ifndef HU_TASK_MANAGER_H
#define HU_TASK_MANAGER_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ──────────────────────────────────────────────────────────────────────────
 * Task Manager — in-memory task tracking for agents
 * ────────────────────────────────────────────────────────────────────────── */

typedef enum hu_task_status {
    HU_TASK_PENDING,
    HU_TASK_IN_PROGRESS,
    HU_TASK_COMPLETED,
} hu_task_status_t;

typedef struct hu_task {
    uint32_t id;
    char *subject;
    size_t subject_len;
    char *description;
    size_t description_len;
    hu_task_status_t status;
} hu_task_t;

typedef struct hu_task_manager hu_task_manager_t;

/* Create task manager. Max tasks defaults to 1000. */
hu_error_t hu_task_manager_create(hu_allocator_t *alloc, size_t max_tasks,
                                  hu_task_manager_t **out);

/* Add a new task. Subject and description are copied. Returns task ID in out_id. */
hu_error_t hu_task_manager_add(hu_task_manager_t *mgr, hu_allocator_t *alloc,
                               const char *subject, size_t subject_len, const char *description,
                               size_t description_len, uint32_t *out_id);

/* Update task status. */
hu_error_t hu_task_manager_update_status(hu_task_manager_t *mgr, uint32_t id,
                                         hu_task_status_t status);

/* Get a task by ID. Output task is borrowed (do not free). */
hu_error_t hu_task_manager_get(const hu_task_manager_t *mgr, uint32_t id,
                               const hu_task_t **out);

/* List all tasks as JSON array. Caller must free output with alloc->free(). */
hu_error_t hu_task_manager_list(const hu_task_manager_t *mgr, hu_allocator_t *alloc,
                                char **out_json, size_t *out_len);

/* Destroy task manager. Frees all task strings and the manager itself. */
void hu_task_manager_destroy(hu_task_manager_t *mgr, hu_allocator_t *alloc);

#endif /* HU_TASK_MANAGER_H */
