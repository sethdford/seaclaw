#include "seaclaw/channels/qq.h"
#include "seaclaw/core/http.h"
#include "seaclaw/core/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define QQ_API_BASE     "https://api.sgroup.qq.com"
#define QQ_SANDBOX_BASE "https://sandbox.api.sgroup.qq.com"
#define QQ_MAX_MSG      4096

#define QQ_QUEUE_MAX       32
#define QQ_SESSION_KEY_MAX 127
#define QQ_CONTENT_MAX     4095

typedef struct sc_qq_queued_msg {
    char session_key[128];
    char content[4096];
} sc_qq_queued_msg_t;

typedef struct sc_qq_ctx {
    sc_allocator_t *alloc;
    char *app_id;
    char *bot_token;
    char *channel_id;
    size_t channel_id_len;
    bool sandbox;
    bool running;
    sc_qq_queued_msg_t queue[QQ_QUEUE_MAX];
    size_t queue_head;
    size_t queue_tail;
    size_t queue_count;
} sc_qq_ctx_t;

static sc_error_t qq_start(void *ctx) {
    sc_qq_ctx_t *c = (sc_qq_ctx_t *)ctx;
    if (!c)
        return SC_ERR_INVALID_ARGUMENT;
    c->running = true;
    return SC_OK;
}

static void qq_stop(void *ctx) {
    sc_qq_ctx_t *c = (sc_qq_ctx_t *)ctx;
    if (c)
        c->running = false;
}

static void qq_queue_push(sc_qq_ctx_t *c, const char *from, size_t from_len, const char *body,
                          size_t body_len) {
    if (c->queue_count >= QQ_QUEUE_MAX)
        return;
    sc_qq_queued_msg_t *slot = &c->queue[c->queue_tail];
    size_t sk = from_len < QQ_SESSION_KEY_MAX ? from_len : QQ_SESSION_KEY_MAX;
    memcpy(slot->session_key, from, sk);
    slot->session_key[sk] = '\0';
    size_t ct = body_len < QQ_CONTENT_MAX ? body_len : QQ_CONTENT_MAX;
    memcpy(slot->content, body, ct);
    slot->content[ct] = '\0';
    c->queue_tail = (c->queue_tail + 1) % QQ_QUEUE_MAX;
    c->queue_count++;
}

static sc_error_t qq_send(void *ctx, const char *target, size_t target_len, const char *message,
                          size_t message_len, const char *const *media, size_t media_count) {
    (void)media;
    (void)media_count;
    sc_qq_ctx_t *c = (sc_qq_ctx_t *)ctx;

#if SC_IS_TEST
    if (!c || !message)
        return SC_ERR_INVALID_ARGUMENT;
    if (!c->bot_token)
        return SC_ERR_CHANNEL_NOT_CONFIGURED;
    if ((!target || target_len == 0) && (!c->channel_id || c->channel_id_len == 0))
        return SC_ERR_CHANNEL_NOT_CONFIGURED;
    qq_queue_push(c, "test-sender", 11, message, message_len);
    return SC_OK;
#else
    const char *ch_id = (target && target_len > 0) ? target : c->channel_id;
    size_t ch_id_len = (target && target_len > 0) ? target_len : c->channel_id_len;

    if (!c || !c->alloc)
        return SC_ERR_INVALID_ARGUMENT;
    if (!c->bot_token || !ch_id || ch_id_len == 0 || !message)
        return SC_ERR_INVALID_ARGUMENT;

    const char *base = c->sandbox ? QQ_SANDBOX_BASE : QQ_API_BASE;
    char url_buf[512];
    int n = snprintf(url_buf, sizeof(url_buf), "%s/channels/%.*s/messages", base, (int)ch_id_len,
                     ch_id);
    if (n < 0 || (size_t)n >= sizeof(url_buf))
        return SC_ERR_INTERNAL;

    sc_json_buf_t jbuf;
    sc_error_t err = sc_json_buf_init(&jbuf, c->alloc);
    if (err)
        return err;
    err = sc_json_buf_append_raw(&jbuf, "{\"content\":", 11);
    if (err)
        goto jfail;
    err = sc_json_append_string(&jbuf, message, message_len);
    if (err)
        goto jfail;
    err = sc_json_buf_append_raw(&jbuf, "}", 1);
    if (err)
        goto jfail;

    char *body = (char *)c->alloc->alloc(c->alloc->ctx, jbuf.len + 1);
    if (!body) {
        err = SC_ERR_OUT_OF_MEMORY;
        goto jfail;
    }
    memcpy(body, jbuf.ptr, jbuf.len + 1);
    size_t body_len = jbuf.len;
    sc_json_buf_free(&jbuf);

    char auth_buf[256];
    int ab = snprintf(auth_buf, sizeof(auth_buf), "Bot %s.%s", c->app_id ? c->app_id : "",
                      c->bot_token ? c->bot_token : "");
    if (ab <= 0 || (size_t)ab >= sizeof(auth_buf)) {
        c->alloc->free(c->alloc->ctx, body, body_len + 1);
        return SC_ERR_INTERNAL;
    }

    sc_http_response_t resp = {0};
    err = sc_http_post_json(c->alloc, url_buf, auth_buf, body, body_len, &resp);
    c->alloc->free(c->alloc->ctx, body, body_len + 1);
    if (err) {
        if (resp.owned && resp.body)
            sc_http_response_free(c->alloc, &resp);
        return SC_ERR_CHANNEL_SEND;
    }
    if (resp.owned && resp.body)
        sc_http_response_free(c->alloc, &resp);
    return SC_OK;
jfail:
    sc_json_buf_free(&jbuf);
    return err;
#endif
}

static const char *qq_name(void *ctx) {
    (void)ctx;
    return "qq";
}
static bool qq_health_check(void *ctx) {
    sc_qq_ctx_t *c = (sc_qq_ctx_t *)ctx;
    return c && c->running;
}

static const sc_channel_vtable_t qq_vtable = {
    .start = qq_start,
    .stop = qq_stop,
    .send = qq_send,
    .name = qq_name,
    .health_check = qq_health_check,
    .send_event = NULL,
    .start_typing = NULL,
    .stop_typing = NULL,
};

sc_error_t sc_qq_on_webhook(void *channel_ctx, sc_allocator_t *alloc, const char *body,
                            size_t body_len) {
    sc_qq_ctx_t *c = (sc_qq_ctx_t *)channel_ctx;
    if (!c || !body || body_len == 0)
        return SC_ERR_INVALID_ARGUMENT;
#if SC_IS_TEST
    (void)alloc;
    qq_queue_push(c, "test-sender", 11, body, body_len);
    return SC_OK;
#else
    sc_json_value_t *parsed = NULL;
    sc_error_t err = sc_json_parse(alloc, body, body_len, &parsed);
    if (err != SC_OK || !parsed)
        return SC_OK;
    sc_json_value_t *d = sc_json_object_get(parsed, "d");
    if (d && d->type == SC_JSON_OBJECT) {
        const char *content = sc_json_get_string(d, "content");
        sc_json_value_t *author = sc_json_object_get(d, "author");
        const char *author_id = author ? sc_json_get_string(author, "id") : NULL;
        if (content && author_id && strlen(content) > 0)
            qq_queue_push(c, author_id, strlen(author_id), content, strlen(content));
    }
    sc_json_free(alloc, parsed);
    return SC_OK;
#endif
}

sc_error_t sc_qq_poll(void *channel_ctx, sc_allocator_t *alloc, sc_channel_loop_msg_t *msgs,
                      size_t max_msgs, size_t *out_count) {
    (void)alloc;
    sc_qq_ctx_t *c = (sc_qq_ctx_t *)channel_ctx;
    if (!c || !msgs || !out_count)
        return SC_ERR_INVALID_ARGUMENT;
    *out_count = 0;
    size_t cnt = 0;
    while (c->queue_count > 0 && cnt < max_msgs) {
        sc_qq_queued_msg_t *slot = &c->queue[c->queue_head];
        memcpy(msgs[cnt].session_key, slot->session_key, sizeof(slot->session_key));
        memcpy(msgs[cnt].content, slot->content, sizeof(slot->content));
        c->queue_head = (c->queue_head + 1) % QQ_QUEUE_MAX;
        c->queue_count--;
        cnt++;
    }
    *out_count = cnt;
    return SC_OK;
}

sc_error_t sc_qq_create(sc_allocator_t *alloc, const char *app_id, size_t app_id_len,
                        const char *bot_token, size_t bot_token_len, bool sandbox,
                        sc_channel_t *out) {
    return sc_qq_create_ex(alloc, app_id, app_id_len, bot_token, bot_token_len, NULL, 0, sandbox,
                           out);
}

sc_error_t sc_qq_create_ex(sc_allocator_t *alloc, const char *app_id, size_t app_id_len,
                           const char *bot_token, size_t bot_token_len, const char *channel_id,
                           size_t channel_id_len, bool sandbox, sc_channel_t *out) {
    if (!alloc || !out)
        return SC_ERR_INVALID_ARGUMENT;
    sc_qq_ctx_t *c = (sc_qq_ctx_t *)calloc(1, sizeof(*c));
    if (!c)
        return SC_ERR_OUT_OF_MEMORY;
    c->alloc = alloc;
    c->sandbox = sandbox;
    if (app_id && app_id_len > 0) {
        c->app_id = (char *)malloc(app_id_len + 1);
        if (!c->app_id) {
            free(c);
            return SC_ERR_OUT_OF_MEMORY;
        }
        memcpy(c->app_id, app_id, app_id_len);
        c->app_id[app_id_len] = '\0';
    }
    if (bot_token && bot_token_len > 0) {
        c->bot_token = (char *)malloc(bot_token_len + 1);
        if (!c->bot_token) {
            if (c->app_id)
                free(c->app_id);
            free(c);
            return SC_ERR_OUT_OF_MEMORY;
        }
        memcpy(c->bot_token, bot_token, bot_token_len);
        c->bot_token[bot_token_len] = '\0';
    }
    if (channel_id && channel_id_len > 0) {
        c->channel_id = (char *)malloc(channel_id_len + 1);
        if (!c->channel_id) {
            if (c->bot_token)
                free(c->bot_token);
            if (c->app_id)
                free(c->app_id);
            free(c);
            return SC_ERR_OUT_OF_MEMORY;
        }
        memcpy(c->channel_id, channel_id, channel_id_len);
        c->channel_id[channel_id_len] = '\0';
        c->channel_id_len = channel_id_len;
    }
    out->ctx = c;
    out->vtable = &qq_vtable;
    return SC_OK;
}

bool sc_qq_is_configured(sc_channel_t *ch) {
    if (!ch || !ch->ctx)
        return false;
    sc_qq_ctx_t *c = (sc_qq_ctx_t *)ch->ctx;
    return c->bot_token != NULL && (c->channel_id != NULL && c->channel_id_len > 0);
}

void sc_qq_destroy(sc_channel_t *ch) {
    if (ch && ch->ctx) {
        sc_qq_ctx_t *c = (sc_qq_ctx_t *)ch->ctx;
        if (c->app_id)
            free(c->app_id);
        if (c->bot_token)
            free(c->bot_token);
        if (c->channel_id)
            free(c->channel_id);
        free(c);
        ch->ctx = NULL;
        ch->vtable = NULL;
    }
}
