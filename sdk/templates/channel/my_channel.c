/*
 * Template: Custom Channel
 *
 * Implements hu_channel_t vtable. Required methods:
 *   - start, stop, send, name, health_check
 *
 * Optional (may be NULL; see include/human/channel.h):
 *   - send_event, start_typing, stop_typing
 *   - load_conversation_history, get_response_constraints, react
 *   - get_attachment_path, human_active_recently, get_latest_attachment_path
 *   - build_reaction_context, build_read_receipt_context
 */
#include "my_channel.h"
#include "human/channel.h"
#include <string.h>

typedef struct hu_my_channel_ctx {
    hu_allocator_t *alloc;
    bool running;
} hu_my_channel_ctx_t;

static hu_error_t my_channel_start(void *ctx) {
    hu_my_channel_ctx_t *c = (hu_my_channel_ctx_t *)ctx;
    if (!c) return HU_ERR_INVALID_ARGUMENT;
    c->running = true;
    return HU_OK;
}

static void my_channel_stop(void *ctx) {
    hu_my_channel_ctx_t *c = (hu_my_channel_ctx_t *)ctx;
    if (c) c->running = false;
}

static hu_error_t my_channel_send(void *ctx,
    const char *target, size_t target_len,
    const char *message, size_t message_len,
    const char *const *media, size_t media_count)
{
#if HU_IS_TEST
    (void)ctx;
    (void)target;
    (void)target_len;
    (void)message;
    (void)message_len;
    (void)media;
    (void)media_count;
    return HU_OK;
#else
    hu_my_channel_ctx_t *c = (hu_my_channel_ctx_t *)ctx;
    if (!c || !c->running) return HU_ERR_CHANNEL_SEND;

    /* Implementation checklist (AGENTS.md §7.2 — Adding a Channel):
     * [ ] Validate target and message pointers; respect platform length limits.
     * [ ] Deliver via your transport (HTTP, WebSocket, etc.); guard with HU_IS_TEST.
     * [ ] If media_count > 0, attach URLs from media[] per platform API.
     * [ ] Register in src/channels/factory.c; add config/auth/health tests.
     */
    (void)target;
    (void)target_len;
    (void)message;
    (void)message_len;
    (void)media;
    (void)media_count;
    return HU_OK;
#endif
}

static const char *my_channel_name(void *ctx) {
    (void)ctx;
    return "my_channel";
}

static bool my_channel_health_check(void *ctx) {
    hu_my_channel_ctx_t *c = (hu_my_channel_ctx_t *)ctx;
    return c && c->running;
}

static const hu_channel_vtable_t my_channel_vtable = {
    .start = my_channel_start,
    .stop = my_channel_stop,
    .send = my_channel_send,
    .name = my_channel_name,
    .health_check = my_channel_health_check,
    .send_event = NULL,
    .start_typing = NULL,
    .stop_typing = NULL,
    .load_conversation_history = NULL,
    .get_response_constraints = NULL,
    .react = NULL,
    .get_attachment_path = NULL,
    .human_active_recently = NULL,
    .get_latest_attachment_path = NULL,
    .build_reaction_context = NULL,
    .build_read_receipt_context = NULL,
};

hu_error_t hu_my_channel_create(hu_allocator_t *alloc, hu_channel_t *out) {
    if (!alloc || !out) return HU_ERR_INVALID_ARGUMENT;

    hu_my_channel_ctx_t *c =
        (hu_my_channel_ctx_t *)alloc->alloc(alloc->ctx, sizeof(*c));
    if (!c) return HU_ERR_OUT_OF_MEMORY;
    memset(c, 0, sizeof(*c));
    c->alloc = alloc;
    c->running = false;

    out->ctx = c;
    out->vtable = &my_channel_vtable;
    return HU_OK;
}

void hu_my_channel_destroy(hu_channel_t *ch) {
    if (ch && ch->ctx) {
        hu_my_channel_ctx_t *c = (hu_my_channel_ctx_t *)ch->ctx;
        if (c->alloc)
            c->alloc->free(c->alloc->ctx, c, sizeof(*c));
        ch->ctx = NULL;
        ch->vtable = NULL;
    }
}
