#ifndef HU_ML_DPO_H
#define HU_ML_DPO_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>
#endif

typedef struct hu_preference_pair {
    char prompt[2048];
    size_t prompt_len;
    char chosen[4096];
    size_t chosen_len;
    char rejected[4096];
    size_t rejected_len;
    double margin;
    int64_t timestamp;
    char source[64];
    size_t source_len;
} hu_preference_pair_t;

typedef struct hu_dpo_collector {
    hu_allocator_t *alloc;
#ifdef HU_ENABLE_SQLITE
    sqlite3 *db;
#else
    void *db;
#endif
    size_t pair_count;
    size_t max_pairs;
} hu_dpo_collector_t;

typedef struct hu_dpo_export {
    hu_preference_pair_t *pairs;
    size_t count;
} hu_dpo_export_t;

hu_error_t hu_dpo_collector_create(hu_allocator_t *alloc,
#ifdef HU_ENABLE_SQLITE
                                   sqlite3 *db,
#else
                                   void *db,
#endif
                                   size_t max_pairs, hu_dpo_collector_t *out);
void hu_dpo_collector_deinit(hu_dpo_collector_t *collector);
hu_error_t hu_dpo_init_tables(hu_dpo_collector_t *collector);

hu_error_t hu_dpo_record_pair(hu_dpo_collector_t *collector,
                              const hu_preference_pair_t *pair);

hu_error_t hu_dpo_record_from_feedback(hu_dpo_collector_t *collector,
                                       const char *prompt, size_t prompt_len,
                                       const char *response, size_t response_len,
                                       bool positive);

hu_error_t hu_dpo_record_from_retry(hu_dpo_collector_t *collector,
                                    const char *prompt, size_t prompt_len,
                                    const char *rejected, size_t rejected_len,
                                    const char *chosen, size_t chosen_len);

hu_error_t hu_dpo_export_jsonl(hu_dpo_collector_t *collector,
                               const char *path, size_t path_len,
                               size_t *exported_count);

hu_error_t hu_dpo_pair_count(hu_dpo_collector_t *collector, size_t *out);
hu_error_t hu_dpo_clear(hu_dpo_collector_t *collector);
void hu_dpo_export_free(hu_allocator_t *alloc, hu_dpo_export_t *export_data);

#endif /* HU_ML_DPO_H */
