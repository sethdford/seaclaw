#include "human/core/allocator.h"
#include "human/observability/otel.h"
#include "test_framework.h"
#include <string.h>

static void span_generates_trace_id(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_span_t *span = hu_span_start(&alloc, "test", 4);
    HU_ASSERT_NOT_NULL(span);
    const char *trace_id = hu_span_get_trace_id(span);
    HU_ASSERT_NOT_NULL(trace_id);
    HU_ASSERT_EQ(strlen(trace_id), (size_t)32);
    for (size_t i = 0; i < 32; i++) {
        char c = trace_id[i];
        HU_ASSERT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }
    HU_ASSERT_EQ(hu_span_end(span, &alloc), HU_OK);
}

static void span_generates_span_id(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_span_t *span = hu_span_start(&alloc, "test", 4);
    HU_ASSERT_NOT_NULL(span);
    const char *span_id = hu_span_get_span_id(span);
    HU_ASSERT_NOT_NULL(span_id);
    HU_ASSERT_EQ(strlen(span_id), (size_t)16);
    for (size_t i = 0; i < 16; i++) {
        char c = span_id[i];
        HU_ASSERT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }
    HU_ASSERT_EQ(hu_span_end(span, &alloc), HU_OK);
}

static void child_span_inherits_trace_id(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_span_t *parent = hu_span_start(&alloc, "parent", 6);
    HU_ASSERT_NOT_NULL(parent);
    hu_span_t *child = hu_span_start_child(&alloc, parent, "child", 5);
    HU_ASSERT_NOT_NULL(child);
    const char *ptid = hu_span_get_trace_id(parent);
    const char *ctid = hu_span_get_trace_id(child);
    HU_ASSERT_NOT_NULL(ptid);
    HU_ASSERT_NOT_NULL(ctid);
    HU_ASSERT_STR_EQ(ptid, ctid);
    const char *psid = hu_span_get_span_id(parent);
    const char *csid = hu_span_get_span_id(child);
    HU_ASSERT_NOT_NULL(psid);
    HU_ASSERT_NOT_NULL(csid);
    HU_ASSERT_TRUE(strcmp(psid, csid) != 0);
    HU_ASSERT_EQ(hu_span_end(child, &alloc), HU_OK);
    HU_ASSERT_EQ(hu_span_end(parent, &alloc), HU_OK);
}

static void genai_attrs_set(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_span_t *span = hu_span_start(&alloc, "genai", 5);
    HU_ASSERT_NOT_NULL(span);
    hu_span_set_genai_system(span, "openai");
    hu_span_set_genai_model(span, "gpt-4");
    hu_span_set_genai_tokens(span, 100, 50);
    hu_span_set_genai_operation(span, "chat");
    HU_ASSERT_EQ(hu_span_end(span, &alloc), HU_OK);
}

static void span_status_set(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_span_t *span = hu_span_start(&alloc, "status", 6);
    HU_ASSERT_NOT_NULL(span);
    hu_span_set_status(span, HU_SPAN_STATUS_OK);
    hu_span_set_status(span, HU_SPAN_STATUS_ERROR);
    HU_ASSERT_EQ(hu_span_end(span, &alloc), HU_OK);
}

static void null_safety(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_span_set_attr_str(NULL, "k", "v");
    hu_span_set_attr_int(NULL, "k", 1);
    hu_span_set_attr_double(NULL, "k", 1.0);
    hu_span_set_status(NULL, HU_SPAN_STATUS_OK);
    hu_span_get_trace_id(NULL);
    hu_span_get_span_id(NULL);
    hu_span_set_genai_system(NULL, "x");
    hu_span_set_genai_model(NULL, "x");
    hu_span_set_genai_tokens(NULL, 0, 0);
    hu_span_set_genai_operation(NULL, "x");
    hu_span_start_child(&alloc, NULL, "x", 1);
    HU_ASSERT_NULL(hu_span_start(NULL, "x", 1));
    HU_ASSERT_NULL(hu_span_start(&alloc, NULL, 0));
    hu_span_t *span = hu_span_start(&alloc, "x", 1);
    if (span) {
        HU_ASSERT_EQ(hu_span_end(span, &alloc), HU_OK);
    }
}

static void export_skipped_in_test_mode(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_span_t *span = hu_span_start(&alloc, "test_export", 10);
    HU_ASSERT_NOT_NULL(span);
    hu_span_set_attr_str(span, "key", "value");
    HU_ASSERT_EQ(hu_span_end(span, &alloc), HU_OK);
}

void run_otel_trace_tests(void) {
    HU_TEST_SUITE("OTLP Traces");
    HU_RUN_TEST(span_generates_trace_id);
    HU_RUN_TEST(span_generates_span_id);
    HU_RUN_TEST(child_span_inherits_trace_id);
    HU_RUN_TEST(genai_attrs_set);
    HU_RUN_TEST(span_status_set);
    HU_RUN_TEST(null_safety);
    HU_RUN_TEST(export_skipped_in_test_mode);
}
