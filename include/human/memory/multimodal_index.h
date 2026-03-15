#ifndef HU_MEMORY_MULTIMODAL_INDEX_H
#define HU_MEMORY_MULTIMODAL_INDEX_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/multimodal.h"
#include <stddef.h>
#include <stdint.h>

typedef struct hu_multimodal_memory_entry {
    int64_t id;
    hu_modality_t modality;
    char description[512];
    size_t description_len;
    int64_t created_at;
} hu_multimodal_memory_entry_t;

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>
hu_error_t hu_multimodal_memory_init_tables(sqlite3 *db);
hu_error_t hu_multimodal_memory_store(hu_allocator_t *alloc, sqlite3 *db, hu_modality_t type,
                                       const char *description, size_t desc_len);
hu_error_t hu_multimodal_memory_search(hu_allocator_t *alloc, sqlite3 *db, const char *query,
                                        size_t query_len, hu_multimodal_memory_entry_t *results,
                                        size_t max_results, size_t *out_count);
#endif

#endif
