#ifndef HU_MEMORY_EVOLVED_OPINIONS_H
#define HU_MEMORY_EVOLVED_OPINIONS_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/humanness.h"
#include <stddef.h>
#include <stdint.h>

/*
 * Evolved opinion persistence — stores perspectives that develop through
 * repeated conversation, with conviction blending and interaction counting.
 *
 * Table: evolved_opinions (separate from F65 opinions in cognitive.h)
 */

#ifdef HU_ENABLE_SQLITE

#include <sqlite3.h>

hu_error_t hu_evolved_opinions_ensure_table(sqlite3 *db);

hu_error_t hu_evolved_opinion_upsert(sqlite3 *db, const char *topic, size_t topic_len,
                                     const char *stance, size_t stance_len, double conviction,
                                     int64_t now_ts);

/* Extract opinionated statements from a response and upsert them.
 * Scans for "I think", "I believe", "I prefer", etc. and stores
 * the surrounding clause as topic+stance with moderate conviction. */
hu_error_t hu_evolved_opinions_extract_and_store(sqlite3 *db, const char *response,
                                                 size_t response_len, int64_t now_ts);

hu_error_t hu_evolved_opinions_get(hu_allocator_t *alloc, sqlite3 *db, double min_conviction,
                                   size_t limit, hu_evolved_opinion_t **out, size_t *out_count);

void hu_evolved_opinions_free(hu_allocator_t *alloc, hu_evolved_opinion_t *opinions, size_t count);

#endif /* HU_ENABLE_SQLITE */

#endif /* HU_MEMORY_EVOLVED_OPINIONS_H */
