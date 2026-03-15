#ifndef HU_INTELLIGENCE_CYCLE_H
#define HU_INTELLIGENCE_CYCLE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>

typedef struct hu_intelligence_cycle_result {
    size_t findings_actioned;
    size_t lessons_extracted;
    size_t events_recorded;
    size_t values_learned;
    size_t causal_recorded;
    size_t skills_updated;
} hu_intelligence_cycle_result_t;

hu_error_t hu_intelligence_run_cycle(hu_allocator_t *alloc, sqlite3 *db,
                                    hu_intelligence_cycle_result_t *out);

#endif /* HU_ENABLE_SQLITE */
#endif
