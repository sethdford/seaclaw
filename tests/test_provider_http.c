#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/json.h"
#include "human/providers/provider_http.h"
#include "test_framework.h"
#include <string.h>

static void test_provider_http_null_alloc_returns_error(void) {
    hu_json_value_t *parsed = NULL;
    const char *body = "{}";
    hu_error_t err = hu_provider_http_post_json(NULL, "https://api.example.com/", NULL, NULL, body,
                                                strlen(body), &parsed);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_NULL(parsed);
}

static void test_provider_http_null_url_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_json_value_t *parsed = NULL;
    const char *body = "{}";
    hu_error_t err = hu_provider_http_post_json(&alloc, NULL, NULL, NULL, body, strlen(body),
                                                &parsed);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_NULL(parsed);
}

static void test_provider_http_null_parsed_out_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *body = "{}";
    hu_error_t err = hu_provider_http_post_json(&alloc, "https://api.example.com/", NULL, NULL,
                                                body, strlen(body), NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void test_provider_http_post_json_mock_succeeds(void) {
#if HU_IS_TEST
    hu_allocator_t alloc = hu_system_allocator();
    hu_json_value_t *parsed = NULL;
    const char *body = "{\"model\":\"gpt-4\"}";
    hu_error_t err = hu_provider_http_post_json(&alloc, "https://api.example.com/", NULL, NULL,
                                                body, strlen(body), &parsed);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(parsed);
    hu_json_free(&alloc, parsed);
#endif
}

static void test_provider_http_post_json_with_extra_headers_mock_succeeds(void) {
#if HU_IS_TEST
    hu_allocator_t alloc = hu_system_allocator();
    hu_json_value_t *parsed = NULL;
    const char *body = "{\"model\":\"claude-3\"}";
    const char *extra = "x-api-key: test-key\r\nanthropic-version: 2023-06-01\r\n";
    hu_error_t err = hu_provider_http_post_json(&alloc, "https://api.anthropic.com/", NULL, extra,
                                                body, strlen(body), &parsed);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(parsed);
    hu_json_free(&alloc, parsed);
#endif
}

static void test_provider_http_post_json_with_auth_header_mock_succeeds(void) {
#if HU_IS_TEST
    hu_allocator_t alloc = hu_system_allocator();
    hu_json_value_t *parsed = NULL;
    const char *body = "{}";
    const char *auth = "Bearer sk-test-key";
    hu_error_t err = hu_provider_http_post_json(&alloc, "https://api.example.com/", auth, NULL,
                                                body, strlen(body), &parsed);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(parsed);
    hu_json_free(&alloc, parsed);
#endif
}

static void test_provider_http_post_json_empty_body_mock_succeeds(void) {
#if HU_IS_TEST
    hu_allocator_t alloc = hu_system_allocator();
    hu_json_value_t *parsed = NULL;
    hu_error_t err = hu_provider_http_post_json(&alloc, "https://api.example.com/", NULL, NULL,
                                                "", 0, &parsed);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(parsed);
    hu_json_free(&alloc, parsed);
#endif
}

void run_provider_http_tests(void) {
    HU_TEST_SUITE("ProviderHttp");
    HU_RUN_TEST(test_provider_http_null_alloc_returns_error);
    HU_RUN_TEST(test_provider_http_null_url_returns_error);
    HU_RUN_TEST(test_provider_http_null_parsed_out_returns_error);
    HU_RUN_TEST(test_provider_http_post_json_mock_succeeds);
    HU_RUN_TEST(test_provider_http_post_json_with_extra_headers_mock_succeeds);
    HU_RUN_TEST(test_provider_http_post_json_with_auth_header_mock_succeeds);
    HU_RUN_TEST(test_provider_http_post_json_empty_body_mock_succeeds);
}
