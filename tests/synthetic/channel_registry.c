#include "channel_harness.h"
#include <string.h>

#if SC_HAS_IMESSAGE
#include "seaclaw/channels/imessage.h"

static sc_error_t imessage_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    return sc_imessage_create(alloc, "+15551234567", 12, NULL, 0, out);
}
static void imessage_test_destroy(sc_channel_t *ch) {
    sc_imessage_destroy(ch);
}
#endif

#if SC_HAS_TELEGRAM
#include "seaclaw/channels/telegram.h"

static sc_error_t telegram_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    return sc_telegram_create(alloc, "test-token", 10, out);
}
static void telegram_test_destroy(sc_channel_t *ch) {
    sc_telegram_destroy(ch);
}
#endif

#if SC_HAS_DISCORD
#include "seaclaw/channels/discord.h"

static sc_error_t discord_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    return sc_discord_create(alloc, "test-token", 10, out);
}
static void discord_test_destroy(sc_channel_t *ch) {
    sc_discord_destroy(ch);
}
#endif

#if SC_HAS_SLACK
#include "seaclaw/channels/slack.h"

static sc_error_t slack_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    return sc_slack_create(alloc, "test-token", 10, out);
}
static void slack_test_destroy(sc_channel_t *ch) {
    sc_slack_destroy(ch);
}
#endif

#if SC_HAS_SIGNAL
#include "seaclaw/channels/signal.h"

static sc_error_t signal_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    return sc_signal_create(alloc, "http://localhost:8080", 19, "test-account", 12, out);
}
static void signal_test_destroy(sc_channel_t *ch) {
    sc_signal_destroy(ch);
}
#endif

#if SC_HAS_WEB
#include "seaclaw/channels/web.h"

static sc_error_t web_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    return sc_web_create(alloc, out);
}
static void web_test_destroy(sc_channel_t *ch) {
    sc_web_destroy(ch);
}
#endif

#if SC_HAS_MQTT
#include "seaclaw/channels/mqtt.h"

static sc_error_t mqtt_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    return sc_mqtt_create(alloc, "mqtt://localhost:1883", 20, "in", 2, "out", 3, NULL, 0, NULL, 0,
                          0, out);
}
static void mqtt_test_destroy(sc_channel_t *ch) {
    sc_allocator_t a = sc_system_allocator();
    sc_mqtt_destroy(ch, &a);
}
#endif

#if SC_HAS_EMAIL
#include "seaclaw/channels/email.h"

static sc_error_t email_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    return sc_email_create(alloc, "localhost", 9, 587, "test@example.com", 16, out);
}
static void email_test_destroy(sc_channel_t *ch) {
    sc_email_destroy(ch);
}
static sc_error_t email_inject_wrapper(sc_channel_t *ch, const char *session_key, size_t sk_len,
                                       const char *content, size_t c_len) {
    return sc_email_test_inject_mock_email(ch, session_key, sk_len, content, c_len);
}
static const char *email_get_last_wrapper(sc_channel_t *ch, size_t *out_len) {
    const char *s = sc_email_test_last_message(ch);
    if (out_len && s)
        *out_len = strlen(s);
    return s;
}
#endif

#if SC_HAS_NOSTR
#include "seaclaw/channels/nostr.h"

static sc_error_t nostr_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    return sc_nostr_create(alloc, "/tmp/nak", 7, "npub1test", 9, "wss://relay.example.com", 22,
                           "seckey", 6, out);
}
static void nostr_test_destroy(sc_channel_t *ch) {
    sc_nostr_destroy(ch);
}
static const char *nostr_get_last_wrapper(sc_channel_t *ch, size_t *out_len) {
    const char *s = sc_nostr_test_last_message(ch);
    if (out_len && s)
        *out_len = strlen(s);
    return s;
}
#endif

#if SC_HAS_QQ
#include "seaclaw/channels/qq.h"

static sc_error_t qq_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    return sc_qq_create(alloc, "test-app-id", 11, "test-bot-token", 14, false, out);
}
static void qq_test_destroy(sc_channel_t *ch) {
    sc_qq_destroy(ch);
}
#endif

#if SC_HAS_DINGTALK
#include "seaclaw/channels/dingtalk.h"

static sc_error_t dingtalk_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    return sc_dingtalk_create(alloc, "test-app-key", 12, "test-app-secret", 16, out);
}
static void dingtalk_test_destroy(sc_channel_t *ch) {
    sc_dingtalk_destroy(ch);
}
#endif

#if SC_HAS_LARK
#include "seaclaw/channels/lark.h"

static sc_error_t lark_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    return sc_lark_create(alloc, "test-app-id", 11, "test-app-secret", 16, out);
}
static void lark_test_destroy(sc_channel_t *ch) {
    sc_lark_destroy(ch);
}
#endif

#if SC_HAS_MATTERMOST
#include "seaclaw/channels/mattermost.h"

static sc_error_t mattermost_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    return sc_mattermost_create(alloc, "http://localhost:8065", 20, "test-token", 10, out);
}
static void mattermost_test_destroy(sc_channel_t *ch) {
    sc_mattermost_destroy(ch);
}
#endif

#if SC_HAS_TEAMS
#include "seaclaw/channels/teams.h"

static sc_error_t teams_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    return sc_teams_create(alloc, "https://example.com/webhook", 26, out);
}
static void teams_test_destroy(sc_channel_t *ch) {
    sc_teams_destroy(ch);
}
#endif

#if SC_HAS_TWILIO
#include "seaclaw/channels/twilio.h"

static sc_error_t twilio_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    return sc_twilio_create(alloc, "ACtest", 6, "test-auth-token", 15, "+15551234567", 12,
                            "+15559876543", 12, out);
}
static void twilio_test_destroy(sc_channel_t *ch) {
    sc_twilio_destroy(ch);
}
#endif

#if SC_HAS_GOOGLE_CHAT
#include "seaclaw/channels/google_chat.h"

static sc_error_t google_chat_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    return sc_google_chat_create(alloc, "https://example.com/webhook", 27, out);
}
static void google_chat_test_destroy(sc_channel_t *ch) {
    sc_google_chat_destroy(ch);
}
#endif

#if SC_HAS_GOOGLE_RCS
#include "seaclaw/channels/google_rcs.h"

static sc_error_t google_rcs_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    return sc_google_rcs_create(alloc, "test-agent-id", 13, "test-token", 10, out);
}
static void google_rcs_test_destroy(sc_channel_t *ch) {
    sc_google_rcs_destroy(ch);
}
#endif

#if SC_HAS_GMAIL
#include "seaclaw/channels/gmail.h"

static sc_error_t gmail_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    return sc_gmail_create(alloc, "test-client-id", 14, "test-client-secret", 18,
                           "test-refresh-token", 17, 60, out);
}
static void gmail_test_destroy(sc_channel_t *ch) {
    sc_gmail_destroy(ch);
}
#endif

#if SC_HAS_LINE
#include "seaclaw/channels/line.h"

static sc_error_t line_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    return sc_line_create(alloc, "test-channel-token", 18, out);
}
static void line_test_destroy(sc_channel_t *ch) {
    sc_line_destroy(ch);
}
#endif

#if SC_HAS_ONEBOT
#include "seaclaw/channels/onebot.h"

static sc_error_t onebot_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    return sc_onebot_create(alloc, "http://localhost:5700", 19, "test-token", 10, out);
}
static void onebot_test_destroy(sc_channel_t *ch) {
    sc_onebot_destroy(ch);
}
#endif

#if SC_HAS_INSTAGRAM
#include "seaclaw/channels/instagram.h"

static sc_error_t instagram_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    return sc_instagram_create(alloc, "test-business-id", 15, "test-access-token", 17,
                               "test-app-secret", 15, out);
}
static void instagram_test_destroy(sc_channel_t *ch) {
    sc_instagram_destroy(ch);
}
#endif

#if SC_HAS_FACEBOOK
#include "seaclaw/channels/facebook.h"

static sc_error_t facebook_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    return sc_facebook_create(alloc, "test-page-id", 12, "test-token", 10, "test-secret", 11, out);
}
static void facebook_test_destroy(sc_channel_t *ch) {
    sc_facebook_destroy(ch);
}
#endif

#if SC_HAS_TWITTER
#include "seaclaw/channels/twitter.h"

static sc_error_t twitter_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    return sc_twitter_create(alloc, "test-bearer-token", 16, out);
}
static void twitter_test_destroy(sc_channel_t *ch) {
    sc_twitter_destroy(ch);
}
#endif

#if SC_HAS_MAIXCAM
#include "seaclaw/channels/maixcam.h"

static sc_error_t maixcam_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    return sc_maixcam_create(alloc, "localhost", 9, 8080, out);
}
static void maixcam_test_destroy(sc_channel_t *ch) {
    sc_maixcam_destroy(ch);
}
#endif

#if SC_HAS_MATRIX
#include "seaclaw/channels/matrix.h"

static sc_error_t matrix_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    return sc_matrix_create(alloc, "https://matrix.example.com", 25, "test-access-token", 17, out);
}
static void matrix_test_destroy(sc_channel_t *ch) {
    sc_matrix_destroy(ch);
}
#endif

#if SC_HAS_IRC
#include "seaclaw/channels/irc.h"

static sc_error_t irc_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    return sc_irc_create(alloc, "localhost", 9, 6667, out);
}
static void irc_test_destroy(sc_channel_t *ch) {
    sc_irc_destroy(ch);
}
#endif

#if SC_HAS_WHATSAPP
#include "seaclaw/channels/whatsapp.h"

static sc_error_t whatsapp_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    return sc_whatsapp_create(alloc, "test-phone-id", 12, "test-token", 10, out);
}
static void whatsapp_test_destroy(sc_channel_t *ch) {
    sc_whatsapp_destroy(ch);
}
#endif

#if SC_HAS_IMAP
#include "seaclaw/channels/imap.h"

static sc_error_t imap_test_create(sc_allocator_t *alloc, sc_channel_t *out) {
    sc_imap_config_t config = {0};
    config.imap_host = "localhost";
    config.imap_host_len = 9;
    config.imap_port = 143;
    config.imap_username = "test";
    config.imap_username_len = 4;
    config.imap_password = "test";
    config.imap_password_len = 4;
    config.imap_folder = "INBOX";
    config.imap_folder_len = 5;
    config.imap_use_tls = false;
    return sc_imap_create(alloc, &config, out);
}
static void imap_test_destroy(sc_channel_t *ch) {
    sc_imap_destroy(ch);
}
static sc_error_t imap_inject_wrapper(sc_channel_t *ch, const char *session_key, size_t sk_len,
                                      const char *content, size_t c_len) {
    return sc_imap_test_push_mock(ch, session_key, sk_len, content, c_len);
}
#endif

static const sc_channel_test_entry_t s_registry[] = {
#if SC_HAS_IMESSAGE
    {"imessage", imessage_test_create, imessage_test_destroy, sc_imessage_test_inject_mock,
     sc_imessage_poll, sc_imessage_test_get_last_message},
#endif
#if SC_HAS_TELEGRAM
    {"telegram", telegram_test_create, telegram_test_destroy, sc_telegram_test_inject_mock,
     sc_telegram_poll, sc_telegram_test_get_last_message},
#endif
#if SC_HAS_DISCORD
    {"discord", discord_test_create, discord_test_destroy, sc_discord_test_inject_mock,
     sc_discord_poll, sc_discord_test_get_last_message},
#endif
#if SC_HAS_SLACK
    {"slack", slack_test_create, slack_test_destroy, sc_slack_test_inject_mock, sc_slack_poll,
     sc_slack_test_get_last_message},
#endif
#if SC_HAS_SIGNAL
    {"signal", signal_test_create, signal_test_destroy, sc_signal_test_inject_mock, sc_signal_poll,
     sc_signal_test_get_last_message},
#endif
#if SC_HAS_WEB
    {"web", web_test_create, web_test_destroy, sc_web_test_inject_mock, NULL,
     sc_web_test_get_last_message},
#endif
#if SC_HAS_MQTT
    {"mqtt", mqtt_test_create, mqtt_test_destroy, sc_mqtt_test_inject_mock, sc_mqtt_poll,
     sc_mqtt_test_get_last_message},
#endif
#if SC_HAS_EMAIL
    {"email", email_test_create, email_test_destroy, email_inject_wrapper, sc_email_poll,
     email_get_last_wrapper},
#endif
#if SC_HAS_NOSTR
    {"nostr", nostr_test_create, nostr_test_destroy, sc_nostr_test_inject_mock_event, sc_nostr_poll,
     nostr_get_last_wrapper},
#endif
#if SC_HAS_QQ
    {"qq", qq_test_create, qq_test_destroy, sc_qq_test_inject_mock, sc_qq_poll,
     sc_qq_test_get_last_message},
#endif
#if SC_HAS_DINGTALK
    {"dingtalk", dingtalk_test_create, dingtalk_test_destroy, sc_dingtalk_test_inject_mock,
     sc_dingtalk_poll, sc_dingtalk_test_get_last_message},
#endif
#if SC_HAS_LARK
    {"lark", lark_test_create, lark_test_destroy, sc_lark_test_inject_mock, sc_lark_poll,
     sc_lark_test_get_last_message},
#endif
#if SC_HAS_MATTERMOST
    {"mattermost", mattermost_test_create, mattermost_test_destroy, sc_mattermost_test_inject_mock,
     sc_mattermost_poll, sc_mattermost_test_get_last_message},
#endif
#if SC_HAS_TEAMS
    {"teams", teams_test_create, teams_test_destroy, sc_teams_test_inject_mock, sc_teams_poll,
     sc_teams_test_get_last_message},
#endif
#if SC_HAS_TWILIO
    {"twilio", twilio_test_create, twilio_test_destroy, sc_twilio_test_inject_mock, sc_twilio_poll,
     sc_twilio_test_get_last_message},
#endif
#if SC_HAS_GOOGLE_CHAT
    {"google_chat", google_chat_test_create, google_chat_test_destroy,
     sc_google_chat_test_inject_mock, sc_google_chat_poll, sc_google_chat_test_get_last_message},
#endif
#if SC_HAS_GOOGLE_RCS
    {"google_rcs", google_rcs_test_create, google_rcs_test_destroy, sc_google_rcs_test_inject_mock,
     sc_google_rcs_poll, sc_google_rcs_test_get_last_message},
#endif
#if SC_HAS_GMAIL
    {"gmail", gmail_test_create, gmail_test_destroy, sc_gmail_test_inject_mock, sc_gmail_poll,
     sc_gmail_test_get_last_message},
#endif
#if SC_HAS_LINE
    {"line", line_test_create, line_test_destroy, sc_line_test_inject_mock, sc_line_poll,
     sc_line_test_get_last_message},
#endif
#if SC_HAS_ONEBOT
    {"onebot", onebot_test_create, onebot_test_destroy, sc_onebot_test_inject_mock, sc_onebot_poll,
     sc_onebot_test_get_last_message},
#endif
#if SC_HAS_INSTAGRAM
    {"instagram", instagram_test_create, instagram_test_destroy, sc_instagram_test_inject_mock,
     sc_instagram_poll, sc_instagram_test_get_last_message},
#endif
#if SC_HAS_FACEBOOK
    {"facebook", facebook_test_create, facebook_test_destroy, sc_facebook_test_inject_mock,
     sc_facebook_poll, sc_facebook_test_get_last_message},
#endif
#if SC_HAS_TWITTER
    {"twitter", twitter_test_create, twitter_test_destroy, sc_twitter_test_inject_mock,
     sc_twitter_poll, sc_twitter_test_get_last_message},
#endif
#if SC_HAS_MAIXCAM
    {"maixcam", maixcam_test_create, maixcam_test_destroy, sc_maixcam_test_inject_mock, NULL,
     sc_maixcam_test_get_last_message},
#endif
#if SC_HAS_MATRIX
    {"matrix", matrix_test_create, matrix_test_destroy, sc_matrix_test_inject_mock, sc_matrix_poll,
     sc_matrix_test_get_last_message},
#endif
#if SC_HAS_IRC
    {"irc", irc_test_create, irc_test_destroy, sc_irc_test_inject_mock, sc_irc_poll,
     sc_irc_test_get_last_message},
#endif
#if SC_HAS_WHATSAPP
    {"whatsapp", whatsapp_test_create, whatsapp_test_destroy, sc_whatsapp_test_inject_mock,
     sc_whatsapp_poll, sc_whatsapp_test_get_last_message},
#endif
#if SC_HAS_IMAP
    {"imap", imap_test_create, imap_test_destroy, imap_inject_wrapper, sc_imap_poll, NULL},
#endif
};

const sc_channel_test_entry_t *sc_channel_test_registry(size_t *count) {
    *count = sizeof(s_registry) / sizeof(s_registry[0]);
    return s_registry;
}

const sc_channel_test_entry_t *sc_channel_test_find(const char *name) {
    size_t n;
    const sc_channel_test_entry_t *r = sc_channel_test_registry(&n);
    for (size_t i = 0; i < n; i++)
        if (strcmp(r[i].name, name) == 0)
            return &r[i];
    return NULL;
}
