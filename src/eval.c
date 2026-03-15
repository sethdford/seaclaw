#include "human/eval.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>

hu_error_t hu_eval_suite_load_json(hu_allocator_t *alloc, const char *json, size_t json_len, hu_eval_suite_t *out) {
    if (!alloc || !json || !json_len || !out) return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    const char *name_key = "\"name\"";
    const char *found = strstr(json, name_key);
    if (found) {
        const char *vs = strchr(found + strlen(name_key), '"');
        if (vs) { vs++;
            const char *ve = strchr(vs, '"');
            if (ve) {
                size_t nlen = (size_t)(ve - vs);
                out->name = alloc->alloc(alloc->ctx, nlen + 1);
                if (out->name) { memcpy(out->name, vs, nlen); out->name[nlen] = 0; }
            }
        }
    }
    if (!out->name) { out->name = alloc->alloc(alloc->ctx, 5); if (out->name) memcpy(out->name, "eval", 5); }
    return HU_OK;
}

hu_error_t hu_eval_check(hu_allocator_t *alloc, const char *actual, size_t actual_len, const char *expected, size_t expected_len, hu_eval_match_mode_t mode, bool *passed) {
    if (!alloc || !actual || !expected || !passed) return HU_ERR_INVALID_ARGUMENT;
    *passed = false;
    switch (mode) {
        case HU_EVAL_EXACT: *passed = (actual_len == expected_len && memcmp(actual, expected, actual_len) == 0); break;
        case HU_EVAL_CONTAINS:
            if (expected_len <= actual_len) { for (size_t i = 0; i <= actual_len - expected_len; i++) { if (memcmp(actual+i, expected, expected_len) == 0) { *passed = true; break; } } } break;
        case HU_EVAL_NUMERIC_CLOSE: { double a = atof(actual); double e = atof(expected); *passed = fabs(a - e) < 0.01; break; }
        case HU_EVAL_LLM_JUDGE: break;
    }
    return HU_OK;
}

hu_error_t hu_eval_report_json(hu_allocator_t *alloc, const hu_eval_run_t *run, char **out, size_t *out_len) {
    if (!alloc || !run || !out || !out_len) return HU_ERR_INVALID_ARGUMENT;
    char buf[1024];
    int n = snprintf(buf, sizeof(buf), "{\"suite\":\"%s\",\"passed\":%zu,\"failed\":%zu,\"pass_rate\":%.2f,\"elapsed_ms\":%" PRId64 "}",
        run->suite_name ? run->suite_name : "", run->passed, run->failed, run->pass_rate, run->total_elapsed_ms);
    if (n < 0) return HU_ERR_INVALID_ARGUMENT;
    *out = alloc->alloc(alloc->ctx, (size_t)n + 1);
    if (!*out) return HU_ERR_OUT_OF_MEMORY;
    memcpy(*out, buf, (size_t)n + 1);
    *out_len = (size_t)n;
    return HU_OK;
}

hu_error_t hu_eval_compare(hu_allocator_t *alloc, const hu_eval_run_t *baseline, const hu_eval_run_t *current, char **report, size_t *report_len) {
    if (!alloc || !baseline || !current || !report || !report_len) return HU_ERR_INVALID_ARGUMENT;
    char buf[512];
    double delta = current->pass_rate - baseline->pass_rate;
    int n = snprintf(buf, sizeof(buf), "{\"baseline\":%.2f,\"current\":%.2f,\"delta\":%.2f}", baseline->pass_rate, current->pass_rate, delta);
    if (n < 0) return HU_ERR_INVALID_ARGUMENT;
    *report = alloc->alloc(alloc->ctx, (size_t)n + 1);
    if (!*report) return HU_ERR_OUT_OF_MEMORY;
    memcpy(*report, buf, (size_t)n + 1);
    *report_len = (size_t)n;
    return HU_OK;
}

void hu_eval_suite_free(hu_allocator_t *alloc, hu_eval_suite_t *suite) {
    if (!alloc || !suite) return;
    if (suite->name) { alloc->free(alloc->ctx, suite->name, strlen(suite->name)+1); suite->name = NULL; }
}
void hu_eval_run_free(hu_allocator_t *alloc, hu_eval_run_t *run) { (void)alloc; (void)run; }
void hu_eval_result_free(hu_allocator_t *alloc, hu_eval_result_t *result) { (void)alloc; (void)result; }
