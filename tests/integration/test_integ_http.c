#include "test_framework.h"
#include "human/core/allocator.h"
#include "human/core/http.h"
#include <string.h>

#if defined(HU_HTTP_CURL)
static void integ_http_get_example_com(void) {
    hu_allocator_t a = hu_system_allocator();
    hu_http_response_t r;
    memset(&r, 0, sizeof(r));
    hu_error_t err = hu_http_get(&a, "https://example.com/", NULL, &r);
    if (err != HU_OK) {
        hu_http_response_free(&a, &r);
        HU_SKIP_IF(1, "hu_http_get failed (offline or TLS)");
    }
    if (r.status_code != 200L) {
        hu_http_response_free(&a, &r);
        HU_SKIP_IF(1, "unexpected HTTP status from example.com");
    }
    HU_ASSERT_TRUE(r.body_len > 20);
    HU_ASSERT_NOT_NULL(r.body);
    HU_ASSERT_TRUE(hu__strcasestr(r.body, "Example") != NULL);
    hu_http_response_free(&a, &r);
}

static void integ_http_post_json_httpbin(void) {
    hu_allocator_t a = hu_system_allocator();
    hu_http_response_t r;
    memset(&r, 0, sizeof(r));
    const char *j = "{\"human\":42}";
    hu_error_t err = hu_http_post_json(&a, "https://httpbin.org/post", NULL, j, strlen(j), &r);
    if (err != HU_OK || r.status_code != 200L) {
        hu_http_response_free(&a, &r);
        HU_SKIP_IF(1, "httpbin.org unreachable");
    }
    HU_ASSERT_NOT_NULL(r.body);
    HU_ASSERT_STR_CONTAINS(r.body, "human");
    hu_http_response_free(&a, &r);
}
#else
static void integ_http_libcurl_disabled(void) {
    HU_SKIP_IF(1, "human_core built without HU_HTTP_CURL");
}
#endif

void run_integration_http_tests(void) {
#if defined(HU_HTTP_CURL)
    HU_RUN_TEST(integ_http_get_example_com);
    HU_RUN_TEST(integ_http_post_json_httpbin);
#else
    HU_RUN_TEST(integ_http_libcurl_disabled);
#endif
}
