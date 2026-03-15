#ifndef HU_EXPERIENCE_H
#define HU_EXPERIENCE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/memory.h"
#include "human/memory/vector.h"
#include <stddef.h>
#include <stdint.h>

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>

typedef struct hu_experience_entry {
    int64_t id;
    char task[512];
    size_t task_len;
    char outcome[512];
    size_t outcome_len;
    double score;
    char lessons[1024];
    size_t lessons_len;
    int64_t timestamp;
} hu_experience_entry_t;

hu_error_t hu_experience_init_tables(hu_allocator_t *alloc, sqlite3 *db);
hu_error_t hu_experience_record_db(hu_allocator_t *alloc, sqlite3 *db,
                                   const char *task, size_t task_len,
                                   const char *outcome, size_t outcome_len,
                                   double score, const char *lessons, size_t lessons_len);
hu_error_t hu_experience_recall_db(hu_allocator_t *alloc, sqlite3 *db,
                                   const char *query, size_t query_len,
                                   hu_experience_entry_t *results, size_t max_results,
                                   size_t *out_count);
#endif /* HU_ENABLE_SQLITE */

typedef struct hu_experience_store {
    hu_allocator_t *alloc;
    hu_memory_t *memory;
    hu_embedder_t *embedder;
    hu_vector_store_t *vec_store;
#ifdef HU_ENABLE_SQLITE
    sqlite3 *db;
#endif
    size_t stored_count;
} hu_experience_store_t;

hu_error_t hu_experience_store_init(hu_allocator_t *alloc, hu_memory_t *memory,
                                    hu_experience_store_t *out);

hu_error_t hu_experience_store_init_semantic(hu_allocator_t *alloc, hu_memory_t *memory,
                                             hu_embedder_t *embedder,
                                             hu_vector_store_t *vec_store,
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
