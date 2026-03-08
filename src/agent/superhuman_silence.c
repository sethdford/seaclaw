/*
 * Superhuman silence interpreter service — detects meaningful pauses.
 */
#include "seaclaw/agent/superhuman.h"
#include "seaclaw/agent/superhuman_silence.h"
#include "seaclaw/core/string.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t now_ms(void) {
    return (uint64_t)time(NULL) * 1000;
}

static sc_error_t silence_build_context(void *ctx, sc_allocator_t *alloc, char **out,
                                         size_t *out_len) {
    sc_superhuman_silence_ctx_t *sctx = (sc_superhuman_silence_ctx_t *)ctx;
    if (!sctx || !alloc || !out || !out_len)
        return SC_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_len = 0;

    if (!sctx->has_prev)
        return SC_OK;

    uint64_t gap = sctx->last_ts_ms > sctx->prev_ts_ms ? sctx->last_ts_ms - sctx->prev_ts_ms
                                                       : sctx->prev_ts_ms - sctx->last_ts_ms;
    if (gap <= SC_SILENCE_GAP_MS)
        return SC_OK;

    static const char MSG[] =
        "A meaningful pause occurred. The user may be processing, reflecting, or uncertain. "
        "Don't rush to fill silence.";
    *out = sc_strndup(alloc, MSG, sizeof(MSG) - 1);
    if (!*out)
        return SC_ERR_OUT_OF_MEMORY;
    *out_len = sizeof(MSG) - 1;
    return SC_OK;
}

static sc_error_t silence_observe(void *ctx, sc_allocator_t *alloc, const char *text,
                                   size_t text_len, const char *role, size_t role_len) {
    sc_superhuman_silence_ctx_t *sctx = (sc_superhuman_silence_ctx_t *)ctx;
    if (!sctx)
        return SC_ERR_INVALID_ARGUMENT;

    uint64_t ts = now_ms();
    sctx->prev_ts_ms = sctx->last_ts_ms;
    sctx->last_ts_ms = ts;
    sctx->has_prev = (sctx->prev_ts_ms != 0);

    (void)alloc;
    (void)text;
    (void)text_len;
    (void)role;
    (void)role_len;
    return SC_OK;
}

sc_error_t sc_superhuman_silence_service(sc_superhuman_silence_ctx_t *ctx,
                                          sc_superhuman_service_t *out) {
    if (!ctx || !out)
        return SC_ERR_INVALID_ARGUMENT;
    out->name = "Silence Interpreter";
    out->build_context = silence_build_context;
    out->observe = silence_observe;
    out->ctx = ctx;
    return SC_OK;
}
