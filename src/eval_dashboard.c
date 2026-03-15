#include "human/eval_dashboard.h"
#include <stdio.h>
#include <string.h>

#define SUITE_COL_WIDTH 17
#define NUM_COL_WIDTH 6
#define RATE_COL_WIDTH 7
#define TIME_COL_WIDTH 6

static void truncate_suite_name(char *buf, size_t buf_size, const char *name) {
    if (!name) {
        buf[0] = '\0';
        return;
    }
    size_t len = strlen(name);
    if (len >= buf_size) {
        memcpy(buf, name, buf_size - 4);
        buf[buf_size - 4] = '.';
        buf[buf_size - 3] = '.';
        buf[buf_size - 2] = '.';
        buf[buf_size - 1] = '\0';
    } else {
        memcpy(buf, name, len + 1);
    }
}

hu_error_t hu_eval_dashboard_render(hu_allocator_t *alloc, FILE *out,
                                     const hu_eval_run_t *runs, size_t runs_count) {
    (void)alloc;
    if (!out) return HU_ERR_INVALID_ARGUMENT;

    fprintf(out, "╔═══════════════════════════════════════════════════════╗\n");
    fprintf(out, "║                   EVAL DASHBOARD                      ║\n");
    fprintf(out, "╠═══════════════════╤════════╤════════╤════════╤════════╣\n");
    fprintf(out, "║ Suite             │ Passed │ Failed │  Rate  │  Time  ║\n");
    fprintf(out, "╠═══════════════════╪════════╪════════╪════════╪════════╣\n");

    if (!runs || runs_count == 0) {
        fprintf(out, "║ (no runs)                                          ║\n");
        fprintf(out, "╠═══════════════════╧════════╧════════╧════════╧════════╣\n");
        fprintf(out, "║ Total: 0/0 passed (0.0%%) | 0ms elapsed               ║\n");
        fprintf(out, "╚═══════════════════════════════════════════════════════╝\n");
        return HU_OK;
    }

    size_t total_passed = 0;
    size_t total_failed = 0;
    int64_t total_elapsed = 0;

    for (size_t i = 0; i < runs_count; i++) {
        const hu_eval_run_t *r = &runs[i];
        size_t passed = r->passed;
        size_t failed = r->failed;
        double rate = (passed + failed) > 0 ? (100.0 * (double)passed / (double)(passed + failed)) : 0.0;
        int64_t elapsed = r->total_elapsed_ms;

        total_passed += passed;
        total_failed += failed;
        total_elapsed += elapsed;

        char suite_buf[SUITE_COL_WIDTH + 1];
        truncate_suite_name(suite_buf, sizeof(suite_buf), r->suite_name ? r->suite_name : "(unnamed)");

        fprintf(out, "║ %-15s │ %6zu │ %6zu │ %5.1f%% │ %5lldms ║\n",
                suite_buf, passed, failed, rate, (long long)elapsed);
    }

    size_t total = total_passed + total_failed;
    double total_rate = total > 0 ? (100.0 * (double)total_passed / (double)total) : 0.0;

    fprintf(out, "╠═══════════════════╧════════╧════════╧════════╧════════╣\n");
    fprintf(out, "║ Total: %zu/%zu passed (%.1f%%) | %lldms elapsed          ║\n",
            total_passed, total, total_rate, (long long)total_elapsed);
    fprintf(out, "╚═══════════════════════════════════════════════════════╝\n");

    return HU_OK;
}
