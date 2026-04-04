#include "human/core/allocator.h"
#include "human/mcp_transport.h"
#include "test_framework.h"
#include <stddef.h>
#include <string.h>

static void mcp_http_create_returns_not_supported_without_curl(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *url = "https://example.com/mcp";
    size_t url_len = strlen(url);
    hu_mcp_transport_t t;

#ifdef HU_HTTP_CURL
    HU_ASSERT_EQ(hu_mcp_transport_http_create(&alloc, url, url_len, &t), HU_OK);
    HU_ASSERT_NOT_NULL(t.ctx);
    HU_ASSERT_NOT_NULL(t.close);
    hu_mcp_transport_destroy(&t, &alloc);
    HU_ASSERT_NULL(t.ctx);
#else
    HU_ASSERT_EQ(hu_mcp_transport_http_create(&alloc, url, url_len, &t), HU_ERR_NOT_SUPPORTED);
#endif
}

static void mcp_http_create_null_alloc_returns_error(void) {
    hu_mcp_transport_t t;
    memset(&t, 0, sizeof(t));
    const char *url = "https://example.com/mcp";

    HU_ASSERT_EQ(hu_mcp_transport_http_create(NULL, url, strlen(url), &t), HU_ERR_INVALID_ARGUMENT);
}

static void mcp_http_create_null_url_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_transport_t t;
    memset(&t, 0, sizeof(t));

    HU_ASSERT_EQ(hu_mcp_transport_http_create(&alloc, NULL, 10, &t), HU_ERR_INVALID_ARGUMENT);
}

static void mcp_http_create_empty_url_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    static const char url[] = "https://example.com/";
    hu_mcp_transport_t t;
    memset(&t, 0, sizeof(t));

    HU_ASSERT_EQ(hu_mcp_transport_http_create(&alloc, url, 0, &t), HU_ERR_INVALID_ARGUMENT);
}

static void mcp_transport_struct_zero_init_is_safe(void) {
    hu_mcp_transport_t t;
    memset(&t, 0, sizeof(t));

    HU_ASSERT_NULL(t.ctx);
    HU_ASSERT(t.send == NULL);
    HU_ASSERT(t.recv == NULL);
    HU_ASSERT(t.close == NULL);
}

void run_mcp_http_integration_tests(void) {
    HU_TEST_SUITE("mcp_http_integration");
    HU_RUN_TEST(mcp_http_create_returns_not_supported_without_curl);
    HU_RUN_TEST(mcp_http_create_null_alloc_returns_error);
    HU_RUN_TEST(mcp_http_create_null_url_returns_error);
    HU_RUN_TEST(mcp_http_create_empty_url_returns_error);
    HU_RUN_TEST(mcp_transport_struct_zero_init_is_safe);
}
