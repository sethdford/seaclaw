#include "seaclaw/channel.h"
#include "seaclaw/channel_loop.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/core/http.h"
#include "seaclaw/core/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LARK_QUEUE_MAX       32
#define LARK_SESSION_KEY_MAX 127
#define LARK_CONTENT_MAX     4095

typedef struct sc_lark_queued_msg {
    char session_key[128];
    char content[4096];
} sc_lark_queued_msg_t;

typedef struct sc_lark_ctx {
    sc_allocator_t *alloc;
    char *webhook_url;
    size_t webhook_url_len;
    char *app_id;
    size_t app_id_len;
    char *app_secret;
    size_t app_secret_len;
    bool running;
    sc_lark_queued_msg_t queue[LARK_QUEUE_MAX];
    size_t queue_head;
    size_t queue_tail;
    size_t queue_count;
} sc_lark_ctx_t;

static sc_error_t lark_start(void *ctx) {
    sc_lark_ctx_t *c = (sc_lark_ctx_t *)ctx;
    if (!c)
        return SC_ERR_INVALID_ARGUMENT;
    c->running = true;
    return SC_OK;
}

static void lark_stop(void *ctx) {
    sc_lark_ctx_t *c = (sc_lark_ctx_t *)ctx;
    if (c)
        c->running = false;
}

static void lark_queue_push(sc_lark_ctx_t *c, const char *from, size_t from_len, const char *body,
                            size_t body_len) {
    if (c->queue_count >= LARK_QUEUE_MAX)
        return;
    sc_lark_queued_msg_t *slot = &c->queue[c->queue_tail];
    size_t sk = from_len < LARK_SESSION_KEY_MAX ? from_len : LARK_SESSION_KEY_MAX;
    memcpy(slot->session_key, from, sk);
    slot->session_key[sk] = '\0';
    size_t ct = body_len < LARK_CONTENT_MAX ? body_len : LARK_CONTENT_MAX;
    memcpy(slot->content, body, ct);
    slot->content[ct] = '\0';
    c->queue_tail = (c->queue_tail + 1) % LARK_QUEUE_MAX;
    c->queue_count++;
}

static sc_error_t lark_send(void *ctx, const char *target, size_t target_len, const char *message,
                            size_t message_len, const char *const *media, size_t media_count) {
    (void)target;
    (void)target_len;
    (void)media;
    (void)media_count;
    sc_lark_ctx_t *c = (sc_lark_ctx_t *)ctx;

#if SC_IS_TEST
    if (!c || !message)
        return SC_ERR_INVALID_ARGUMENT;
    if (c->webhook_url && c->webhook_url_len > 0)
        lark_queue_push(c, "test-sender", 11, message, message_len);
    return SC_OK;
#else
    if (!c || !c->alloc)
        return SC_ERR_INVALID_ARGUMENT;
    if (!c->webhook_url || c->webhook_url_len == 0)
        return SC_ERR_CHANNEL_NOT_CONFIGURED;
    if (!message)
        return SC_ERR_INVALID_ARGUMENT;
    if (c->webhook_url_len < 9 || strncmp(c->webhook_url, "https://", 8) != 0)
        return SC_ERR_CHANNEL_NOT_CONFIGURED;

    sc_json_buf_t inner;
    sc_error_t err = sc_json_buf_init(&inner, c->alloc);
    if (err)
        return err;
    err = sc_json_buf_append_raw(&inner, "{\"text\":", 8);
    if (err)
        goto inner_fail;
    err = sc_json_append_string(&inner, message, message_len);
    if (err)
        goto inner_fail;
    err = sc_json_buf_append_raw(&inner, "}", 1);
    if (err)
        goto inner_fail;

    sc_json_buf_t jbuf;
    err = sc_json_buf_init(&jbuf, c->alloc);
    if (err)
        goto inner_fail;
    err = sc_json_buf_append_raw(&jbuf, "{\"msg_type\":\"text\",\"content\":", 29);
    if (err)
        goto jfail;
    err = sc_json_buf_append_raw(&jbuf, inner.ptr, inner.len);
    if (err)
        goto jfail;
    err = sc_json_buf_append_raw(&jbuf, "}", 1);
    if (err)
        goto jfail;
    sc_json_buf_free(&inner);

    sc_http_response_t resp = {0};
    err = sc_http_post_json(c->alloc, c->webhook_url, NULL, jbuf.ptr, jbuf.len, &resp);
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
    return SC_OK;
jfail:
    sc_json_buf_free(&jbuf);
inner_fail:
    sc_json_buf_free(&inner);
    return err;
#endif
}

static const char *lark_name(void *ctx) {
    (void)ctx;
    return "lark";
}
static bool lark_health_check(void *ctx) {
    (void)ctx;
    return true;
}

static const sc_channel_vtable_t lark_vtable = {
    .start = lark_start,
    .stop = lark_stop,
    .send = lark_send,
    .name = lark_name,
    .health_check = lark_health_check,
    .send_event = NULL,
    .start_typing = NULL,
    .stop_typing = NULL,
};

sc_error_t sc_lark_on_webhook(void *channel_ctx, sc_allocator_t *alloc, const char *body,
                              size_t body_len) {
    sc_lark_ctx_t *c = (sc_lark_ctx_t *)channel_ctx;
    if (!c || !body || body_len == 0)
        return SC_ERR_INVALID_ARGUMENT;
#if SC_IS_TEST
    (void)alloc;
    lark_queue_push(c, "test-sender", 11, body, body_len);
    return SC_OK;
#else
    sc_json_value_t *parsed = NULL;
    sc_error_t err = sc_json_parse(alloc, body, body_len, &parsed);
    if (err != SC_OK || !parsed)
        return SC_OK;
    sc_json_value_t *ev = sc_json_object_get(parsed, "event");
    if (ev && ev->type == SC_JSON_OBJECT) {
        sc_json_value_t *msg = sc_json_object_get(ev, "message");
        sc_json_value_t *sender = sc_json_object_get(ev, "sender");
        if (msg && sender) {
            sc_json_value_t *sid = sc_json_object_get(sender, "sender_id");
            const char *open_id = sid ? sc_json_get_string(sid, "open_id") : NULL;
            const char *content = sc_json_get_string(msg, "content");
            if (open_id && content && strlen(content) > 0)
                lark_queue_push(c, open_id, strlen(open_id), content, strlen(content));
        }
    }
    sc_json_free(alloc, parsed);
    return SC_OK;
#endif
}

sc_error_t sc_lark_poll(void *channel_ctx, sc_allocator_t *alloc, sc_channel_loop_msg_t *msgs,
                        size_t max_msgs, size_t *out_count) {
    (void)alloc;
    sc_lark_ctx_t *c = (sc_lark_ctx_t *)channel_ctx;
    if (!c || !msgs || !out_count)
        return SC_ERR_INVALID_ARGUMENT;
    *out_count = 0;
    size_t cnt = 0;
    while (c->queue_count > 0 && cnt < max_msgs) {
        sc_lark_queued_msg_t *slot = &c->queue[c->queue_head];
        memcpy(msgs[cnt].session_key, slot->session_key, sizeof(slot->session_key));
        memcpy(msgs[cnt].content, slot->content, sizeof(slot->content));
        c->queue_head = (c->queue_head + 1) % LARK_QUEUE_MAX;
        c->queue_count--;
        cnt++;
    }
    *out_count = cnt;
    return SC_OK;
}

sc_error_t sc_lark_create(sc_allocator_t *alloc, const char *app_id_or_webhook, size_t app_id_len,
                          const char *app_secret, size_t app_secret_len, sc_channel_t *out) {
    if (!alloc || !out)
        return SC_ERR_INVALID_ARGUMENT;
    sc_lark_ctx_t *c = (sc_lark_ctx_t *)calloc(1, sizeof(*c));
    if (!c)
        return SC_ERR_OUT_OF_MEMORY;
    c->alloc = alloc;
    if (app_id_or_webhook && app_id_len > 0) {
        if (app_id_len >= 8 && strncmp(app_id_or_webhook, "https://", 8) == 0) {
            c->webhook_url = (char *)malloc(app_id_len + 1);
            if (!c->webhook_url) {
                free(c);
                return SC_ERR_OUT_OF_MEMORY;
            }
            memcpy(c->webhook_url, app_id_or_webhook, app_id_len);
            c->webhook_url[app_id_len] = '\0';
            c->webhook_url_len = app_id_len;
        } else {
            c->app_id = (char *)malloc(app_id_len + 1);
            if (!c->app_id) {
                free(c);
                return SC_ERR_OUT_OF_MEMORY;
            }
            memcpy(c->app_id, app_id_or_webhook, app_id_len);
            c->app_id[app_id_len] = '\0';
            c->app_id_len = app_id_len;
        }
    }
    if (app_secret && app_secret_len > 0) {
        c->app_secret = (char *)malloc(app_secret_len + 1);
        if (!c->app_secret) {
            if (c->app_id)
                free(c->app_id);
            if (c->webhook_url)
                free(c->webhook_url);
            free(c);
            return SC_ERR_OUT_OF_MEMORY;
        }
        memcpy(c->app_secret, app_secret, app_secret_len);
        c->app_secret[app_secret_len] = '\0';
        c->app_secret_len = app_secret_len;
    }
    out->ctx = c;
    out->vtable = &lark_vtable;
    return SC_OK;
}

bool sc_lark_is_configured(sc_channel_t *ch) {
    if (!ch || !ch->ctx)
        return false;
    sc_lark_ctx_t *c = (sc_lark_ctx_t *)ch->ctx;
    return (c->webhook_url != NULL && c->webhook_url_len > 0);
}

void sc_lark_destroy(sc_channel_t *ch) {
    if (ch && ch->ctx) {
        sc_lark_ctx_t *c = (sc_lark_ctx_t *)ch->ctx;
        if (c->webhook_url)
            free(c->webhook_url);
        if (c->app_id)
            free(c->app_id);
        if (c->app_secret)
            free(c->app_secret);
        free(c);
        ch->ctx = NULL;
        ch->vtable = NULL;
    }
}
