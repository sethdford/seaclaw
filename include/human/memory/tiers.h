#ifndef HU_MEMORY_TIERS_H
#define HU_MEMORY_TIERS_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>
#endif

typedef enum hu_memory_tier {
    HU_TIER_CORE = 0,
    HU_TIER_RECALL,
    HU_TIER_ARCHIVAL
} hu_memory_tier_t;

typedef struct hu_core_memory {
    char user_name[128];
    char user_bio[512];
    char user_preferences[1024];
    char relationship_summary[512];
    char active_goals[512];
    int64_t updated_at;
} hu_core_memory_t;

typedef struct hu_tier_manager {
    hu_allocator_t *alloc;
#ifdef HU_ENABLE_SQLITE
    sqlite3 *db;
#else
    void *db;
#endif
    hu_core_memory_t core;
    size_t core_token_budget;
    size_t recall_token_budget;
} hu_tier_manager_t;

hu_error_t hu_tier_manager_create(hu_allocator_t *alloc,
#ifdef HU_ENABLE_SQLITE
                                  sqlite3 *db,
#else
                                  void *db,
#endif
                                  hu_tier_manager_t *out);
void hu_tier_manager_deinit(hu_tier_manager_t *mgr);

hu_error_t hu_tier_manager_init_tables(hu_tier_manager_t *mgr);
hu_error_t hu_tier_manager_load_core(hu_tier_manager_t *mgr);

hu_error_t hu_tier_manager_update_core(hu_tier_manager_t *mgr, const char *field,
                                       size_t field_len, const char *value, size_t value_len);

hu_error_t hu_tier_manager_store(hu_tier_manager_t *mgr, hu_memory_tier_t tier,
                                 const char *key, size_t key_len,
                                 const char *content, size_t content_len);

hu_error_t hu_tier_manager_promote(hu_tier_manager_t *mgr,
                                   const char *key, size_t key_len,
                                   hu_memory_tier_t from, hu_memory_tier_t to);

hu_error_t hu_tier_manager_demote(hu_tier_manager_t *mgr,
                                  const char *key, size_t key_len,
                                  hu_memory_tier_t from, hu_memory_tier_t to);

hu_error_t hu_tier_manager_build_core_prompt(hu_tier_manager_t *mgr,
                                             char *out, size_t out_cap, size_t *out_len);

hu_error_t hu_tier_manager_auto_tier(hu_tier_manager_t *mgr, const char *key,
                                     size_t key_len, const char *content,
                                     size_t content_len, hu_memory_tier_t *assigned);

const char *hu_memory_tier_str(hu_memory_tier_t tier);

#endif /* HU_MEMORY_TIERS_H */
