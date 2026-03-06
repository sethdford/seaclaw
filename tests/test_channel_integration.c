/* Integration tests for Teams, Twilio, Google Chat channels.
 * Tests full lifecycle: create -> is_configured -> send -> name -> deinit.
 * Uses SC_IS_TEST (test build) which mocks network calls.
 */
#include "seaclaw/channel.h"
#include "seaclaw/channel_loop.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "test_framework.h"
#include <string.h>

#if SC_HAS_TEAMS
#include "seaclaw/channels/teams.h"
#endif
#if SC_HAS_TWILIO
#include "seaclaw/channels/twilio.h"
#endif
#if SC_HAS_GOOGLE_CHAT
#include "seaclaw/channels/google_chat.h"
#endif

#if SC_HAS_TEAMS
static void test_teams_integration_full_lifecycle(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_channel_t ch = {0};
    sc_error_t err = sc_teams_create(&alloc, "https://example.com/incoming", 27, &ch);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_TRUE(sc_teams_is_configured(&ch));
    SC_ASSERT_STR_EQ(ch.vtable->name(ch.ctx), "teams");
    err = ch.vtable->send(ch.ctx, NULL, 0, "test message", 12, NULL, 0);
    SC_ASSERT_EQ(err, SC_OK);
    sc_teams_destroy(&ch);
}

static void test_teams_integration_empty_config_not_configured(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_channel_t ch = {0};
    sc_error_t err = sc_teams_create(&alloc, NULL, 0, &ch);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_FALSE(sc_teams_is_configured(&ch));
    err = ch.vtable->send(ch.ctx, NULL, 0, "test", 4, NULL, 0);
    SC_ASSERT(err == SC_ERR_CHANNEL_NOT_CONFIGURED);
    sc_teams_destroy(&ch);
}
#endif

#if SC_HAS_TWILIO
static void test_twilio_integration_full_lifecycle(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_channel_t ch = {0};
    sc_error_t err = sc_twilio_create(&alloc, "ACXXXX", 6, "token", 5, "+15551234567", 12,
                                      "+15559876543", 12, &ch);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_TRUE(sc_twilio_is_configured(&ch));
    SC_ASSERT_STR_EQ(ch.vtable->name(ch.ctx), "twilio");
    err = ch.vtable->send(ch.ctx, NULL, 0, "test message", 12, NULL, 0);
    SC_ASSERT_EQ(err, SC_OK);
    sc_twilio_destroy(&ch);
}

static void test_twilio_integration_empty_config_not_configured(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_channel_t ch = {0};
    sc_error_t err = sc_twilio_create(&alloc, NULL, 0, NULL, 0, NULL, 0, NULL, 0, &ch);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_FALSE(sc_twilio_is_configured(&ch));
    err = ch.vtable->send(ch.ctx, NULL, 0, "test", 4, NULL, 0);
    SC_ASSERT(err == SC_ERR_CHANNEL_NOT_CONFIGURED);
    sc_twilio_destroy(&ch);
}
#endif

#if SC_HAS_GOOGLE_CHAT
static void test_google_chat_integration_full_lifecycle(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_channel_t ch = {0};
    sc_error_t err = sc_google_chat_create(
        &alloc, "https://chat.googleapis.com/v1/spaces/abc/webhooks", 45, &ch);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_TRUE(sc_google_chat_is_configured(&ch));
    SC_ASSERT_STR_EQ(ch.vtable->name(ch.ctx), "google_chat");
    err = ch.vtable->send(ch.ctx, NULL, 0, "test message", 12, NULL, 0);
    SC_ASSERT_EQ(err, SC_OK);
    sc_google_chat_destroy(&ch);
}

static void test_google_chat_integration_empty_config_not_configured(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_channel_t ch = {0};
    sc_error_t err = sc_google_chat_create(&alloc, NULL, 0, &ch);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_FALSE(sc_google_chat_is_configured(&ch));
    err = ch.vtable->send(ch.ctx, NULL, 0, "test", 4, NULL, 0);
    SC_ASSERT(err == SC_ERR_CHANNEL_NOT_CONFIGURED);
    sc_google_chat_destroy(&ch);
}
#endif

void run_channel_integration_tests(void) {
    SC_TEST_SUITE("Channel Integration");
#if SC_HAS_TEAMS
    SC_RUN_TEST(test_teams_integration_full_lifecycle);
    SC_RUN_TEST(test_teams_integration_empty_config_not_configured);
#endif
#if SC_HAS_TWILIO
    SC_RUN_TEST(test_twilio_integration_full_lifecycle);
    SC_RUN_TEST(test_twilio_integration_empty_config_not_configured);
#endif
#if SC_HAS_GOOGLE_CHAT
    SC_RUN_TEST(test_google_chat_integration_full_lifecycle);
    SC_RUN_TEST(test_google_chat_integration_empty_config_not_configured);
#endif
}
