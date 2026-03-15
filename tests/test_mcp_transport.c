#include "human/core/allocator.h"
#include "human/mcp_transport.h"
#include "test_framework.h"
#include <string.h>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

static void stdio_roundtrip(void) {
#if defined(__unix__) || defined(__APPLE__)
    hu_allocator_t alloc = hu_system_allocator();
    int fds[2];
    HU_ASSERT_EQ(pipe(fds), 0);
    hu_mcp_transport_t t = {0};
    HU_ASSERT_EQ(hu_mcp_transport_stdio_create(&alloc, fds[0], fds[1], &t), HU_OK);
    const char *msg = "hello";
    HU_ASSERT_EQ(t.send(t.ctx, msg, strlen(msg)), HU_OK);
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(t.recv(t.ctx, &alloc, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_EQ(out_len, (size_t)5);
    HU_ASSERT_TRUE(memcmp(out, msg, 5) == 0);
    alloc.free(alloc.ctx, out, out_len + 1);
    hu_mcp_transport_destroy(&t, &alloc);
    close(fds[0]);
    close(fds[1]);
#else
    (void)0;
#endif
}

static void null_safety(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_transport_destroy(NULL, &alloc);
    hu_mcp_transport_destroy(&(hu_mcp_transport_t){0}, NULL);
}

static void invalid_fd(void) {
#if defined(__unix__) || defined(__APPLE__)
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_transport_t t = {0};
    HU_ASSERT_EQ(hu_mcp_transport_stdio_create(&alloc, -1, -1, &t), HU_OK);
    hu_error_t err = t.send(t.ctx, "x", 1);
    (void)err;
    hu_mcp_transport_destroy(&t, &alloc);
#else
    (void)0;
#endif
}

static void http_create_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_transport_t t = {0};
    HU_ASSERT_NEQ(hu_mcp_transport_http_create(NULL, "http://localhost", 16, &t), HU_OK);
    HU_ASSERT_NEQ(hu_mcp_transport_http_create(&alloc, NULL, 0, &t), HU_OK);
    HU_ASSERT_NEQ(hu_mcp_transport_http_create(&alloc, "http://localhost", 16, NULL), HU_OK);
}

static void http_create_and_destroy(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_transport_t t = {0};
    hu_error_t err = hu_mcp_transport_http_create(&alloc, "http://localhost:9999/mcp", 24, &t);
#ifdef HU_HTTP_CURL
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(t.ctx);
    HU_ASSERT_NOT_NULL(t.send);
    HU_ASSERT_NOT_NULL(t.recv);
    HU_ASSERT_NOT_NULL(t.close);
    hu_mcp_transport_destroy(&t, &alloc);
#else
    HU_ASSERT_EQ(err, HU_ERR_NOT_SUPPORTED);
#endif
}

static void http_recv_without_send(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mcp_transport_t t = {0};
    hu_error_t err = hu_mcp_transport_http_create(&alloc, "http://localhost:9999", 20, &t);
#ifdef HU_HTTP_CURL
    HU_ASSERT_EQ(err, HU_OK);
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(t.recv(t.ctx, &alloc, &out, &out_len), HU_OK);
    HU_ASSERT_NULL(out);
    HU_ASSERT_EQ(out_len, 0u);
    hu_mcp_transport_destroy(&t, &alloc);
#else
    HU_ASSERT_EQ(err, HU_ERR_NOT_SUPPORTED);
#endif
}

void run_mcp_transport_tests(void) {
    HU_TEST_SUITE("MCP Transport");
    HU_RUN_TEST(stdio_roundtrip);
    HU_RUN_TEST(null_safety);
    HU_RUN_TEST(invalid_fd);
    HU_RUN_TEST(http_create_null_args);
    HU_RUN_TEST(http_create_and_destroy);
    HU_RUN_TEST(http_recv_without_send);
}
