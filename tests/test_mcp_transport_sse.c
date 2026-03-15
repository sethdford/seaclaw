/* Tests for MCP SSE transport — null args, NOT_SUPPORTED when curl disabled. */
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/mcp_transport.h"
#include "test_framework.h"
#include <string.h>

static void mcp_sse_transport_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_transport_t t = {0};
    HU_ASSERT_NEQ(hu_mcp_transport_sse_create(NULL, "http://localhost/sse", 20, &t), HU_OK);
    HU_ASSERT_NEQ(hu_mcp_transport_sse_create(&alloc, NULL, 0, &t), HU_OK);
    HU_ASSERT_NEQ(hu_mcp_transport_sse_create(&alloc, "http://localhost/sse", 20, NULL), HU_OK);
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

void run_mcp_transport_sse_tests(void) {
    HU_TEST_SUITE("MCP Transport SSE");
    HU_RUN_TEST(mcp_sse_transport_null_args);
    HU_RUN_TEST(mcp_sse_create_without_curl);
}
