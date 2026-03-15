#ifndef HU_FEEDS_FINDINGS_H
#define HU_FEEDS_FINDINGS_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>
#include <stdint.h>

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>

typedef struct hu_research_finding {
    int64_t id;
    char source[64];
    char finding[2048];
    char relevance[256];
    char priority[16];
    char suggested_action[1024];
    char status[16];
    int64_t created_at;
} hu_research_finding_t;

hu_error_t hu_findings_store(hu_allocator_t *alloc, sqlite3 *db,
                             const char *source, const char *finding,
                             const char *relevance, const char *priority,
                             const char *suggested_action);
hu_error_t hu_findings_get_pending(hu_allocator_t *alloc, sqlite3 *db,
                                   size_t limit,
                                   hu_research_finding_t **out, size_t *out_count);
void hu_findings_free(hu_allocator_t *alloc, hu_research_finding_t *items, size_t count);
hu_error_t hu_findings_mark_status(sqlite3 *db, int64_t id, const char *status);

#endif /* HU_ENABLE_SQLITE */
#endif
