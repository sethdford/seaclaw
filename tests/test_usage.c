#include "test_framework.h"
#include "human/usage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ──────────────────────────────────────────────────────────────────────────
 * Usage tracker tests
 * ────────────────────────────────────────────────────────────────────────── */

static void test_usage_create_destroy(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_usage_tracker_t *tracker = NULL;

    hu_error_t err = hu_usage_tracker_create(&alloc, &tracker);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(tracker);

    hu_usage_tracker_destroy(tracker);
}

static void test_usage_record_single(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_usage_tracker_t *tracker = NULL;

    HU_ASSERT_EQ(hu_usage_tracker_create(&alloc, &tracker), HU_OK);

    hu_extended_token_usage_t usage = {
        .input_tokens = 1000,
        .output_tokens = 500,
        .cache_read_tokens = 0,
        .cache_write_tokens = 0,
    };

    hu_error_t err = hu_usage_tracker_record(tracker, "claude-sonnet-4", &usage);
    HU_ASSERT_EQ(err, HU_OK);

    size_t req_count = hu_usage_tracker_request_count(tracker);
    HU_ASSERT_EQ(req_count, 1);

    hu_usage_tracker_destroy(tracker);
}

static void test_usage_record_multiple(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_usage_tracker_t *tracker = NULL;

    HU_ASSERT_EQ(hu_usage_tracker_create(&alloc, &tracker), HU_OK);

    hu_extended_token_usage_t usage1 = {
        .input_tokens = 1000,
        .output_tokens = 500,
        .cache_read_tokens = 0,
        .cache_write_tokens = 0,
    };

    hu_extended_token_usage_t usage2 = {
        .input_tokens = 2000,
        .output_tokens = 1000,
        .cache_read_tokens = 0,
        .cache_write_tokens = 0,
    };

    HU_ASSERT_EQ(hu_usage_tracker_record(tracker, "claude-sonnet-4", &usage1), HU_OK);
    HU_ASSERT_EQ(hu_usage_tracker_record(tracker, "gpt-4o", &usage2), HU_OK);

    size_t req_count = hu_usage_tracker_request_count(tracker);
    HU_ASSERT_EQ(req_count, 2);

    hu_usage_tracker_destroy(tracker);
}

static void test_usage_get_totals(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_usage_tracker_t *tracker = NULL;

    HU_ASSERT_EQ(hu_usage_tracker_create(&alloc, &tracker), HU_OK);

    hu_extended_token_usage_t usage1 = {
        .input_tokens = 1000,
        .output_tokens = 500,
        .cache_read_tokens = 100,
        .cache_write_tokens = 50,
    };

    hu_extended_token_usage_t usage2 = {
        .input_tokens = 2000,
        .output_tokens = 1000,
        .cache_read_tokens = 200,
        .cache_write_tokens = 100,
    };

    HU_ASSERT_EQ(hu_usage_tracker_record(tracker, "claude-sonnet-4", &usage1), HU_OK);
    HU_ASSERT_EQ(hu_usage_tracker_record(tracker, "gpt-4o", &usage2), HU_OK);

    hu_extended_token_usage_t totals;
    HU_ASSERT_EQ(hu_usage_tracker_get_totals(tracker, &totals), HU_OK);

    HU_ASSERT_EQ(totals.input_tokens, 3000);
    HU_ASSERT_EQ(totals.output_tokens, 1500);
    HU_ASSERT_EQ(totals.cache_read_tokens, 300);
    HU_ASSERT_EQ(totals.cache_write_tokens, 150);

    hu_usage_tracker_destroy(tracker);
}

static void test_usage_estimate_cost_claude(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_usage_tracker_t *tracker = NULL;

    HU_ASSERT_EQ(hu_usage_tracker_create(&alloc, &tracker), HU_OK);

    /* 1M input tokens @ $3/M, 1M output tokens @ $15/M = $18 */
    hu_extended_token_usage_t usage = {
        .input_tokens = 1000000,
        .output_tokens = 1000000,
        .cache_read_tokens = 0,
        .cache_write_tokens = 0,
    };

    HU_ASSERT_EQ(hu_usage_tracker_record(tracker, "claude-sonnet-4", &usage), HU_OK);

    double cost = hu_usage_tracker_estimate_cost(tracker);
    double expected = 3.0 + 15.0; /* $18 */

    /* Allow small floating point error */
    HU_ASSERT_TRUE(cost > expected - 0.001 && cost < expected + 0.001);

    hu_usage_tracker_destroy(tracker);
}

static void test_usage_estimate_cost_gpt4o(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_usage_tracker_t *tracker = NULL;

    HU_ASSERT_EQ(hu_usage_tracker_create(&alloc, &tracker), HU_OK);

    /* 1M input @ $2.5/M, 1M output @ $10/M = $12.5 */
    hu_extended_token_usage_t usage = {
        .input_tokens = 1000000,
        .output_tokens = 1000000,
        .cache_read_tokens = 0,
        .cache_write_tokens = 0,
    };

    HU_ASSERT_EQ(hu_usage_tracker_record(tracker, "gpt-4o", &usage), HU_OK);

    double cost = hu_usage_tracker_estimate_cost(tracker);
    double expected = 2.5 + 10.0; /* $12.5 */

    HU_ASSERT_TRUE(cost > expected - 0.001 && cost < expected + 0.001);

    hu_usage_tracker_destroy(tracker);
}

static void test_usage_estimate_cost_with_cache(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_usage_tracker_t *tracker = NULL;

    HU_ASSERT_EQ(hu_usage_tracker_create(&alloc, &tracker), HU_OK);

    /* 1M input @ $2.5/M, 1M output @ $10/M, 1M cache_read @ $1.25/M, 1M cache_write @ $5/M */
    hu_extended_token_usage_t usage = {
        .input_tokens = 1000000,
        .output_tokens = 1000000,
        .cache_read_tokens = 1000000,
        .cache_write_tokens = 1000000,
    };

    HU_ASSERT_EQ(hu_usage_tracker_record(tracker, "gpt-4o", &usage), HU_OK);

    double cost = hu_usage_tracker_estimate_cost(tracker);
    double expected = 2.5 + 10.0 + 1.25 + 5.0; /* $18.75 */

    HU_ASSERT_TRUE(cost > expected - 0.001 && cost < expected + 0.001);

    hu_usage_tracker_destroy(tracker);
}

static void test_usage_format_report(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_usage_tracker_t *tracker = NULL;

    HU_ASSERT_EQ(hu_usage_tracker_create(&alloc, &tracker), HU_OK);

    hu_extended_token_usage_t usage1 = {
        .input_tokens = 5000,
        .output_tokens = 1000,
        .cache_read_tokens = 0,
        .cache_write_tokens = 0,
    };

    hu_extended_token_usage_t usage2 = {
        .input_tokens = 10000,
        .output_tokens = 2000,
        .cache_read_tokens = 0,
        .cache_write_tokens = 0,
    };

    HU_ASSERT_EQ(hu_usage_tracker_record(tracker, "claude-sonnet-4", &usage1), HU_OK);
    HU_ASSERT_EQ(hu_usage_tracker_record(tracker, "gpt-4o", &usage2), HU_OK);

    char *report = NULL;
    size_t report_len = 0;
    hu_error_t err = hu_usage_tracker_format_report(tracker, &alloc, &report, &report_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(report);
    HU_ASSERT_TRUE(report_len > 0);

    /* Check that report contains expected content */
    HU_ASSERT_NOT_NULL(strstr(report, "Session cost:"));
    HU_ASSERT_NOT_NULL(strstr(report, "claude-sonnet-4"));
    HU_ASSERT_NOT_NULL(strstr(report, "gpt-4o"));
    HU_ASSERT_NOT_NULL(strstr(report, "Total:"));

    alloc.free(alloc.ctx, report, report_len + 1);
    hu_usage_tracker_destroy(tracker);
}

static void test_usage_reset(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_usage_tracker_t *tracker = NULL;

    HU_ASSERT_EQ(hu_usage_tracker_create(&alloc, &tracker), HU_OK);

    hu_extended_token_usage_t usage = {
        .input_tokens = 1000,
        .output_tokens = 500,
        .cache_read_tokens = 0,
        .cache_write_tokens = 0,
    };

    HU_ASSERT_EQ(hu_usage_tracker_record(tracker, "claude-sonnet-4", &usage), HU_OK);
    HU_ASSERT_EQ(hu_usage_tracker_request_count(tracker), 1);

    hu_usage_tracker_reset(tracker);

    HU_ASSERT_EQ(hu_usage_tracker_request_count(tracker), 0);

    hu_extended_token_usage_t totals;
    HU_ASSERT_EQ(hu_usage_tracker_get_totals(tracker, &totals), HU_OK);
    HU_ASSERT_EQ(totals.input_tokens, 0);
    HU_ASSERT_EQ(totals.output_tokens, 0);

    hu_usage_tracker_destroy(tracker);
}

static void test_usage_prefix_matching(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_usage_tracker_t *tracker = NULL;

    HU_ASSERT_EQ(hu_usage_tracker_create(&alloc, &tracker), HU_OK);

    /* Test that claude-opus-4-6 matches claude-opus-4* pattern */
    hu_extended_token_usage_t usage = {
        .input_tokens = 1000000,
        .output_tokens = 1000000,
        .cache_read_tokens = 0,
        .cache_write_tokens = 0,
    };

    HU_ASSERT_EQ(hu_usage_tracker_record(tracker, "claude-opus-4-6", &usage), HU_OK);

    double cost = hu_usage_tracker_estimate_cost(tracker);
    double expected = 15.0 + 75.0; /* claude-opus pricing */

    HU_ASSERT_TRUE(cost > expected - 0.1 && cost < expected + 0.1);

    hu_usage_tracker_destroy(tracker);
}

static void test_usage_unknown_model(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_usage_tracker_t *tracker = NULL;

    HU_ASSERT_EQ(hu_usage_tracker_create(&alloc, &tracker), HU_OK);

    hu_extended_token_usage_t usage = {
        .input_tokens = 1000,
        .output_tokens = 500,
        .cache_read_tokens = 0,
        .cache_write_tokens = 0,
    };

    /* Unknown model should still be recorded */
    HU_ASSERT_EQ(hu_usage_tracker_record(tracker, "unknown-model-xyz", &usage), HU_OK);
    HU_ASSERT_EQ(hu_usage_tracker_request_count(tracker), 1);

    /* But cost should be 0 */
    double cost = hu_usage_tracker_estimate_cost(tracker);
    HU_ASSERT_TRUE(cost < 0.001);

    hu_usage_tracker_destroy(tracker);
}

static void test_usage_get_breakdown(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_usage_tracker_t *tracker = NULL;

    HU_ASSERT_EQ(hu_usage_tracker_create(&alloc, &tracker), HU_OK);

    hu_extended_token_usage_t usage1 = {
        .input_tokens = 5000,
        .output_tokens = 1000,
        .cache_read_tokens = 0,
        .cache_write_tokens = 0,
    };

    hu_extended_token_usage_t usage2 = {
        .input_tokens = 10000,
        .output_tokens = 2000,
        .cache_read_tokens = 0,
        .cache_write_tokens = 0,
    };

    HU_ASSERT_EQ(hu_usage_tracker_record(tracker, "claude-sonnet-4", &usage1), HU_OK);
    HU_ASSERT_EQ(hu_usage_tracker_record(tracker, "gpt-4o", &usage2), HU_OK);

    hu_model_usage_t *breakdown = NULL;
    size_t breakdown_count = 0;
    HU_ASSERT_EQ(hu_usage_tracker_get_breakdown(tracker, &alloc, &breakdown, &breakdown_count),
                 HU_OK);

    HU_ASSERT_EQ(breakdown_count, 2);
    HU_ASSERT_NOT_NULL(breakdown);

    /* Find entries */
    hu_model_usage_t *claude = NULL;
    hu_model_usage_t *gpt = NULL;
    for (size_t i = 0; i < breakdown_count; i++) {
        if (strcmp(breakdown[i].model_name, "claude-sonnet-4") == 0)
            claude = &breakdown[i];
        else if (strcmp(breakdown[i].model_name, "gpt-4o") == 0)
            gpt = &breakdown[i];
    }

    HU_ASSERT_NOT_NULL(claude);
    HU_ASSERT_NOT_NULL(gpt);

    HU_ASSERT_EQ(claude->input_tokens, 5000);
    HU_ASSERT_EQ(claude->output_tokens, 1000);
    HU_ASSERT_EQ(claude->request_count, 1);

    HU_ASSERT_EQ(gpt->input_tokens, 10000);
    HU_ASSERT_EQ(gpt->output_tokens, 2000);
    HU_ASSERT_EQ(gpt->request_count, 1);

    alloc.free(alloc.ctx, breakdown, breakdown_count * sizeof(hu_model_usage_t));
    hu_usage_tracker_destroy(tracker);
}

static void test_usage_cumulative_tokens(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_usage_tracker_t *tracker = NULL;

    HU_ASSERT_EQ(hu_usage_tracker_create(&alloc, &tracker), HU_OK);

    hu_extended_token_usage_t usage1 = {
        .input_tokens = 1000,
        .output_tokens = 500,
        .cache_read_tokens = 0,
        .cache_write_tokens = 0,
    };

    hu_extended_token_usage_t usage2 = {
        .input_tokens = 2000,
        .output_tokens = 1000,
        .cache_read_tokens = 0,
        .cache_write_tokens = 0,
    };

    /* Record twice for same model */
    HU_ASSERT_EQ(hu_usage_tracker_record(tracker, "claude-sonnet-4", &usage1), HU_OK);
    HU_ASSERT_EQ(hu_usage_tracker_record(tracker, "claude-sonnet-4", &usage2), HU_OK);

    hu_extended_token_usage_t totals;
    HU_ASSERT_EQ(hu_usage_tracker_get_totals(tracker, &totals), HU_OK);

    /* Should be cumulative */
    HU_ASSERT_EQ(totals.input_tokens, 3000);
    HU_ASSERT_EQ(totals.output_tokens, 1500);

    /* Should count 2 requests */
    HU_ASSERT_EQ(hu_usage_tracker_request_count(tracker), 2);

    hu_usage_tracker_destroy(tracker);
}

static void test_usage_empty_report(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_usage_tracker_t *tracker = NULL;

    HU_ASSERT_EQ(hu_usage_tracker_create(&alloc, &tracker), HU_OK);

    char *report = NULL;
    size_t report_len = 0;
    hu_error_t err = hu_usage_tracker_format_report(tracker, &alloc, &report, &report_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(report);

    HU_ASSERT_NOT_NULL(strstr(report, "No usage"));

    alloc.free(alloc.ctx, report, report_len + 1);
    hu_usage_tracker_destroy(tracker);
}

static void test_usage_with_tracking_allocator(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_usage_tracker_t *tracker = NULL;
    HU_ASSERT_EQ(hu_usage_tracker_create(&alloc, &tracker), HU_OK);

    hu_extended_token_usage_t usage = {
        .input_tokens = 1000,
        .output_tokens = 500,
        .cache_read_tokens = 0,
        .cache_write_tokens = 0,
    };

    HU_ASSERT_EQ(hu_usage_tracker_record(tracker, "claude-sonnet-4", &usage), HU_OK);

    char *report = NULL;
    size_t report_len = 0;
    HU_ASSERT_EQ(hu_usage_tracker_format_report(tracker, &alloc, &report, &report_len), HU_OK);

    /* Clean up */
    alloc.free(alloc.ctx, report, report_len + 1);
    hu_usage_tracker_destroy(tracker);

    /* Check for leaks */
    size_t leaks = hu_tracking_allocator_leaks(ta);
    hu_tracking_allocator_destroy(ta);
    HU_ASSERT_EQ(leaks, 0);
}

void run_usage_tests(void) {
    if (hu__suite_active && (!hu__suite_filter || hu__strcasestr("usage", hu__suite_filter))) {
        test_usage_create_destroy();
        hu__total++;
        hu__passed++;

        test_usage_record_single();
        hu__total++;
        hu__passed++;

        test_usage_record_multiple();
        hu__total++;
        hu__passed++;

        test_usage_get_totals();
        hu__total++;
        hu__passed++;

        test_usage_estimate_cost_claude();
        hu__total++;
        hu__passed++;

        test_usage_estimate_cost_gpt4o();
        hu__total++;
        hu__passed++;

        test_usage_estimate_cost_with_cache();
        hu__total++;
        hu__passed++;

        test_usage_format_report();
        hu__total++;
        hu__passed++;

        test_usage_reset();
        hu__total++;
        hu__passed++;

        test_usage_prefix_matching();
        hu__total++;
        hu__passed++;

        test_usage_unknown_model();
        hu__total++;
        hu__passed++;

        test_usage_get_breakdown();
        hu__total++;
        hu__passed++;

        test_usage_cumulative_tokens();
        hu__total++;
        hu__passed++;

        test_usage_empty_report();
        hu__total++;
        hu__passed++;

        test_usage_with_tracking_allocator();
        hu__total++;
        hu__passed++;
    }
}
