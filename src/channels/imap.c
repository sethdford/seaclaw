/*
 * IMAP channel — poll inbox for email, send stores in outbox (v1).
 * Real IMAP/SMTP requires a library; v1 implements vtable + SC_IS_TEST mock path.
 */
#include "seaclaw/channels/imap.h"
#include "seaclaw/channel.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/core/string.h"
#include <stdlib.h>
#include <string.h>

#define SC_IMAP_OUTBOX_MAX      32
#define SC_IMAP_MOCK_QUEUE_MAX  16
#define SC_IMAP_SESSION_KEY_MAX 127
#define SC_IMAP_CONTENT_MAX     4095

typedef struct sc_imap_outbox_entry {
    char *target;
    char *message;
} sc_imap_outbox_entry_t;

#if SC_IS_TEST
typedef struct sc_imap_mock_msg {
    char session_key[SC_IMAP_SESSION_KEY_MAX + 1];
    char content[SC_IMAP_CONTENT_MAX + 1];
} sc_imap_mock_msg_t;
#endif

typedef struct sc_imap_ctx {
    sc_allocator_t *alloc;
    char *imap_host;
    size_t imap_host_len;
    uint16_t imap_port;
    char *imap_username;
    size_t imap_username_len;
    char *imap_password;
    size_t imap_password_len;
    char *imap_folder;
    size_t imap_folder_len;
    bool imap_use_tls;
    bool running;
    sc_imap_outbox_entry_t outbox[SC_IMAP_OUTBOX_MAX];
    size_t outbox_count;
#if SC_IS_TEST
    sc_imap_mock_msg_t mock_queue[SC_IMAP_MOCK_QUEUE_MAX];
    size_t mock_head;
    size_t mock_tail;
    size_t mock_count;
#endif
} sc_imap_ctx_t;

static sc_error_t imap_start(void *ctx) {
    sc_imap_ctx_t *c = (sc_imap_ctx_t *)ctx;
    if (!c)
        return SC_ERR_INVALID_ARGUMENT;
    c->running = true;
    return SC_OK;
}

static void imap_stop(void *ctx) {
    sc_imap_ctx_t *c = (sc_imap_ctx_t *)ctx;
    if (c)
        c->running = false;
}

static sc_error_t imap_send(void *ctx, const char *target, size_t target_len, const char *message,
                            size_t message_len, const char *const *media, size_t media_count) {
    (void)media;
    (void)media_count;
    sc_imap_ctx_t *c = (sc_imap_ctx_t *)ctx;
    if (!c || !c->alloc)
        return SC_ERR_INVALID_ARGUMENT;
    if (!target || target_len == 0 || !message)
        return SC_ERR_INVALID_ARGUMENT;
    if (c->outbox_count >= SC_IMAP_OUTBOX_MAX)
        return SC_ERR_OUT_OF_MEMORY;

#if SC_IS_TEST
    /* v1: store in outbox (log/store), no real SMTP */
    char *t = sc_strndup(c->alloc, target, target_len);
    char *m = sc_strndup(c->alloc, message, message_len);
    if (!t || !m) {
        if (t)
            c->alloc->free(c->alloc->ctx, t, target_len + 1);
        if (m)
            c->alloc->free(c->alloc->ctx, m, message_len + 1);
        return SC_ERR_OUT_OF_MEMORY;
    }
    c->outbox[c->outbox_count].target = t;
    c->outbox[c->outbox_count].message = m;
    c->outbox_count++;
    return SC_OK;
#else
    /* Non-test: would send via SMTP; for v1 just store in outbox */
    (void)target_len;
    (void)message_len;
    char *t = sc_strndup(c->alloc, target, target_len);
    char *m = sc_strndup(c->alloc, message, message_len);
    if (!t || !m) {
        if (t)
            c->alloc->free(c->alloc->ctx, t, target_len + 1);
        if (m)
            c->alloc->free(c->alloc->ctx, m, message_len + 1);
        return SC_ERR_OUT_OF_MEMORY;
    }
    c->outbox[c->outbox_count].target = t;
    c->outbox[c->outbox_count].message = m;
    c->outbox_count++;
    return SC_OK;
#endif
}

static const char *imap_name(void *ctx) {
    (void)ctx;
    return "imap";
}

static bool imap_health_check(void *ctx) {
    sc_imap_ctx_t *c = (sc_imap_ctx_t *)ctx;
    if (!c)
        return false;
    return (c->imap_host && c->imap_host[0] && c->imap_username && c->imap_username[0] &&
            c->imap_password && c->imap_password[0]);
}

static const sc_channel_vtable_t imap_vtable = {
    .start = imap_start,
    .stop = imap_stop,
    .send = imap_send,
    .name = imap_name,
    .health_check = imap_health_check,
    .send_event = NULL,
    .start_typing = NULL,
    .stop_typing = NULL,
};

sc_error_t sc_imap_create(sc_allocator_t *alloc, const sc_imap_config_t *config,
                          sc_channel_t *out) {
    if (!alloc || !out)
        return SC_ERR_INVALID_ARGUMENT;

    sc_imap_ctx_t *c = (sc_imap_ctx_t *)calloc(1, sizeof(*c));
    if (!c)
        return SC_ERR_OUT_OF_MEMORY;
    c->alloc = alloc;
    c->imap_port = config && config->imap_port > 0 ? config->imap_port : 993;
    c->imap_use_tls = config ? config->imap_use_tls : true;

    if (config && config->imap_host && config->imap_host_len > 0) {
        c->imap_host = (char *)malloc(config->imap_host_len + 1);
        if (!c->imap_host) {
            free(c);
            return SC_ERR_OUT_OF_MEMORY;
        }
        memcpy(c->imap_host, config->imap_host, config->imap_host_len);
        c->imap_host[config->imap_host_len] = '\0';
        c->imap_host_len = config->imap_host_len;
    }
    if (config && config->imap_username && config->imap_username_len > 0) {
        c->imap_username = (char *)malloc(config->imap_username_len + 1);
        if (!c->imap_username) {
            if (c->imap_host)
                free(c->imap_host);
            free(c);
            return SC_ERR_OUT_OF_MEMORY;
        }
        memcpy(c->imap_username, config->imap_username, config->imap_username_len);
        c->imap_username[config->imap_username_len] = '\0';
        c->imap_username_len = config->imap_username_len;
    }
    if (config && config->imap_password && config->imap_password_len > 0) {
        c->imap_password = (char *)malloc(config->imap_password_len + 1);
        if (!c->imap_password) {
            if (c->imap_host)
                free(c->imap_host);
            if (c->imap_username)
                free(c->imap_username);
            free(c);
            return SC_ERR_OUT_OF_MEMORY;
        }
        memcpy(c->imap_password, config->imap_password, config->imap_password_len);
        c->imap_password[config->imap_password_len] = '\0';
        c->imap_password_len = config->imap_password_len;
    }
    if (config && config->imap_folder && config->imap_folder_len > 0) {
        c->imap_folder = (char *)malloc(config->imap_folder_len + 1);
        if (!c->imap_folder) {
            if (c->imap_host)
                free(c->imap_host);
            if (c->imap_username)
                free(c->imap_username);
            if (c->imap_password)
                free(c->imap_password);
            free(c);
            return SC_ERR_OUT_OF_MEMORY;
        }
        memcpy(c->imap_folder, config->imap_folder, config->imap_folder_len);
        c->imap_folder[config->imap_folder_len] = '\0';
        c->imap_folder_len = config->imap_folder_len;
    } else {
        c->imap_folder = sc_strndup(c->alloc, "INBOX", 5);
        c->imap_folder_len = 5;
    }

    out->ctx = c;
    out->vtable = &imap_vtable;
    return SC_OK;
}

void sc_imap_destroy(sc_channel_t *ch) {
    if (!ch || !ch->ctx)
        return;
    sc_imap_ctx_t *c = (sc_imap_ctx_t *)ch->ctx;
    for (size_t i = 0; i < c->outbox_count; i++) {
        if (c->outbox[i].target)
            free(c->outbox[i].target);
        if (c->outbox[i].message)
            free(c->outbox[i].message);
    }
    if (c->imap_host)
        free(c->imap_host);
    if (c->imap_username)
        free(c->imap_username);
    if (c->imap_password)
        free(c->imap_password);
    if (c->imap_folder)
        free(c->imap_folder);
    free(c);
    ch->ctx = NULL;
    ch->vtable = NULL;
}

bool sc_imap_is_configured(sc_channel_t *ch) {
    if (!ch || !ch->ctx)
        return false;
    sc_imap_ctx_t *c = (sc_imap_ctx_t *)ch->ctx;
    return c->imap_host != NULL && c->imap_host[0] != '\0' && c->imap_username != NULL &&
           c->imap_username[0] != '\0' && c->imap_password != NULL && c->imap_password[0] != '\0';
}

sc_error_t sc_imap_poll(void *channel_ctx, sc_allocator_t *alloc, sc_channel_loop_msg_t *msgs,
                        size_t max_msgs, size_t *out_count) {
    if (!channel_ctx || !msgs || !out_count)
        return SC_ERR_INVALID_ARGUMENT;
    *out_count = 0;

#if SC_IS_TEST
    (void)alloc;
    sc_imap_ctx_t *c = (sc_imap_ctx_t *)channel_ctx;
    if (c->mock_count == 0)
        return SC_OK;
    size_t n = 0;
    while (n < max_msgs && c->mock_count > 0) {
        size_t idx = c->mock_head;
        size_t sk_len = strlen(c->mock_queue[idx].session_key);
        size_t ct_len = strlen(c->mock_queue[idx].content);
        if (sk_len >= sizeof(msgs[n].session_key))
            sk_len = sizeof(msgs[n].session_key) - 1;
        if (ct_len >= sizeof(msgs[n].content))
            ct_len = sizeof(msgs[n].content) - 1;
        memcpy(msgs[n].session_key, c->mock_queue[idx].session_key, sk_len);
        msgs[n].session_key[sk_len] = '\0';
        memcpy(msgs[n].content, c->mock_queue[idx].content, ct_len);
        msgs[n].content[ct_len] = '\0';
        c->mock_head = (c->mock_head + 1) % SC_IMAP_MOCK_QUEUE_MAX;
        c->mock_count--;
        n++;
    }
    *out_count = n;
    return SC_OK;
#else
    (void)alloc;
    (void)channel_ctx;
    (void)msgs;
    (void)max_msgs;
    /* Real IMAP requires a library; v1 returns empty */
    return SC_OK;
#endif
}

#if SC_IS_TEST
sc_error_t sc_imap_test_push_mock(sc_channel_t *ch, const char *session_key, size_t session_key_len,
                                  const char *content, size_t content_len) {
    if (!ch || !ch->ctx)
        return SC_ERR_INVALID_ARGUMENT;
    sc_imap_ctx_t *c = (sc_imap_ctx_t *)ch->ctx;
    if (c->mock_count >= SC_IMAP_MOCK_QUEUE_MAX)
        return SC_ERR_OUT_OF_MEMORY;
    size_t idx = c->mock_tail;
    size_t sk_copy =
        session_key_len > SC_IMAP_SESSION_KEY_MAX ? SC_IMAP_SESSION_KEY_MAX : session_key_len;
    size_t ct_copy = content_len > SC_IMAP_CONTENT_MAX ? SC_IMAP_CONTENT_MAX : content_len;
    memcpy(c->mock_queue[idx].session_key, session_key, sk_copy);
    c->mock_queue[idx].session_key[sk_copy] = '\0';
    memcpy(c->mock_queue[idx].content, content, ct_copy);
    c->mock_queue[idx].content[ct_copy] = '\0';
    c->mock_tail = (c->mock_tail + 1) % SC_IMAP_MOCK_QUEUE_MAX;
    c->mock_count++;
    return SC_OK;
}
#endif
