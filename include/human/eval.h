#ifndef HU_EVAL_H
#define HU_EVAL_H
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct hu_eval_task { char *id; char *prompt; size_t prompt_len; char *expected; size_t expected_len; char *category; int difficulty; int64_t timeout_ms; } hu_eval_task_t;
typedef struct hu_eval_result { char *task_id; bool passed; char *actual_output; size_t actual_output_len; int64_t elapsed_ms; int tool_calls_made; int tokens_used; char *error_msg; } hu_eval_result_t;
typedef struct hu_eval_suite { char *name; hu_eval_task_t *tasks; size_t tasks_count; } hu_eval_suite_t;
typedef struct hu_eval_run { char *suite_name; char *provider; char *model; hu_eval_result_t *results; size_t results_count; size_t passed; size_t failed; double pass_rate; int64_t total_elapsed_ms; int total_tokens; } hu_eval_run_t;
typedef enum { HU_EVAL_EXACT=0, HU_EVAL_CONTAINS, HU_EVAL_NUMERIC_CLOSE, HU_EVAL_LLM_JUDGE } hu_eval_match_mode_t;

hu_error_t hu_eval_suite_load_json(hu_allocator_t *alloc, const char *json, size_t json_len, hu_eval_suite_t *out);
hu_error_t hu_eval_check(hu_allocator_t *alloc, const char *actual, size_t actual_len, const char *expected, size_t expected_len, hu_eval_match_mode_t mode, bool *passed);
hu_error_t hu_eval_report_json(hu_allocator_t *alloc, const hu_eval_run_t *run, char **out, size_t *out_len);
hu_error_t hu_eval_compare(hu_allocator_t *alloc, const hu_eval_run_t *baseline, const hu_eval_run_t *current, char **report, size_t *report_len);
void hu_eval_suite_free(hu_allocator_t *alloc, hu_eval_suite_t *suite);
void hu_eval_run_free(hu_allocator_t *alloc, hu_eval_run_t *run);
void hu_eval_result_free(hu_allocator_t *alloc, hu_eval_result_t *result);
#endif
