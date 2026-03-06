#if SC_HAS_IMESSAGE
#include "seaclaw/channels/imessage.h"
#include "seaclaw/channel.h"
#include "seaclaw/channel_loop.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "test_framework.h"
#include <string.h>

#if (defined(__APPLE__) && defined(__MACH__)) || SC_IS_TEST
extern size_t escape_for_applescript(char *out, size_t out_cap, const char *in, size_t in_len);
#endif

#if (defined(__APPLE__) && defined(__MACH__)) || SC_IS_TEST
static void test_imessage_escape_basic(void) {
    char out[256];
    size_t len = escape_for_applescript(out, sizeof(out), "hello", 5);
    SC_ASSERT_EQ(len, 5u);
    SC_ASSERT_STR_EQ(out, "hello");
}

static void test_imessage_escape_quotes(void) {
    char out[256];
    size_t len = escape_for_applescript(out, sizeof(out), "he said \"hi\"", 12);
    SC_ASSERT_TRUE(len > 12); /* escaped quotes should be longer */
    SC_ASSERT_TRUE(strstr(out, "\\\"") != NULL);
}

static void test_imessage_escape_backslash(void) {
    char out[256];
    size_t len = escape_for_applescript(out, sizeof(out), "path\\file", 9);
    SC_ASSERT_TRUE(len > 9);
    SC_ASSERT_TRUE(strstr(out, "\\\\") != NULL);
}

static void test_imessage_escape_empty(void) {
    char out[16];
    size_t len = escape_for_applescript(out, sizeof(out), "", 0);
    SC_ASSERT_EQ(len, 0u);
    SC_ASSERT_STR_EQ(out, "");
}

static void test_imessage_escape_truncation(void) {
    char out[8]; /* small buffer */
    size_t len = escape_for_applescript(out, sizeof(out), "hello world this is a long string", 33);
    SC_ASSERT_TRUE(len < sizeof(out));
    SC_ASSERT_TRUE(out[len] == '\0');
}
#endif

static void test_imessage_create_with_allow_from(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_channel_t ch;
    const char *allow_from[] = {"+15551234567", "user@example.com"};
    sc_error_t err = sc_imessage_create(&alloc, "+15559876543", 11, allow_from, 2, &ch);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(ch.ctx);
    SC_ASSERT_STR_EQ(ch.vtable->name(ch.ctx), "imessage");
    sc_imessage_destroy(&ch);
}

static void test_imessage_create_null_alloc(void) {
    sc_channel_t ch;
    sc_error_t err = sc_imessage_create(NULL, "+15551234567", 11, NULL, 0, &ch);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
}

static void test_imessage_health_check(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_channel_t ch;
    sc_imessage_create(&alloc, "+15551234567", 11, NULL, 0, &ch);
    (void)ch.vtable->health_check(ch.ctx); /* platform-dependent: true on macOS with db, false otherwise */
    sc_imessage_destroy(&ch);
}

static void test_imessage_send_test_mode(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_channel_t ch;
    sc_imessage_create(&alloc, "+15551234567", 11, NULL, 0, &ch);
    sc_error_t err = ch.vtable->send(ch.ctx, "+15551234567", 11, "test message", 12, NULL, 0);
    SC_ASSERT_EQ(err, SC_OK);
    sc_imessage_destroy(&ch);
}

static void test_imessage_poll_test_mode(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_channel_t ch;
    sc_imessage_create(&alloc, "+15551234567", 11, NULL, 0, &ch);
    sc_channel_loop_msg_t msgs[4];
    size_t out_count = 99;
    sc_error_t err = sc_imessage_poll(ch.ctx, &alloc, msgs, 4, &out_count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(out_count, 0u);
    sc_imessage_destroy(&ch);
}

void run_imessage_extended_tests(void) {
    SC_TEST_SUITE("iMessage Extended");

#if (defined(__APPLE__) && defined(__MACH__)) || SC_IS_TEST
    SC_RUN_TEST(test_imessage_escape_basic);
    SC_RUN_TEST(test_imessage_escape_quotes);
    SC_RUN_TEST(test_imessage_escape_backslash);
    SC_RUN_TEST(test_imessage_escape_empty);
    SC_RUN_TEST(test_imessage_escape_truncation);
#endif
    SC_RUN_TEST(test_imessage_create_with_allow_from);
    SC_RUN_TEST(test_imessage_create_null_alloc);
    SC_RUN_TEST(test_imessage_health_check);
    SC_RUN_TEST(test_imessage_send_test_mode);
    SC_RUN_TEST(test_imessage_poll_test_mode);
}
#else
void run_imessage_extended_tests(void) {
    (void)0; /* iMessage channel not built */
}
#endif
