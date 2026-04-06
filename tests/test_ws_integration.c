#include "human/websocket/websocket.h"
#include "test_framework.h"
#include <stdlib.h>
#include <string.h>

#if defined(HU_GATEWAY_POSIX)
static void test_sec_websocket_accept_matches_rfc6455_vector(void) {
    /* RFC 6455 section 1.3 example handshake */
    HU_ASSERT_TRUE(hu_ws_sec_websocket_accept_valid("dGhlIHNhbXBsZSBub25jZQ==",
                                                    "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="));
    HU_ASSERT_FALSE(hu_ws_sec_websocket_accept_valid("dGhlIHNhbXBsZSBub25jZQ==", "bogus===="));
}
#endif

static void test_ws_upgrade_request_includes_auth_header(void) {
    char buf[2048];
    size_t n = hu_ws_build_upgrade_request(buf, sizeof(buf), "api.example.com", "/v1/ws", "dGVzdA==",
                                           "Authorization: Bearer sk-test\r\nX-Custom: foo\r\n");
    HU_ASSERT(n > 0);
    HU_ASSERT(strstr(buf, "Authorization: Bearer sk-test\r\n") != NULL);
    HU_ASSERT(strstr(buf, "X-Custom: foo\r\n") != NULL);
    HU_ASSERT(strstr(buf, "Upgrade: websocket\r\n") != NULL);
    HU_ASSERT(strstr(buf, "Sec-WebSocket-Key: dGVzdA==\r\n") != NULL);
    HU_ASSERT(strstr(buf, "GET /v1/ws HTTP/1.1\r\n") != NULL);
    HU_ASSERT(strstr(buf, "Host: api.example.com\r\n") != NULL);
}

static void test_ws_upgrade_request_null_extra_headers_omits_custom_lines(void) {
    char buf[1024];
    size_t n = hu_ws_build_upgrade_request(buf, sizeof(buf), "h.example", "/", "YWJjZGVmZ2hpamtsbW5vcA==",
                                           NULL);
    HU_ASSERT(n > 0);
    HU_ASSERT(strstr(buf, "Sec-WebSocket-Version: 13\r\n\r\n") != NULL);
}

static void test_ws_upgrade_request_empty_extra_same_as_null(void) {
    char b1[1024], b2[1024];
    size_t n1 = hu_ws_build_upgrade_request(b1, sizeof(b1), "x.test", "/path", "dGVzdA==", "");
    size_t n2 = hu_ws_build_upgrade_request(b2, sizeof(b2), "x.test", "/path", "dGVzdA==", NULL);
    HU_ASSERT_EQ(n1, n2);
    HU_ASSERT_STR_EQ(b1, b2);
}

static void test_ws_upgrade_request_sizing_matches_buffer_write(void) {
    size_t need = hu_ws_build_upgrade_request(NULL, 0, "host", "/p", "dGVzdA==", "H: v\r\n");
    HU_ASSERT(need > 0);
    char *small = (char *)malloc(need); /* one byte short of need+1 for null */
    HU_ASSERT_NOT_NULL(small);
    HU_ASSERT_EQ(hu_ws_build_upgrade_request(small, need, "host", "/p", "dGVzdA==", "H: v\r\n"), 0u);
    char *ok = (char *)malloc(need + 1);
    HU_ASSERT_NOT_NULL(ok);
    HU_ASSERT_EQ(hu_ws_build_upgrade_request(ok, need + 1, "host", "/p", "dGVzdA==", "H: v\r\n"), need);
    free(small);
    free(ok);
}

static void test_ws_upgrade_request_invalid_args_returns_zero(void) {
    char buf[64];
    HU_ASSERT_EQ(hu_ws_build_upgrade_request(buf, sizeof(buf), NULL, "/", "dGVzdA==", NULL), 0u);
    HU_ASSERT_EQ(hu_ws_build_upgrade_request(buf, sizeof(buf), "", "/p", "dGVzdA==", NULL), 0u);
    HU_ASSERT_EQ(hu_ws_build_upgrade_request(buf, sizeof(buf), "h", NULL, "dGVzdA==", NULL), 0u);
    HU_ASSERT_EQ(hu_ws_build_upgrade_request(buf, sizeof(buf), "h", "", "dGVzdA==", NULL), 0u);
    HU_ASSERT_EQ(hu_ws_build_upgrade_request(buf, sizeof(buf), "h", "/", NULL, NULL), 0u);
    HU_ASSERT_EQ(hu_ws_build_upgrade_request(buf, sizeof(buf), "h", "/", "", NULL), 0u);
}

void run_ws_integration_tests(void) {
    HU_TEST_SUITE("ws_integration");
    HU_RUN_TEST(test_ws_upgrade_request_includes_auth_header);
    HU_RUN_TEST(test_ws_upgrade_request_null_extra_headers_omits_custom_lines);
    HU_RUN_TEST(test_ws_upgrade_request_empty_extra_same_as_null);
    HU_RUN_TEST(test_ws_upgrade_request_sizing_matches_buffer_write);
    HU_RUN_TEST(test_ws_upgrade_request_invalid_args_returns_zero);
#if defined(HU_GATEWAY_POSIX)
    HU_RUN_TEST(test_sec_websocket_accept_matches_rfc6455_vector);
#endif
}
