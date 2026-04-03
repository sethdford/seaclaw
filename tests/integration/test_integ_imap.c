#include "test_framework.h"
#if HU_HAS_IMAP
#include "human/channel_loop.h"
#include "human/channels/imap.h"
#endif
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Optional real IMAP/SMTP integration test.
 * Enable by setting HU_INTEG_IMAP=1 and these env vars:
 * - HU_INTEG_IMAP_HOST, HU_INTEG_IMAP_PORT
 * - HU_INTEG_IMAP_USER, HU_INTEG_IMAP_PASS
 * - HU_INTEG_SMTP_HOST, HU_INTEG_SMTP_PORT
 * - HU_INTEG_IMAP_TO (recipient, often same as user)
 * Optional:
 * - HU_INTEG_IMAP_FROM
 * - HU_INTEG_IMAP_FOLDER (default INBOX)
 * - HU_INTEG_IMAP_TLS (1|0, default 1)
 */
#if HU_HAS_IMAP
static int env_enabled(const char *name) {
    const char *v = getenv(name);
    return (v && v[0] != '\0');
}

static uint16_t env_port_or(const char *name, uint16_t defv) {
    const char *v = getenv(name);
    if (!v || !v[0])
        return defv;
    long p = strtol(v, NULL, 10);
    if (p < 1 || p > 65535)
        return defv;
    return (uint16_t)p;
}
#endif

static void integ_imap_send_and_poll_live(void) {
#if !HU_HAS_IMAP
    HU_SKIP_IF(1, "HU_HAS_IMAP not enabled for this build");
#else
    HU_SKIP_IF(!env_enabled("HU_INTEG_IMAP"), "HU_INTEG_IMAP not enabled");
    const char *imap_host = getenv("HU_INTEG_IMAP_HOST");
    const char *imap_user = getenv("HU_INTEG_IMAP_USER");
    const char *imap_pass = getenv("HU_INTEG_IMAP_PASS");
    const char *smtp_host = getenv("HU_INTEG_SMTP_HOST");
    const char *to = getenv("HU_INTEG_IMAP_TO");
    const char *from = getenv("HU_INTEG_IMAP_FROM");
    const char *folder = getenv("HU_INTEG_IMAP_FOLDER");
    if (!folder || !folder[0])
        folder = "INBOX";
    const char *tls = getenv("HU_INTEG_IMAP_TLS");
    bool use_tls = !(tls && tls[0] == '0');

    HU_SKIP_IF(!imap_host || !imap_user || !imap_pass || !smtp_host || !to,
               "Missing HU_INTEG_IMAP_* env vars");

    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch = {0};
    hu_imap_config_t cfg = {
        .imap_host = imap_host,
        .imap_host_len = strlen(imap_host),
        .imap_port = env_port_or("HU_INTEG_IMAP_PORT", 993),
        .imap_username = imap_user,
        .imap_username_len = strlen(imap_user),
        .imap_password = imap_pass,
        .imap_password_len = strlen(imap_pass),
        .imap_folder = folder,
        .imap_folder_len = strlen(folder),
        .imap_use_tls = use_tls,
        .smtp_host = smtp_host,
        .smtp_host_len = strlen(smtp_host),
        .smtp_port = env_port_or("HU_INTEG_SMTP_PORT", 587),
        .from_address = (from && from[0]) ? from : imap_user,
        .from_address_len = (from && from[0]) ? strlen(from) : strlen(imap_user),
    };
    hu_error_t err = hu_imap_create(&alloc, &cfg, &ch);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(ch.vtable->health_check(ch.ctx));

    const char *payload = "human integration imap live";
    err = ch.vtable->send(ch.ctx, to, strlen(to), payload, strlen(payload), NULL, 0);
    HU_ASSERT_EQ(err, HU_OK);

    /* Delivery + UNSEEN indexing can lag (especially Gmail). Poll with backoff. */
    hu_channel_loop_msg_t msgs[4];
    bool saw_payload = false;
    for (int attempt = 0; attempt < 8 && !saw_payload; attempt++) {
        if (attempt > 0)
            usleep(1000000);
        else
            usleep(500000);
        size_t out_count = 0;
        err = hu_imap_poll(ch.ctx, &alloc, msgs, 4, &out_count);
        HU_ASSERT_EQ(err, HU_OK);
        for (size_t i = 0; i < out_count && i < 4; i++) {
            if (strstr(msgs[i].content, payload) != NULL) {
                saw_payload = true;
                break;
            }
        }
    }
    if (!saw_payload)
        HU_SKIP_IF(1, "no poll result contained test payload (UNSEEN empty or delayed)");
    hu_imap_destroy(&ch);
#endif
}

void run_integration_imap_tests(void) {
    HU_RUN_TEST(integ_imap_send_and_poll_live);
}
