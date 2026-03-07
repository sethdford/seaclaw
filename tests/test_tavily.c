/* Tests for sc_web_search_tavily */
#include "seaclaw/core/allocator.h"
#include "seaclaw/tools/web_search_providers.h"
#include "test_framework.h"

static void test_tavily_null_alloc(void) {
    sc_tool_result_t result = {0};
    sc_error_t err = sc_web_search_tavily(NULL, "query", 5, 1, "key", &result);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
}

static void test_tavily_null_query(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_result_t result = {0};
    sc_error_t err = sc_web_search_tavily(&alloc, NULL, 0, 1, "key", &result);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
}

static void test_tavily_null_api_key(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_result_t result = {0};
    sc_error_t err = sc_web_search_tavily(&alloc, "query", 5, 1, NULL, &result);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
}

static void test_tavily_null_out(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_error_t err = sc_web_search_tavily(&alloc, "query", 5, 1, "key", NULL);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
}

static void test_tavily_zero_query_len(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_result_t result = {0};
    sc_error_t err = sc_web_search_tavily(&alloc, "q", 0, 1, "key", &result);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
}

static void test_tavily_count_zero(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_result_t result = {0};
    sc_error_t err = sc_web_search_tavily(&alloc, "query", 5, 0, "key", &result);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
}

static void test_tavily_count_over_ten(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_result_t result = {0};
    sc_error_t err = sc_web_search_tavily(&alloc, "query", 5, 11, "key", &result);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
}

#if SC_IS_TEST
/* In test mode, sc_http_post_json returns SC_OK with mock JSON. The mock has no "results"
 * key, so Tavily returns "No web results found." with output_owned=true. */
static void test_tavily_valid_args_returns_result(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_result_t result = {0};
    sc_error_t err = sc_web_search_tavily(&alloc, "test query", 10, 3, "test-key", &result);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_TRUE(result.success);
    SC_ASSERT_NOT_NULL(result.output);
    SC_ASSERT_TRUE(result.output_len > 0);
    sc_tool_result_free(&alloc, &result);
}
#endif

void run_tavily_tests(void) {
    SC_TEST_SUITE("Tavily web search");
    SC_RUN_TEST(test_tavily_null_alloc);
    SC_RUN_TEST(test_tavily_null_query);
    SC_RUN_TEST(test_tavily_null_api_key);
    SC_RUN_TEST(test_tavily_null_out);
    SC_RUN_TEST(test_tavily_zero_query_len);
    SC_RUN_TEST(test_tavily_count_zero);
    SC_RUN_TEST(test_tavily_count_over_ten);
#if SC_IS_TEST
    SC_RUN_TEST(test_tavily_valid_args_returns_result);
#endif
}
