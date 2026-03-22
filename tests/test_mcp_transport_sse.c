/* Tests for MCP transports — SSE, HTTP, stdio: validation, curl gating, null safety. */
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/mcp_transport.h"
#include "test_framework.h"
#include <string.h>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

static void mcp_sse_transport_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_transport_t t = {0};
    HU_ASSERT_NEQ(hu_mcp_transport_sse_create(NULL, "http://localhost/sse", 20, &t), HU_OK);
    HU_ASSERT_NEQ(hu_mcp_transport_sse_create(&alloc, NULL, 0, &t), HU_OK);
    HU_ASSERT_NEQ(hu_mcp_transport_sse_create(&alloc, "http://localhost/sse", 20, NULL), HU_OK);
}

static void mcp_sse_empty_url_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_transport_t t = {0};
    HU_ASSERT_EQ(hu_mcp_transport_sse_create(&alloc, "", 0, &t), HU_ERR_INVALID_ARGUMENT);
}

static void mcp_sse_create_without_curl(void) {
#ifdef HU_HTTP_CURL
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_transport_t t = {0};
    hu_error_t err = hu_mcp_transport_sse_create(&alloc, "http://localhost/sse", 20, &t);
    HU_ASSERT_EQ(err, HU_OK);
    if (t.ctx && t.close)
        t.close(t.ctx, &alloc);
#else
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_transport_t t = {0};
    hu_error_t err = hu_mcp_transport_sse_create(&alloc, "http://localhost/sse", 20, &t);
    HU_ASSERT_EQ(err, HU_ERR_NOT_SUPPORTED);
    HU_ASSERT_NULL(t.ctx);
#endif
}

static void mcp_http_transport_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_transport_t t = {0};
    HU_ASSERT_NEQ(hu_mcp_transport_http_create(NULL, "http://localhost/mcp", 20, &t), HU_OK);
    HU_ASSERT_NEQ(hu_mcp_transport_http_create(&alloc, NULL, 0, &t), HU_OK);
    HU_ASSERT_NEQ(hu_mcp_transport_http_create(&alloc, "http://localhost/mcp", 20, NULL), HU_OK);
}

static void mcp_http_create_without_curl(void) {
#ifdef HU_HTTP_CURL
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_transport_t t = {0};
    hu_error_t err = hu_mcp_transport_http_create(&alloc, "http://localhost/mcp", 20, &t);
    HU_ASSERT_EQ(err, HU_OK);
    if (t.ctx && t.close)
        t.close(t.ctx, &alloc);
#else
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_transport_t t = {0};
    hu_error_t err = hu_mcp_transport_http_create(&alloc, "http://localhost/mcp", 20, &t);
    HU_ASSERT_EQ(err, HU_ERR_NOT_SUPPORTED);
    HU_ASSERT_NULL(t.ctx);
#endif
}

static void mcp_stdio_transport_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_transport_t t = {0};
    HU_ASSERT_NEQ(hu_mcp_transport_stdio_create(NULL, 0, 1, &t), HU_OK);
    HU_ASSERT_NEQ(hu_mcp_transport_stdio_create(&alloc, 0, 1, NULL), HU_OK);
}

static void mcp_stdio_create_valid_fds(void) {
#if defined(__unix__) || defined(__APPLE__)
    /* dup stdin/stdout so destroy() closes copies, not the test process 0/1. */
    hu_allocator_t alloc = hu_system_allocator();
    int rd = dup(STDIN_FILENO);
    int wr = dup(STDOUT_FILENO);
    HU_ASSERT_TRUE(rd >= 0);
    HU_ASSERT_TRUE(wr >= 0);
    hu_mcp_transport_t t = {0};
    HU_ASSERT_EQ(hu_mcp_transport_stdio_create(&alloc, rd, wr, &t), HU_OK);
    HU_ASSERT_NOT_NULL(t.ctx);
    HU_ASSERT_NOT_NULL(t.send);
    HU_ASSERT_NOT_NULL(t.recv);
    HU_ASSERT_NOT_NULL(t.close);
    hu_mcp_transport_destroy(&t, &alloc);
#else
    (void)0;
#endif
}

static void mcp_transport_destroy_null_safe(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_transport_destroy(NULL, &alloc);
}

void run_mcp_transport_sse_tests(void) {
    HU_TEST_SUITE("MCP Transport");
    HU_RUN_TEST(mcp_sse_transport_null_args);
    HU_RUN_TEST(mcp_sse_empty_url_returns_error);
    HU_RUN_TEST(mcp_sse_create_without_curl);
    HU_RUN_TEST(mcp_http_transport_null_args);
    HU_RUN_TEST(mcp_http_create_without_curl);
    HU_RUN_TEST(mcp_stdio_transport_null_args);
    HU_RUN_TEST(mcp_stdio_create_valid_fds);
    HU_RUN_TEST(mcp_transport_destroy_null_safe);
}
