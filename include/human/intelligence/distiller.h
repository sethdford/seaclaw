#ifndef HU_INTELLIGENCE_DISTILLER_H
#define HU_INTELLIGENCE_DISTILLER_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>
#include <stdint.h>

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>

/*
 * Experience Distiller — summarize recurring experience patterns into
 * general lessons. Mines experience_log for repeated themes and creates
 * entries in general_lessons.
 */

hu_error_t hu_distiller_init_tables(sqlite3 *db);

hu_error_t hu_experience_distill(hu_allocator_t *alloc, sqlite3 *db,
                                  size_t min_occurrences, int64_t now_ts,
                                  size_t *lessons_created);

#endif /* HU_ENABLE_SQLITE */
#endif /* HU_INTELLIGENCE_DISTILLER_H */
