#ifndef HU_EXPERIENCE_H
#define HU_EXPERIENCE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/memory.h"
#include <stddef.h>

typedef struct hu_experience_store {
    hu_allocator_t *alloc;
    hu_memory_t *memory;
    size_t stored_count;
} hu_experience_store_t;

hu_error_t hu_experience_store_init(hu_allocator_t *alloc, hu_memory_t *memory,
                                    hu_experience_store_t *out);
void hu_experience_store_deinit(hu_experience_store_t *store);

hu_error_t hu_experience_record(hu_experience_store_t *store,
                                const char *task, size_t task_len,
                                const char *actions, size_t actions_len,
                                const char *outcome, size_t outcome_len,
                                double score);
hu_error_t hu_experience_recall_similar(hu_experience_store_t *store,
                                        const char *task, size_t task_len,
                                        char **out_context, size_t *out_len);
hu_error_t hu_experience_build_prompt(hu_experience_store_t *store,
                                      const char *current_task, size_t task_len,
                                      char **out, size_t *out_len);

#endif
