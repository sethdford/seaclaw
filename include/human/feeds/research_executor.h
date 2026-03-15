#ifndef HU_FEEDS_RESEARCH_EXECUTOR_H
#define HU_FEEDS_RESEARCH_EXECUTOR_H
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>
#endif

typedef enum {
    HU_RESEARCH_ACTION_PROMPT_UPDATE = 0,
    HU_RESEARCH_ACTION_SKILL_CREATE,
    HU_RESEARCH_ACTION_KNOWLEDGE_ADD,
    HU_RESEARCH_ACTION_CONFIG_SUGGEST
} hu_research_action_type_t;

typedef struct hu_research_action {
    hu_research_action_type_t type;
    char description[512];
    size_t description_len;
    bool is_safe;      /* true = can execute without approval */
    bool executed;
    int64_t executed_at;
} hu_research_action_t;

hu_error_t hu_research_classify_action(const char *suggested, size_t suggested_len,
                                        hu_research_action_t *action);

#ifdef HU_ENABLE_SQLITE
hu_error_t hu_research_execute_safe(hu_allocator_t *alloc, sqlite3 *db,
                                     const hu_research_action_t *action);

hu_error_t hu_research_dedup_finding(hu_allocator_t *alloc, sqlite3 *db,
                                      const char *source, size_t source_len,
                                      const char *finding, size_t finding_len,
                                      bool *is_duplicate);
#endif

#endif
