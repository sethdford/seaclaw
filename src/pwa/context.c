/*
 * PWA Context — build cross-app context string from all open PWA tabs.
 * Used to inject context from other PWAs into the agent's system prompt.
 */
#include "human/pwa_context.h"
#include "human/pwa.h"
#include <stdio.h>
#include <string.h>

#define PWA_CONTEXT_SUMMARY_MAX 200

/* Built-in app names (same order as learner/main). */
static const char *const PWA_CONTEXT_APPS[] = {
    "slack", "discord", "whatsapp", "gmail", "calendar",
    "notion", "twitter", "telegram", "linkedin", "facebook",
};
#define PWA_CONTEXT_APP_COUNT (sizeof(PWA_CONTEXT_APPS) / sizeof(PWA_CONTEXT_APPS[0]))

#if HU_IS_TEST
static const char PWA_CONTEXT_TEST_STRING[] =
    "[Calendar] Test event today\n"
    "[Slack] Test: hello from alice\n"
    "\n--- Entities ---\n"
    "Emails: alice@example.com\n"
    "\n--- Alerts ---\n"
    "[NORMAL] slack: New messages\n";
#endif

hu_error_t hu_pwa_context_build(hu_allocator_t *alloc, char **out, size_t *out_len) {
    if (!alloc || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

    *out = NULL;
    *out_len = 0;

#if HU_IS_TEST
    size_t len = sizeof(PWA_CONTEXT_TEST_STRING) - 1;
    char *result = (char *)alloc->alloc(alloc->ctx, len + 1);
    if (!result)
        return HU_ERR_OUT_OF_MEMORY;
    memcpy(result, PWA_CONTEXT_TEST_STRING, len + 1);
    *out = result;
    *out_len = len;
    return HU_OK;
#else
    hu_pwa_browser_t browser;
    hu_error_t err = hu_pwa_detect_browser(&browser);
    if (err != HU_OK) {
        /* No browser — return empty string */
        char *empty = (char *)alloc->alloc(alloc->ctx, 1);
        if (!empty)
            return HU_ERR_OUT_OF_MEMORY;
        empty[0] = '\0';
        *out = empty;
        *out_len = 0;
        return HU_OK;
    }

    char *buf = (char *)alloc->alloc(alloc->ctx, HU_PWA_CONTEXT_MAX_LEN + 1);
    if (!buf)
        return HU_ERR_OUT_OF_MEMORY;
    size_t used = 0;
    buf[0] = '\0';

    for (size_t i = 0; i < PWA_CONTEXT_APP_COUNT; i++) {
        const char *app_name = PWA_CONTEXT_APPS[i];
        const hu_pwa_driver_t *drv = hu_pwa_driver_resolve(app_name);
        if (!drv || !drv->read_messages_js)
            continue;

        char *content = NULL;
        size_t content_len = 0;
        err = hu_pwa_read_messages(alloc, browser, app_name, &content, &content_len);
        if (err != HU_OK || !content || content_len == 0) {
            if (content)
                alloc->free(alloc->ctx, content, content_len + 1);
            continue;
        }

        const char *display = drv->display_name ? drv->display_name : app_name;
        char prefix[64];
        int n = snprintf(prefix, sizeof(prefix), "[%s] ", display);
        if (n <= 0 || (size_t)n >= sizeof(prefix)) {
            alloc->free(alloc->ctx, content, content_len + 1);
            continue;
        }

        size_t orig_len = content_len;
        size_t summary_len = content_len;
        if (summary_len > PWA_CONTEXT_SUMMARY_MAX)
            summary_len = PWA_CONTEXT_SUMMARY_MAX;
        for (size_t j = 0; j < summary_len; j++) {
            if (content[j] == '\n')
                content[j] = ' ';
        }
        content[summary_len] = '\0';

        size_t line_len = (size_t)n + summary_len + 2; /* prefix + content + "\n\0" */
        if (used + line_len > HU_PWA_CONTEXT_MAX_LEN) {
            alloc->free(alloc->ctx, content, orig_len + 1);
            break;
        }

        memcpy(buf + used, prefix, (size_t)n);
        used += (size_t)n;
        memcpy(buf + used, content, summary_len + 1);
        used += summary_len;
        buf[used++] = '\n';
        buf[used] = '\0';

        alloc->free(alloc->ctx, content, orig_len + 1);
    }

    *out = buf;
    *out_len = used;
    return HU_OK;
#endif
}
