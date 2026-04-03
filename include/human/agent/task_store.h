#ifndef HU_AGENT_TASK_STORE_H
#define HU_AGENT_TASK_STORE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>
#include <stdint.h>

typedef enum hu_task_status {
    HU_TASK_STATUS_PENDING = 0,
    HU_TASK_STATUS_RUNNING = 1,
    HU_TASK_STATUS_COMPLETED = 2,
    HU_TASK_STATUS_FAILED = 3,
    HU_TASK_STATUS_CANCELLED = 4,
} hu_task_status_t;

typedef struct hu_task_record {
    uint64_t id;
    char *name;
    hu_task_status_t status;
    char *program_json;
    char *trace_json;
    int64_t created_at;
    int64_t updated_at;
    uint64_t parent_task_id;
} hu_task_record_t;

typedef struct hu_task_store hu_task_store_t;

hu_error_t hu_task_store_create(hu_allocator_t *alloc, void *db, hu_task_store_t **out);
void hu_task_store_destroy(hu_task_store_t *store, hu_allocator_t *alloc);

hu_error_t hu_task_store_save(hu_task_store_t *store, hu_allocator_t *alloc,
                              const hu_task_record_t *task, uint64_t *out_id);
hu_error_t hu_task_store_load(hu_task_store_t *store, hu_allocator_t *alloc, uint64_t id,
                              hu_task_record_t *out);
hu_error_t hu_task_store_update_status(hu_task_store_t *store, uint64_t id, hu_task_status_t status);
hu_error_t hu_task_store_list(hu_task_store_t *store, hu_allocator_t *alloc,
                              hu_task_status_t *filter_status, hu_task_record_t **out,
                              size_t *out_count);

void hu_task_record_free(hu_allocator_t *alloc, hu_task_record_t *r);
void hu_task_records_free(hu_allocator_t *alloc, hu_task_record_t *records, size_t count);

const char *hu_task_status_string(hu_task_status_t s);

#endif
