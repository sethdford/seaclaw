#ifndef HU_MEMORY_ENGINES_H
#define HU_MEMORY_ENGINES_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/memory.h"
#include <stddef.h>

/* memory_lru — in-memory LRU cache */
hu_memory_t hu_memory_lru_create(hu_allocator_t *alloc, size_t max_entries);

/* postgres — PostgreSQL backend (returns HU_ERR_NOT_SUPPORTED when libpq not linked) */
hu_memory_t hu_postgres_memory_create(hu_allocator_t *alloc, const char *url, const char *schema,
                                      const char *table);

/* redis — Redis backend (returns HU_ERR_NOT_SUPPORTED when not linked) */
hu_memory_t hu_redis_memory_create(hu_allocator_t *alloc, const char *host, unsigned short port,
                                   const char *key_prefix);

/* lancedb — SQLite + vector backend (requires HU_ENABLE_SQLITE) */
hu_memory_t hu_lancedb_memory_create(hu_allocator_t *alloc, const char *db_path);

/* api — HTTP REST API backend (requires HU_ENABLE_CURL) */
hu_memory_t hu_api_memory_create(hu_allocator_t *alloc, const char *base_url, const char *api_key,
                                 unsigned int timeout_ms);

/* lucid — SQLite + lucid CLI backend (requires HU_ENABLE_SQLITE) */
hu_memory_t hu_lucid_memory_create(hu_allocator_t *alloc, const char *db_path,
                                   const char *workspace_dir);

#endif /* HU_MEMORY_ENGINES_H */
