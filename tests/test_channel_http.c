#include "test_framework.h"
#include "channel_http.h"
#include <string.h>

/* ── hu_channel_http_build_auth ──────────────────────────────────── */

static void channel_http_build_auth_basic(void) {
    char buf[128];
    int n = hu_channel_http_build_auth(buf, sizeof(buf), "Bearer", "tok123", 6);
    HU_ASSERT(n > 0);
    HU_ASSERT(strcmp(buf, "Authorization: Bearer tok123") == 0);
}

static void channel_http_build_auth_null_buf_fails(void) {
    HU_ASSERT(hu_channel_http_build_auth(NULL, 128, "Bearer", "tok", 3) == -1);
}

static void channel_http_build_auth_null_scheme_fails(void) {
    char buf[128];
    HU_ASSERT(hu_channel_http_build_auth(buf, sizeof(buf), NULL, "tok", 3) == -1);
}

static void channel_http_build_auth_null_token_fails(void) {
    char buf[128];
    HU_ASSERT(hu_channel_http_build_auth(buf, sizeof(buf), "Bearer", NULL, 0) == -1);
}

static void channel_http_build_auth_buffer_too_small(void) {
    char buf[5];
    HU_ASSERT(hu_channel_http_build_auth(buf, sizeof(buf), "Bearer", "tok123", 6) == -1);
}

static void channel_http_build_auth_zero_buf_fails(void) {
    char buf[128];
    HU_ASSERT(hu_channel_http_build_auth(buf, 0, "Bearer", "tok", 3) == -1);
}

static void channel_http_build_auth_bot_scheme(void) {
    char buf[128];
    int n = hu_channel_http_build_auth(buf, sizeof(buf), "Bot", "abc.def", 7);
    HU_ASSERT(n > 0);
    HU_ASSERT(strcmp(buf, "Authorization: Bot abc.def") == 0);
}

/* ── hu_channel_http_build_url ───────────────────────────────────── */

static void channel_http_build_url_basic(void) {
    char buf[256];
    int n = hu_channel_http_build_url(buf, sizeof(buf),
                                      "https://api.example.com", "/v1/send");
    HU_ASSERT(n > 0);
    HU_ASSERT(strcmp(buf, "https://api.example.com/v1/send") == 0);
}

static void channel_http_build_url_with_format(void) {
    char buf[256];
    int n = hu_channel_http_build_url(buf, sizeof(buf),
                                      "https://api.telegram.org/bot", "%s/sendMessage", "TOKEN");
    HU_ASSERT(n > 0);
    HU_ASSERT(strcmp(buf, "https://api.telegram.org/botTOKEN/sendMessage") == 0);
}

static void channel_http_build_url_null_buf_fails(void) {
    HU_ASSERT(hu_channel_http_build_url(NULL, 256, "https://a.com", "/x") == -1);
}

static void channel_http_build_url_null_base_fails(void) {
    char buf[256];
    HU_ASSERT(hu_channel_http_build_url(buf, sizeof(buf), NULL, "/x") == -1);
}

static void channel_http_build_url_null_path_fails(void) {
    char buf[256];
    HU_ASSERT(hu_channel_http_build_url(buf, sizeof(buf), "https://a.com", NULL) == -1);
}

static void channel_http_build_url_buffer_too_small(void) {
    char buf[10];
    HU_ASSERT(hu_channel_http_build_url(buf, sizeof(buf),
                                         "https://api.example.com", "/v1/send") == -1);
}

static void channel_http_build_url_zero_buf_fails(void) {
    char buf[256];
    HU_ASSERT(hu_channel_http_build_url(buf, 0, "https://a.com", "/x") == -1);
}

/* ── Suite registration ──────────────────────────────────────────── */

void run_channel_http_tests(void) {
    HU_TEST_SUITE("channel_http");

    HU_RUN_TEST(channel_http_build_auth_basic);
    HU_RUN_TEST(channel_http_build_auth_null_buf_fails);
    HU_RUN_TEST(channel_http_build_auth_null_scheme_fails);
    HU_RUN_TEST(channel_http_build_auth_null_token_fails);
    HU_RUN_TEST(channel_http_build_auth_buffer_too_small);
    HU_RUN_TEST(channel_http_build_auth_zero_buf_fails);
    HU_RUN_TEST(channel_http_build_auth_bot_scheme);

    HU_RUN_TEST(channel_http_build_url_basic);
    HU_RUN_TEST(channel_http_build_url_with_format);
    HU_RUN_TEST(channel_http_build_url_null_buf_fails);
    HU_RUN_TEST(channel_http_build_url_null_base_fails);
    HU_RUN_TEST(channel_http_build_url_null_path_fails);
    HU_RUN_TEST(channel_http_build_url_buffer_too_small);
    HU_RUN_TEST(channel_http_build_url_zero_buf_fails);
}
