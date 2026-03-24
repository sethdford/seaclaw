#include "human/observability/otlp.h"
#include "test_framework.h"
#include <string.h>

static void test_otlp_trace_init(void) {
    hu_otlp_trace_t trace;
    hu_otlp_trace_init(&trace);
    HU_ASSERT_EQ((int)trace.span_count, 0);
}

static void test_otlp_span_begin_end(void) {
    hu_otlp_trace_t trace;
    hu_otlp_trace_init(&trace);

    hu_otlp_span_t *span = NULL;
    HU_ASSERT_EQ(hu_otlp_span_begin(&trace, "test-span", 9, NULL, &span), HU_OK);
    HU_ASSERT_NOT_NULL(span);
    HU_ASSERT_EQ((int)trace.span_count, 1);
    HU_ASSERT_TRUE(span->start_ns > 0);
    HU_ASSERT_TRUE(strlen(span->trace_id) > 0);
    HU_ASSERT_TRUE(strlen(span->span_id) > 0);

    HU_ASSERT_EQ(hu_otlp_span_end(span, 1), HU_OK);
    HU_ASSERT_TRUE(span->end_ns >= span->start_ns);
    HU_ASSERT_EQ(span->status, 1);
}

static void test_otlp_child_span(void) {
    hu_otlp_trace_t trace;
    hu_otlp_trace_init(&trace);

    hu_otlp_span_t *parent = NULL;
    hu_otlp_span_begin(&trace, "parent", 6, NULL, &parent);

    hu_otlp_span_t *child = NULL;
    hu_otlp_span_begin(&trace, "child", 5, parent->span_id, &child);
    HU_ASSERT_NOT_NULL(child);
    HU_ASSERT_EQ((int)trace.span_count, 2);
    HU_ASSERT_STR_EQ(child->parent_span_id, parent->span_id);
    HU_ASSERT_STR_EQ(child->trace_id, parent->trace_id);
}

static void test_otlp_trace_to_json(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_otlp_trace_t trace;
    hu_otlp_trace_init(&trace);

    hu_otlp_span_t *span = NULL;
    hu_otlp_span_begin(&trace, "json-test", 9, NULL, &span);
    hu_otlp_span_end(span, 0);

    char *json = NULL;
    size_t json_len = 0;
    HU_ASSERT_EQ(hu_otlp_trace_to_json(&alloc, &trace, &json, &json_len), HU_OK);
    HU_ASSERT_NOT_NULL(json);
    HU_ASSERT_TRUE(json_len > 0);
    HU_ASSERT_STR_CONTAINS(json, "resourceSpans");
    HU_ASSERT_STR_CONTAINS(json, "json-test");
    alloc.free(alloc.ctx, json, json_len + 1);
}

static void test_otlp_span_null_args(void) {
    HU_ASSERT_EQ(hu_otlp_span_begin(NULL, NULL, 0, NULL, NULL), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_otlp_span_end(NULL, 0), HU_ERR_INVALID_ARGUMENT);
}

static void test_cost_monitor_init(void) {
    hu_cost_monitor_t m;
    hu_cost_monitor_init(&m, 2.5);
    HU_ASSERT_EQ((int)m.window_filled, 0);
    HU_ASSERT_TRUE(m.anomaly_threshold > 2.0);
}

static void test_cost_monitor_record_and_baseline(void) {
    hu_cost_monitor_t m;
    hu_cost_monitor_init(&m, 2.0);
    hu_cost_monitor_record(&m, 1.0);
    hu_cost_monitor_record(&m, 2.0);
    hu_cost_monitor_record(&m, 3.0);
    double baseline = hu_cost_monitor_baseline(&m);
    HU_ASSERT_TRUE(baseline > 1.5 && baseline < 2.5);
}

static void test_cost_monitor_anomaly_detection(void) {
    hu_cost_monitor_t m;
    hu_cost_monitor_init(&m, 2.0);
    for (int i = 0; i < 10; i++)
        hu_cost_monitor_record(&m, 1.0);

    HU_ASSERT_FALSE(hu_cost_monitor_is_anomaly(&m, 1.5));
    HU_ASSERT_TRUE(hu_cost_monitor_is_anomaly(&m, 5.0));
}

static void test_cost_monitor_few_samples_no_anomaly(void) {
    hu_cost_monitor_t m;
    hu_cost_monitor_init(&m, 2.0);
    hu_cost_monitor_record(&m, 1.0);
    HU_ASSERT_FALSE(hu_cost_monitor_is_anomaly(&m, 100.0));
}

static void test_factuality_score_high_confidence(void) {
    hu_factuality_score_t score;
    const char *text = "According to source: http://example.com, the answer is 42.";
    HU_ASSERT_EQ(hu_factuality_score_text(text, strlen(text), &score), HU_OK);
    HU_ASSERT_TRUE(score.confidence > 0.7);
    HU_ASSERT_TRUE(score.has_citations);
}

static void test_factuality_score_hedging(void) {
    hu_factuality_score_t score;
    const char *text = "I think possibly the answer might be something.";
    HU_ASSERT_EQ(hu_factuality_score_text(text, strlen(text), &score), HU_OK);
    HU_ASSERT_TRUE(score.has_hedging);
    HU_ASSERT_TRUE(score.confidence < 0.7);
}

static void test_factuality_score_null(void) {
    HU_ASSERT_EQ(hu_factuality_score_text(NULL, 0, NULL), HU_ERR_INVALID_ARGUMENT);
}

void run_otlp_tests(void) {
    HU_TEST_SUITE("OTLP Observability");
    HU_RUN_TEST(test_otlp_trace_init);
    HU_RUN_TEST(test_otlp_span_begin_end);
    HU_RUN_TEST(test_otlp_child_span);
    HU_RUN_TEST(test_otlp_trace_to_json);
    HU_RUN_TEST(test_otlp_span_null_args);
    HU_RUN_TEST(test_cost_monitor_init);
    HU_RUN_TEST(test_cost_monitor_record_and_baseline);
    HU_RUN_TEST(test_cost_monitor_anomaly_detection);
    HU_RUN_TEST(test_cost_monitor_few_samples_no_anomaly);
    HU_RUN_TEST(test_factuality_score_high_confidence);
    HU_RUN_TEST(test_factuality_score_hedging);
    HU_RUN_TEST(test_factuality_score_null);
}
