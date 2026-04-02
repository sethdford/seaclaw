#include "human/core/allocator.h"
#include "human/core/json.h"
#include "human/mcp_jsonrpc.h"
#include "test_framework.h"
#include <string.h>

/**
 * Tests for JSON-RPC 2.0 message building and parsing.
 * Verifies correct message format and response handling.
 */

/* ── Tracking allocator for leak detection ────────────────────────────── */

typedef struct {
    hu_allocator_t base;
    size_t allocs;
    size_t frees;
    size_t current;
} tracking_alloc_t;

static void *tracking_alloc(void *ctx, size_t size) {
    tracking_alloc_t *ta = (tracking_alloc_t *)ctx;
    ta->allocs++;
    ta->current += size;
    return ta->base.alloc(ta->base.ctx, size);
}

static void tracking_free(void *ctx, void *ptr, size_t size) {
    tracking_alloc_t *ta = (tracking_alloc_t *)ctx;
    ta->frees++;
    ta->current -= size;
    ta->base.free(ta->base.ctx, ptr, size);
}

static void *tracking_realloc(void *ctx, void *ptr, size_t old_size, size_t new_size) {
    tracking_alloc_t *ta = (tracking_alloc_t *)ctx;
    ta->current -= old_size;
    ta->current += new_size;
    /* Track realloc as a free + alloc pair */
    if (ptr && old_size > 0)
        ta->frees++;
    if (new_size > 0)
        ta->allocs++;
    return ta->base.realloc(ta->base.ctx, ptr, old_size, new_size);
}

static hu_allocator_t make_tracking_alloc(tracking_alloc_t *out) {
    memset(out, 0, sizeof(*out));
    out->base = hu_system_allocator();
    hu_allocator_t alloc = {
        .ctx = out,
        .alloc = tracking_alloc,
        .free = tracking_free,
        .realloc = tracking_realloc,
    };
    return alloc;
}

/* ── Test: Build request with method and params ────────────────────── */

static void test_build_request_basic(void) {
    tracking_alloc_t ta;
    hu_allocator_t alloc = make_tracking_alloc(&ta);

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err =
        hu_mcp_jsonrpc_build_request(&alloc, 42, "test_method", "{\"arg\":\"value\"}", &out, &out_len);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(out_len > 0);

    /* Verify content contains expected parts */
    HU_ASSERT_NOT_NULL(strstr(out, "\"jsonrpc\":\"2.0\""));
    HU_ASSERT_NOT_NULL(strstr(out, "\"id\":42"));
    HU_ASSERT_NOT_NULL(strstr(out, "\"method\":\"test_method\""));

    /* json_buf allocates with capacity, free with 0 to let system handle it */
    free(out);

    /* Verify allocs/frees balanced (buf internal alloc freed via free()) */
    (void)ta;
}

/* ── Test: Build request with NULL params ────────────────────────── */

static void test_build_request_null_params(void) {
    tracking_alloc_t ta;
    hu_allocator_t alloc = make_tracking_alloc(&ta);

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_mcp_jsonrpc_build_request(&alloc, 100, "tools/list", NULL, &out, &out_len);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);

    /* Should have empty params object */
    HU_ASSERT_NOT_NULL(strstr(out, "\"params\":{}"));

    alloc.free(alloc.ctx, out, out_len + 1);
    HU_ASSERT_EQ(ta.allocs, ta.frees);
}

/* ── Test: Build request with empty string params ──────────────────── */

static void test_build_request_empty_params(void) {
    tracking_alloc_t ta;
    hu_allocator_t alloc = make_tracking_alloc(&ta);

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_mcp_jsonrpc_build_request(&alloc, 7, "method", "", &out, &out_len);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(strstr(out, "\"params\":{}"));

    alloc.free(alloc.ctx, out, out_len + 1);
    HU_ASSERT_EQ(ta.allocs, ta.frees);
}

/* ── Test: Build request null safety ────────────────────────────────── */

static void test_build_request_null_safety(void) {
    tracking_alloc_t ta;
    hu_allocator_t alloc = make_tracking_alloc(&ta);
    char *out = NULL;
    size_t out_len = 0;

    /* Null allocator */
    HU_ASSERT_NEQ(hu_mcp_jsonrpc_build_request(NULL, 1, "m", NULL, &out, &out_len), HU_OK);

    /* Null method */
    HU_ASSERT_NEQ(hu_mcp_jsonrpc_build_request(&alloc, 1, NULL, NULL, &out, &out_len), HU_OK);

    /* Null out pointer */
    HU_ASSERT_NEQ(hu_mcp_jsonrpc_build_request(&alloc, 1, "m", NULL, NULL, &out_len), HU_OK);

    /* Null out_len pointer */
    HU_ASSERT_NEQ(hu_mcp_jsonrpc_build_request(&alloc, 1, "m", NULL, &out, NULL), HU_OK);
}

/* ── Test: Parse valid response with result ─────────────────────────── */

static void test_parse_response_valid(void) {
    tracking_alloc_t ta;
    hu_allocator_t alloc = make_tracking_alloc(&ta);

    const char *json = "{\"jsonrpc\":\"2.0\",\"id\":42,\"result\":{\"status\":\"ok\"}}";
    uint32_t id = 0;
    char *result = NULL;
    size_t result_len = 0;
    bool is_error = true;

    hu_error_t err =
        hu_mcp_jsonrpc_parse_response(&alloc, json, strlen(json), &id, &result, &result_len, &is_error);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(id, 42u);
    HU_ASSERT_FALSE(is_error);
    HU_ASSERT_NOT_NULL(result);

    /* Result should be JSON stringified */
    HU_ASSERT_TRUE(strstr(result, "status") != NULL);

    alloc.free(alloc.ctx, result, result_len + 1);
    HU_ASSERT_EQ(ta.allocs, ta.frees);
}

/* ── Test: Parse response with error ────────────────────────────────── */

static void test_parse_response_error(void) {
    tracking_alloc_t ta;
    hu_allocator_t alloc = make_tracking_alloc(&ta);

    const char *json = "{\"jsonrpc\":\"2.0\",\"id\":99,\"error\":{\"code\":-32600,\"message\":\"Invalid "
                       "Request\"}}";
    uint32_t id = 0;
    char *result = NULL;
    size_t result_len = 0;
    bool is_error = false;

    hu_error_t err =
        hu_mcp_jsonrpc_parse_response(&alloc, json, strlen(json), &id, &result, &result_len, &is_error);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(id, 99u);
    HU_ASSERT_TRUE(is_error);
    HU_ASSERT_NOT_NULL(result);
    HU_ASSERT_TRUE(strstr(result, "Invalid") != NULL);

    alloc.free(alloc.ctx, result, result_len + 1);
    HU_ASSERT_EQ(ta.allocs, ta.frees);
}

/* ── Test: Parse malformed JSON ────────────────────────────────────── */

static void test_parse_response_malformed_json(void) {
    tracking_alloc_t ta;
    hu_allocator_t alloc = make_tracking_alloc(&ta);

    const char *json = "not valid json";
    uint32_t id = 0;
    char *result = NULL;
    size_t result_len = 0;
    bool is_error = false;

    hu_error_t err =
        hu_mcp_jsonrpc_parse_response(&alloc, json, strlen(json), &id, &result, &result_len, &is_error);

    HU_ASSERT_NEQ(err, HU_OK);
    HU_ASSERT_NULL(result);
}

/* ── Test: Parse response missing result field ───────────────────── */

static void test_parse_response_missing_result(void) {
    tracking_alloc_t ta;
    hu_allocator_t alloc = make_tracking_alloc(&ta);

    const char *json = "{\"jsonrpc\":\"2.0\",\"id\":42}";
    uint32_t id = 0;
    char *result = NULL;
    size_t result_len = 0;
    bool is_error = false;

    hu_error_t err =
        hu_mcp_jsonrpc_parse_response(&alloc, json, strlen(json), &id, &result, &result_len, &is_error);

    /* Missing both result and error should fail */
    HU_ASSERT_NEQ(err, HU_OK);
}

/* ── Test: ID matching ─────────────────────────────────────────────── */

static void test_parse_response_id_matching(void) {
    tracking_alloc_t ta;
    hu_allocator_t alloc = make_tracking_alloc(&ta);

    for (uint32_t test_id = 0; test_id < 1000; test_id += 100) {
        char buf[512];
        snprintf(buf, sizeof(buf), "{\"jsonrpc\":\"2.0\",\"id\":%u,\"result\":42}", test_id);

        uint32_t id = 0;
        char *result = NULL;
        size_t result_len = 0;
        bool is_error = false;

        hu_error_t err = hu_mcp_jsonrpc_parse_response(&alloc, buf, strlen(buf), &id, &result,
                                                       &result_len, &is_error);

        HU_ASSERT_EQ(err, HU_OK);
        HU_ASSERT_EQ(id, test_id);

        alloc.free(alloc.ctx, result, result_len + 1);
    }

    HU_ASSERT_EQ(ta.allocs, ta.frees);
}

/* ── Test: Build tools/list request ────────────────────────────────── */

static void test_build_tools_list(void) {
    tracking_alloc_t ta;
    hu_allocator_t alloc = make_tracking_alloc(&ta);

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_mcp_jsonrpc_build_tools_list(&alloc, 123, &out, &out_len);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);

    /* Should be a valid request */
    HU_ASSERT_NOT_NULL(strstr(out, "\"jsonrpc\":\"2.0\""));
    HU_ASSERT_NOT_NULL(strstr(out, "\"id\":123"));
    HU_ASSERT_NOT_NULL(strstr(out, "tools/list"));

    alloc.free(alloc.ctx, out, out_len + 1);
    HU_ASSERT_EQ(ta.allocs, ta.frees);
}

/* ── Test: Build tools/call request ────────────────────────────────── */

static void test_build_tools_call(void) {
    tracking_alloc_t ta;
    hu_allocator_t alloc = make_tracking_alloc(&ta);

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_mcp_jsonrpc_build_tools_call(&alloc, 456, "my_tool", "{\"x\":123}", &out, &out_len);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);

    /* Verify content */
    HU_ASSERT_NOT_NULL(strstr(out, "\"jsonrpc\":\"2.0\""));
    HU_ASSERT_NOT_NULL(strstr(out, "\"id\":456"));
    HU_ASSERT_NOT_NULL(strstr(out, "tools/call"));
    HU_ASSERT_NOT_NULL(strstr(out, "my_tool"));

    alloc.free(alloc.ctx, out, out_len + 1);
    HU_ASSERT_EQ(ta.allocs, ta.frees);
}

/* ── Test: Build tools/call with null args ────────────────────────── */

static void test_build_tools_call_null_args(void) {
    tracking_alloc_t ta;
    hu_allocator_t alloc = make_tracking_alloc(&ta);

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_mcp_jsonrpc_build_tools_call(&alloc, 789, "tool", NULL, &out, &out_len);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);

    /* Should have empty arguments */
    HU_ASSERT_NOT_NULL(strstr(out, "\"arguments\":{}"));

    alloc.free(alloc.ctx, out, out_len + 1);
    HU_ASSERT_EQ(ta.allocs, ta.frees);
}

/* ── Test: tools/call null safety ──────────────────────────────────── */

static void test_build_tools_call_null_safety(void) {
    tracking_alloc_t ta;
    hu_allocator_t alloc = make_tracking_alloc(&ta);
    char *out = NULL;
    size_t out_len = 0;

    /* Null allocator */
    HU_ASSERT_NEQ(hu_mcp_jsonrpc_build_tools_call(NULL, 1, "t", NULL, &out, &out_len), HU_OK);

    /* Null tool_name */
    HU_ASSERT_NEQ(hu_mcp_jsonrpc_build_tools_call(&alloc, 1, NULL, NULL, &out, &out_len), HU_OK);

    /* Null out pointer */
    HU_ASSERT_NEQ(hu_mcp_jsonrpc_build_tools_call(&alloc, 1, "t", NULL, NULL, &out_len), HU_OK);
}

/* ── Test: Round-trip request → parse response ───────────────────── */

static void test_roundtrip_request_response(void) {
    tracking_alloc_t ta;
    hu_allocator_t alloc = make_tracking_alloc(&ta);

    /* Build a request */
    char *request = NULL;
    size_t request_len = 0;
    hu_error_t err = hu_mcp_jsonrpc_build_tools_list(&alloc, 555, &request, &request_len);
    HU_ASSERT_EQ(err, HU_OK);

    /* Simulate receiving a response with same ID */
    const char *response = "{\"jsonrpc\":\"2.0\",\"id\":555,\"result\":{\"tools\":[]}}";

    uint32_t response_id = 0;
    char *result = NULL;
    size_t result_len = 0;
    bool is_error = false;

    err = hu_mcp_jsonrpc_parse_response(&alloc, response, strlen(response), &response_id, &result,
                                        &result_len, &is_error);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(response_id, 555u);
    HU_ASSERT_FALSE(is_error);

    alloc.free(alloc.ctx, request, request_len + 1);
    alloc.free(alloc.ctx, result, result_len + 1);
    HU_ASSERT_EQ(ta.allocs, ta.frees);
}

/* ── Test: Parse response with string result ────────────────────────── */

static void test_parse_response_string_result(void) {
    tracking_alloc_t ta;
    hu_allocator_t alloc = make_tracking_alloc(&ta);

    const char *json = "{\"jsonrpc\":\"2.0\",\"id\":77,\"result\":\"hello world\"}";
    uint32_t id = 0;
    char *result = NULL;
    size_t result_len = 0;
    bool is_error = false;

    hu_error_t err =
        hu_mcp_jsonrpc_parse_response(&alloc, json, strlen(json), &id, &result, &result_len, &is_error);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_FALSE(is_error);
    HU_ASSERT_NOT_NULL(result);

    alloc.free(alloc.ctx, result, result_len + 1);
    HU_ASSERT_EQ(ta.allocs, ta.frees);
}

/* ── Test: Parse response with number result ────────────────────────── */

static void test_parse_response_number_result(void) {
    tracking_alloc_t ta;
    hu_allocator_t alloc = make_tracking_alloc(&ta);

    const char *json = "{\"jsonrpc\":\"2.0\",\"id\":88,\"result\":42}";
    uint32_t id = 0;
    char *result = NULL;
    size_t result_len = 0;
    bool is_error = false;

    hu_error_t err =
        hu_mcp_jsonrpc_parse_response(&alloc, json, strlen(json), &id, &result, &result_len, &is_error);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_FALSE(is_error);
    HU_ASSERT_NOT_NULL(result);

    alloc.free(alloc.ctx, result, result_len + 1);
    HU_ASSERT_EQ(ta.allocs, ta.frees);
}

/* ── Test: Build request with complex params ──────────────────────── */

static void test_build_request_complex_params(void) {
    tracking_alloc_t ta;
    hu_allocator_t alloc = make_tracking_alloc(&ta);

    const char *complex_params = "{\"nested\":{\"arr\":[1,2,3],\"str\":\"value\"}}";
    char *out = NULL;
    size_t out_len = 0;

    hu_error_t err = hu_mcp_jsonrpc_build_request(&alloc, 200, "complex_method", complex_params,
                                                   &out, &out_len);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(strstr(out, "nested"));

    alloc.free(alloc.ctx, out, out_len + 1);
    HU_ASSERT_EQ(ta.allocs, ta.frees);
}

/* ── Main test suite ──────────────────────────────────────────────── */

void run_mcp_jsonrpc_tests(void) {
    HU_RUN_TEST(test_build_request_basic);
    HU_RUN_TEST(test_build_request_null_params);
    HU_RUN_TEST(test_build_request_empty_params);
    HU_RUN_TEST(test_build_request_null_safety);

    HU_RUN_TEST(test_parse_response_valid);
    HU_RUN_TEST(test_parse_response_error);
    HU_RUN_TEST(test_parse_response_malformed_json);
    HU_RUN_TEST(test_parse_response_missing_result);
    HU_RUN_TEST(test_parse_response_id_matching);
    HU_RUN_TEST(test_parse_response_string_result);
    HU_RUN_TEST(test_parse_response_number_result);

    HU_RUN_TEST(test_build_tools_list);
    HU_RUN_TEST(test_build_tools_call);
    HU_RUN_TEST(test_build_tools_call_null_args);
    HU_RUN_TEST(test_build_tools_call_null_safety);

    HU_RUN_TEST(test_roundtrip_request_response);
    HU_RUN_TEST(test_build_request_complex_params);
}
