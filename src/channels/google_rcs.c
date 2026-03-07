/*
 * Google RCS Business Messaging channel.
 */
#include "seaclaw/channels/google_rcs.h"
#include "seaclaw/channel.h"
#include "seaclaw/channel_loop.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/core/http.h"
#include "seaclaw/core/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SC_RCS_API_BASE         "https://rcsbusinessmessaging.googleapis.com/v1/phones/"
#define SC_RCS_AGENT_MSG_SUFFIX "/agentMessages"
#define SC_RCS_QUEUE_MAX        32
#define SC_RCS_SESSION_KEY_MAX  127
#define SC_RCS_CONTENT_MAX      4095

typedef struct sc_google_rcs_queued_msg {
    char session_key[128];
    char content[4096];
} sc_google_rcs_queued_msg_t;

typedef struct sc_google_rcs_ctx {
    sc_allocator_t *alloc;
    char *agent_id;
    size_t agent_id_len;
    char *token;
    size_t token_len;
    bool running;
    sc_google_rcs_queued_msg_t queue[SC_RCS_QUEUE_MAX];
    size_t queue_head;
    size_t queue_tail;
    size_t queue_count;
#if SC_IS_TEST
    char last_message[4096];
    size_t last_message_len;
    struct {
        char session_key[128];
        char content[4096];
    } mock_msgs[8];
    size_t mock_count;
#endif
} sc_google_rcs_ctx_t;

static sc_error_t google_rcs_start(void *ctx) {
    sc_google_rcs_ctx_t *c = (sc_google_rcs_ctx_t *)ctx;
    if (!c)
        return SC_ERR_INVALID_ARGUMENT;
    c->running = true;
    return SC_OK;
}

static void google_rcs_stop(void *ctx) {
    sc_google_rcs_ctx_t *c = (sc_google_rcs_ctx_t *)ctx;
    if (c)
        c->running = false;
}

static sc_error_t google_rcs_send(void *ctx, const char *target, size_t target_len,
                                  const char *message, size_t message_len, const char *const *media,
                                  size_t media_count) {
    sc_google_rcs_ctx_t *c = (sc_google_rcs_ctx_t *)ctx;

#if SC_IS_TEST
    {
        size_t len = message_len > 4095 ? 4095 : message_len;
        if (message && len > 0)
            memcpy(c->last_message, message, len);
        c->last_message[len] = '\0';
        c->last_message_len = len;
        return SC_OK;
    }
#else
    if (!c || !c->alloc)
        return SC_ERR_INVALID_ARGUMENT;
    if (!c->agent_id || c->agent_id_len == 0)
        return SC_ERR_CHANNEL_NOT_CONFIGURED;
    if (!c->token || c->token_len == 0)
        return SC_ERR_CHANNEL_NOT_CONFIGURED;
    if (!target || target_len == 0 || !message)
        return SC_ERR_INVALID_ARGUMENT;

    char url_buf[512];
    int n = snprintf(url_buf, sizeof(url_buf), "%s%.*s%s", SC_RCS_API_BASE, (int)target_len, target,
                     SC_RCS_AGENT_MSG_SUFFIX);
    if (n < 0 || (size_t)n >= sizeof(url_buf))
        return SC_ERR_INTERNAL;

    sc_json_buf_t jbuf;
    sc_error_t err = sc_json_buf_init(&jbuf, c->alloc);
    if (err)
        return err;
    err = sc_json_buf_append_raw(&jbuf, "{\"contentMessage\":{\"text\":\"", 28);
    if (err)
        goto jfail;
    err = sc_json_append_string(&jbuf, message, message_len);
    if (err)
        goto jfail;
    err = sc_json_buf_append_raw(&jbuf, "\"}}", 3);
    if (err)
        goto jfail;

    char auth_buf[512];
    n = snprintf(auth_buf, sizeof(auth_buf), "Bearer %.*s", (int)c->token_len, c->token);
    if (n <= 0 || (size_t)n >= sizeof(auth_buf)) {
        sc_json_buf_free(&jbuf);
        return SC_ERR_INTERNAL;
    }

    sc_http_response_t resp = {0};
    err = sc_http_post_json(c->alloc, url_buf, auth_buf, jbuf.ptr, jbuf.len, &resp);
    sc_json_buf_free(&jbuf);
    if (err != SC_OK) {
        if (resp.owned && resp.body)
            sc_http_response_free(c->alloc, &resp);
        return SC_ERR_CHANNEL_SEND;
    }
    if (resp.owned && resp.body)
        sc_http_response_free(c->alloc, &resp);
    if (resp.status_code < 200 || resp.status_code >= 300)
        return SC_ERR_CHANNEL_SEND;

    (void)media_count;
    (void)media;
    return SC_OK;
jfail:
    sc_json_buf_free(&jbuf);
    return err;
#endif
}

static void google_rcs_queue_push(sc_google_rcs_ctx_t *c, const char *from, size_t from_len,
                                  const char *body, size_t body_len) {
    if (!c || !from || !body || c->queue_count >= SC_RCS_QUEUE_MAX)
        return;
    sc_google_rcs_queued_msg_t *slot = &c->queue[c->queue_tail];
    size_t sk = from_len < SC_RCS_SESSION_KEY_MAX ? from_len : SC_RCS_SESSION_KEY_MAX;
    memcpy(slot->session_key, from, sk);
    slot->session_key[sk] = '\0';
    size_t ct = body_len < SC_RCS_CONTENT_MAX ? body_len : SC_RCS_CONTENT_MAX;
    memcpy(slot->content, body, ct);
    slot->content[ct] = '\0';
    c->queue_tail = (c->queue_tail + 1) % SC_RCS_QUEUE_MAX;
    c->queue_count++;
}

sc_error_t sc_google_rcs_on_webhook(void *channel_ctx, sc_allocator_t *alloc, const char *body,
                                    size_t body_len) {
    sc_google_rcs_ctx_t *c = (sc_google_rcs_ctx_t *)channel_ctx;
    if (!c || !body || body_len == 0)
        return SC_ERR_INVALID_ARGUMENT;
#if SC_IS_TEST
    (void)alloc;
    google_rcs_queue_push(c, "test-sender", 11, body, body_len);
    return SC_OK;
#else
    (void)alloc;
    sc_json_value_t *parsed = NULL;
    sc_error_t err = sc_json_parse(alloc, body, body_len, &parsed);
    if (err != SC_OK || !parsed)
        return SC_OK;

    sc_json_value_t *user_msg = sc_json_object_get(parsed, "userMessage");
    if (user_msg && user_msg->type == SC_JSON_OBJECT) {
        sc_json_value_t *text_obj = sc_json_object_get(user_msg, "text");
        const char *text =
            text_obj && text_obj->type == SC_JSON_STRING ? text_obj->data.string.ptr : NULL;
        size_t text_len =
            text_obj && text_obj->type == SC_JSON_STRING ? text_obj->data.string.len : 0;
        const char *sender = sc_json_get_string(parsed, "senderPhoneNumber");
        if (sender && text && text_len > 0)
            google_rcs_queue_push(c, sender, strlen(sender), text, text_len);
    }
    sc_json_free(alloc, parsed);
    return SC_OK;
#endif
}

sc_error_t sc_google_rcs_poll(void *channel_ctx, sc_allocator_t *alloc, sc_channel_loop_msg_t *msgs,
                              size_t max_msgs, size_t *out_count) {
    (void)alloc;
    sc_google_rcs_ctx_t *c = (sc_google_rcs_ctx_t *)channel_ctx;
    if (!c || !msgs || !out_count)
        return SC_ERR_INVALID_ARGUMENT;
    *out_count = 0;
#if SC_IS_TEST
    if (c->mock_count > 0) {
        size_t n = c->mock_count < max_msgs ? c->mock_count : max_msgs;
        for (size_t i = 0; i < n; i++) {
            memcpy(msgs[i].session_key, c->mock_msgs[i].session_key, 128);
            memcpy(msgs[i].content, c->mock_msgs[i].content, 4096);
        }
        *out_count = n;
        c->mock_count = 0;
        return SC_OK;
    }
#endif
    size_t cnt = 0;
    while (c->queue_count > 0 && cnt < max_msgs) {
        sc_google_rcs_queued_msg_t *slot = &c->queue[c->queue_head];
        memcpy(msgs[cnt].session_key, slot->session_key, sizeof(slot->session_key));
        memcpy(msgs[cnt].content, slot->content, sizeof(slot->content));
        c->queue_head = (c->queue_head + 1) % SC_RCS_QUEUE_MAX;
        c->queue_count--;
        cnt++;
    }
    *out_count = cnt;
    return SC_OK;
}

static const char *google_rcs_name(void *ctx) {
    (void)ctx;
    return "google_rcs";
}
static bool google_rcs_health_check(void *ctx) {
    (void)ctx;
    return true;
}

static const sc_channel_vtable_t google_rcs_vtable = {
    .start = google_rcs_start,
    .stop = google_rcs_stop,
    .send = google_rcs_send,
    .name = google_rcs_name,
    .health_check = google_rcs_health_check,
    .send_event = NULL,
    .start_typing = NULL,
    .stop_typing = NULL,
};

sc_error_t sc_google_rcs_create(sc_allocator_t *alloc, const char *agent_id, size_t agent_id_len,
                                const char *token, size_t token_len, sc_channel_t *out) {
    if (!alloc || !out)
        return SC_ERR_INVALID_ARGUMENT;
    sc_google_rcs_ctx_t *c = (sc_google_rcs_ctx_t *)alloc->alloc(alloc->ctx, sizeof(*c));
    if (!c)
        return SC_ERR_OUT_OF_MEMORY;
    memset(c, 0, sizeof(*c));
    c->alloc = alloc;
    if (agent_id && agent_id_len > 0) {
        c->agent_id = (char *)alloc->alloc(alloc->ctx, agent_id_len + 1);
        if (!c->agent_id) {
            alloc->free(alloc->ctx, c, sizeof(*c));
            return SC_ERR_OUT_OF_MEMORY;
        }
        memcpy(c->agent_id, agent_id, agent_id_len);
        c->agent_id[agent_id_len] = '\0';
        c->agent_id_len = agent_id_len;
    }
    if (token && token_len > 0) {
        c->token = (char *)alloc->alloc(alloc->ctx, token_len + 1);
        if (!c->token) {
            if (c->agent_id)
                alloc->free(alloc->ctx, c->agent_id, agent_id_len + 1);
            alloc->free(alloc->ctx, c, sizeof(*c));
            return SC_ERR_OUT_OF_MEMORY;
        }
        memcpy(c->token, token, token_len);
        c->token[token_len] = '\0';
        c->token_len = token_len;
    }
    out->ctx = c;
    out->vtable = &google_rcs_vtable;
    return SC_OK;
}

void sc_google_rcs_destroy(sc_channel_t *ch) {
    if (ch && ch->ctx) {
        sc_google_rcs_ctx_t *c = (sc_google_rcs_ctx_t *)ch->ctx;
        if (c->alloc) {
            if (c->agent_id)
                c->alloc->free(c->alloc->ctx, c->agent_id, c->agent_id_len + 1);
            if (c->token)
                c->alloc->free(c->alloc->ctx, c->token, c->token_len + 1);
            c->alloc->free(c->alloc->ctx, c, sizeof(*c));
        }
        ch->ctx = NULL;
        ch->vtable = NULL;
    }
}

#if SC_IS_TEST
sc_error_t sc_google_rcs_test_inject_mock(sc_channel_t *ch, const char *session_key,
                                          size_t session_key_len, const char *content,
                                          size_t content_len) {
    if (!ch || !ch->ctx)
        return SC_ERR_INVALID_ARGUMENT;
    sc_google_rcs_ctx_t *c = (sc_google_rcs_ctx_t *)ch->ctx;
    if (c->mock_count >= 8)
        return SC_ERR_OUT_OF_MEMORY;
    size_t i = c->mock_count++;
    size_t sk = session_key_len > 127 ? 127 : session_key_len;
    size_t ct = content_len > 4095 ? 4095 : content_len;
    if (session_key && sk > 0)
        memcpy(c->mock_msgs[i].session_key, session_key, sk);
    c->mock_msgs[i].session_key[sk] = '\0';
    if (content && ct > 0)
        memcpy(c->mock_msgs[i].content, content, ct);
    c->mock_msgs[i].content[ct] = '\0';
    return SC_OK;
}
const char *sc_google_rcs_test_get_last_message(sc_channel_t *ch, size_t *out_len) {
    if (!ch || !ch->ctx)
        return NULL;
    sc_google_rcs_ctx_t *c = (sc_google_rcs_ctx_t *)ch->ctx;
    if (out_len)
        *out_len = c->last_message_len;
    return c->last_message;
}
#endif
