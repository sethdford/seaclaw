#ifndef HU_INTELLIGENCE_WEAKNESS_H
#define HU_INTELLIGENCE_WEAKNESS_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/eval.h"
#include <stddef.h>

typedef enum {
    HU_WEAKNESS_REASONING = 0,
    HU_WEAKNESS_KNOWLEDGE,
    HU_WEAKNESS_TOOL_USE,
    HU_WEAKNESS_FORMAT,
    HU_WEAKNESS_UNKNOWN
} hu_weakness_type_t;

typedef struct hu_weakness {
    hu_weakness_type_t type;
    char task_id[128];
    char category[64];
    char description[512];
    size_t description_len;
    char suggested_fix[512];
    size_t suggested_fix_len;
} hu_weakness_t;

typedef struct hu_weakness_report {
    hu_weakness_t *items;
    size_t count;
    size_t by_type[5];
} hu_weakness_report_t;

hu_error_t hu_weakness_analyze(hu_allocator_t *alloc, const hu_eval_run_t *run,
                               const hu_eval_suite_t *suite,
                               hu_weakness_report_t *out);

void hu_weakness_report_free(hu_allocator_t *alloc, hu_weakness_report_t *report);

const char *hu_weakness_type_str(hu_weakness_type_t type);

#endif
