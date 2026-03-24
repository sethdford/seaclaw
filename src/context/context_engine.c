/*
 * Default (legacy) context engine — wraps h-uman's existing context
 * management behavior behind the hu_context_engine_t vtable.
 */

#include "human/context_engine.h"
#include "human/core/string.h"
#include <string.h>

typedef struct {
    hu_allocator_t *alloc;
    hu_context_message_t *history;
    size_t history_count;
    size_t history_cap;
} legacy_ctx_t;

static hu_error_t legacy_bootstrap(void *ctx_ptr, hu_allocator_t *alloc) {
    (void)alloc;
    legacy_ctx_t *ctx = (legacy_ctx_t *)ctx_ptr;
    if (!ctx)
        return HU_ERR_INVALID_ARGUMENT;
    return HU_OK;
}

static hu_error_t legacy_ingest(void *ctx_ptr, hu_allocator_t *alloc,
                                const hu_context_message_t *message) {
    legacy_ctx_t *ctx = (legacy_ctx_t *)ctx_ptr;
    if (!ctx || !alloc || !message)
        return HU_ERR_INVALID_ARGUMENT;

    if (ctx->history_count >= ctx->history_cap) {
        size_t new_cap = ctx->history_cap == 0 ? 64 : ctx->history_cap * 2;
        hu_context_message_t *nb = (hu_context_message_t *)alloc->alloc(
            alloc->ctx, new_cap * sizeof(hu_context_message_t));
        if (!nb)
            return HU_ERR_OUT_OF_MEMORY;
        if (ctx->history && ctx->history_count > 0)
            memcpy(nb, ctx->history, ctx->history_count * sizeof(hu_context_message_t));
        if (ctx->history)
            alloc->free(alloc->ctx, ctx->history, ctx->history_cap * sizeof(hu_context_message_t));
        ctx->history = nb;
        ctx->history_cap = new_cap;
    }

    hu_context_message_t *entry = &ctx->history[ctx->history_count];
    entry->role = hu_strndup(alloc, message->role, message->role_len);
    entry->role_len = message->role_len;
    entry->content = hu_strndup(alloc, message->content, message->content_len);
    entry->content_len = message->content_len;
    entry->timestamp = message->timestamp;

    if (!entry->role || !entry->content) {
        if (entry->role)
            hu_str_free(alloc, (char *)entry->role);
        if (entry->content)
            hu_str_free(alloc, (char *)entry->content);
        return HU_ERR_OUT_OF_MEMORY;
    }

    ctx->history_count++;
    return HU_OK;
}

static hu_error_t legacy_assemble(void *ctx_ptr, hu_allocator_t *alloc,
                                  const hu_context_budget_t *budget, hu_assembled_context_t *out) {
    legacy_ctx_t *ctx = (legacy_ctx_t *)ctx_ptr;
    if (!ctx || !alloc || !budget || !out)
        return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));

    /* Recency-based: include as many recent messages as fit the budget.
     * ~4 chars per token is a rough estimate for budget checking. */
    size_t token_estimate = 0;
    size_t available = budget->max_tokens - budget->reserved_tokens;
    size_t start = ctx->history_count;

    while (start > 0) {
        size_t idx = start - 1;
        size_t msg_tokens = (ctx->history[idx].content_len + 3) / 4;
        if (token_estimate + msg_tokens > available)
            break;
        token_estimate += msg_tokens;
        start = idx;
    }

    size_t count = ctx->history_count - start;
    if (count > 0) {
        out->messages =
            (hu_context_message_t *)alloc->alloc(alloc->ctx, count * sizeof(hu_context_message_t));
        if (!out->messages)
            return HU_ERR_OUT_OF_MEMORY;
        memcpy(out->messages, ctx->history + start, count * sizeof(hu_context_message_t));
        out->messages_count = count;
        out->owns_messages = false;
    }
    out->total_tokens = token_estimate;
    return HU_OK;
}

static hu_error_t legacy_compact(void *ctx_ptr, hu_allocator_t *alloc, size_t target_tokens) {
    legacy_ctx_t *ctx = (legacy_ctx_t *)ctx_ptr;
    if (!ctx || !alloc)
        return HU_ERR_INVALID_ARGUMENT;

    /* Simple compaction: keep the most recent messages that fit the target.
     * More sophisticated implementations can summarize older messages. */
    size_t token_estimate = 0;
    size_t keep_from = ctx->history_count;

    while (keep_from > 0) {
        size_t idx = keep_from - 1;
        size_t msg_tokens = (ctx->history[idx].content_len + 3) / 4;
        if (token_estimate + msg_tokens > target_tokens)
            break;
        token_estimate += msg_tokens;
        keep_from = idx;
    }

    /* Free evicted messages */
    for (size_t i = 0; i < keep_from; i++) {
        if (ctx->history[i].role)
            hu_str_free(alloc, (char *)ctx->history[i].role);
        if (ctx->history[i].content)
            hu_str_free(alloc, (char *)ctx->history[i].content);
    }

    if (keep_from > 0 && keep_from < ctx->history_count) {
        size_t remaining = ctx->history_count - keep_from;
        memmove(ctx->history, ctx->history + keep_from, remaining * sizeof(hu_context_message_t));
        ctx->history_count = remaining;
    } else if (keep_from >= ctx->history_count) {
        ctx->history_count = 0;
    }

    return HU_OK;
}

static hu_error_t legacy_after_turn(void *ctx_ptr, hu_allocator_t *alloc,
                                    const hu_context_message_t *user_msg,
                                    const hu_context_message_t *assistant_msg) {
    (void)ctx_ptr;
    (void)alloc;
    (void)user_msg;
    (void)assistant_msg;
    return HU_OK;
}

static hu_error_t legacy_prepare_subagent(void *ctx_ptr, hu_allocator_t *alloc,
                                          hu_context_engine_t *subagent_engine) {
    (void)ctx_ptr;
    (void)alloc;
    (void)subagent_engine;
    return HU_ERR_NOT_SUPPORTED;
}

static hu_error_t legacy_merge_subagent(void *ctx_ptr, hu_allocator_t *alloc,
                                        const hu_context_engine_t *subagent_engine) {
    (void)ctx_ptr;
    (void)alloc;
    (void)subagent_engine;
    return HU_ERR_NOT_SUPPORTED;
}

static const char *legacy_get_name(void *ctx) {
    (void)ctx;
    return "legacy";
}

static void legacy_deinit(void *ctx_ptr, hu_allocator_t *alloc) {
    legacy_ctx_t *ctx = (legacy_ctx_t *)ctx_ptr;
    if (!ctx || !alloc)
        return;
    for (size_t i = 0; i < ctx->history_count; i++) {
        if (ctx->history[i].role)
            hu_str_free(alloc, (char *)ctx->history[i].role);
        if (ctx->history[i].content)
            hu_str_free(alloc, (char *)ctx->history[i].content);
    }
    if (ctx->history)
        alloc->free(alloc->ctx, ctx->history, ctx->history_cap * sizeof(hu_context_message_t));
    alloc->free(alloc->ctx, ctx, sizeof(legacy_ctx_t));
}

static const hu_context_engine_vtable_t legacy_vtable = {
    .bootstrap = legacy_bootstrap,
    .ingest = legacy_ingest,
    .assemble = legacy_assemble,
    .compact = legacy_compact,
    .after_turn = legacy_after_turn,
    .prepare_subagent = legacy_prepare_subagent,
    .merge_subagent = legacy_merge_subagent,
    .get_name = legacy_get_name,
    .deinit = legacy_deinit,
};

hu_error_t hu_context_engine_legacy_create(hu_allocator_t *alloc, hu_context_engine_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;

    legacy_ctx_t *ctx = (legacy_ctx_t *)alloc->alloc(alloc->ctx, sizeof(legacy_ctx_t));
    if (!ctx)
        return HU_ERR_OUT_OF_MEMORY;
    memset(ctx, 0, sizeof(*ctx));
    ctx->alloc = alloc;

    out->ctx = ctx;
    out->vtable = &legacy_vtable;
    return HU_OK;
}

void hu_assembled_context_free(hu_allocator_t *alloc, hu_assembled_context_t *ctx) {
    if (!alloc || !ctx)
        return;
    if (ctx->system_prompt)
        hu_str_free(alloc, ctx->system_prompt);
    if (ctx->injected_context)
        hu_str_free(alloc, ctx->injected_context);
    if (ctx->owns_messages && ctx->messages) {
        for (size_t i = 0; i < ctx->messages_count; i++) {
            if (ctx->messages[i].role)
                hu_str_free(alloc, (char *)ctx->messages[i].role);
            if (ctx->messages[i].content)
                hu_str_free(alloc, (char *)ctx->messages[i].content);
        }
    }
    if (ctx->messages)
        alloc->free(alloc->ctx, ctx->messages, ctx->messages_count * sizeof(hu_context_message_t));
    memset(ctx, 0, sizeof(*ctx));
}
