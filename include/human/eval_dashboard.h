#ifndef HU_EVAL_DASHBOARD_H
#define HU_EVAL_DASHBOARD_H
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/eval.h"
#include <stdio.h>

hu_error_t hu_eval_dashboard_render(hu_allocator_t *alloc, FILE *out,
                                     const hu_eval_run_t *runs, size_t runs_count);

hu_error_t hu_eval_dashboard_render_trend(hu_allocator_t *alloc, FILE *out,
                                          const hu_eval_run_t *baseline, size_t baseline_count,
                                          const hu_eval_run_t *current, size_t current_count);

#endif
