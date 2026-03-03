/*
 * Template: Custom Channel
 *
 * Implements sc_channel_t vtable. Required methods:
 *   - start: initialize / start listening
 *   - stop: stop listening
 *   - send: send message to target
 *   - name: return channel name (static string)
 *   - health_check: return true if healthy
 *
 * Optional (may be NULL): send_event, start_typing, stop_typing
 */
#include "my_channel.h"
#include "seaclaw/channel.h"
#include <string.h>
#include <stdlib.h>

typedef struct sc_my_channel_ctx {
    bool running;
} sc_my_channel_ctx_t;

static sc_error_t my_channel_start(void *ctx) {
    sc_my_channel_ctx_t *c = (sc_my_channel_ctx_t *)ctx;
    if (!c) return SC_ERR_INVALID_ARGUMENT;
    c->running = true;
    return SC_OK;
}

static void my_channel_stop(void *ctx) {
    sc_my_channel_ctx_t *c = (sc_my_channel_ctx_t *)ctx;
    if (c) c->running = false;
}

static sc_error_t my_channel_send(void *ctx,
    const char *target, size_t target_len,
    const char *message, size_t message_len,
    const char *const *media, size_t media_count)
{
#if SC_IS_TEST
    (void)ctx;
    (void)target;
    (void)target_len;
    (void)message;
    (void)message_len;
    (void)media;
    (void)media_count;
    return SC_OK;
#else
    sc_my_channel_ctx_t *c = (sc_my_channel_ctx_t *)ctx;
    if (!c || !c->running) return SC_ERR_CHANNEL_SEND;

    /* TODO: Send message to target via your transport (HTTP, WebSocket, etc.).
     * media is optional array of URLs; media_count 0 if none. */
    (void)target;
    (void)target_len;
    (void)message;
    (void)message_len;
    (void)media;
    (void)media_count;
    return SC_OK;
#endif
}

static const char *my_channel_name(void *ctx) {
    (void)ctx;
    return "my_channel";
}

static bool my_channel_health_check(void *ctx) {
    sc_my_channel_ctx_t *c = (sc_my_channel_ctx_t *)ctx;
    return c && c->running;
}

static const sc_channel_vtable_t my_channel_vtable = {
    .start = my_channel_start,
    .stop = my_channel_stop,
    .send = my_channel_send,
    .name = my_channel_name,
    .health_check = my_channel_health_check,
    .send_event = NULL,
    .start_typing = NULL,
    .stop_typing = NULL,
};

sc_error_t sc_my_channel_create(sc_allocator_t *alloc, sc_channel_t *out) {
    (void)alloc;
    if (!out) return SC_ERR_INVALID_ARGUMENT;

    sc_my_channel_ctx_t *c = (sc_my_channel_ctx_t *)calloc(1, sizeof(*c));
    if (!c) return SC_ERR_OUT_OF_MEMORY;
    c->running = false;

    out->ctx = c;
    out->vtable = &my_channel_vtable;
    return SC_OK;
}

void sc_my_channel_destroy(sc_channel_t *ch) {
    if (ch && ch->ctx) {
        free(ch->ctx);
        ch->ctx = NULL;
        ch->vtable = NULL;
    }
}
