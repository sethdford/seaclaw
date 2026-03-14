#include "human/core/http.h"
#include "human/core/string.h"
#include "human/mcp_transport.h"
#include <string.h>

typedef struct sse_ctx {
    char *url;
} sse_ctx_t;

static hu_error_t sse_send(void *ctx, const char *data, size_t len) {
#ifdef HU_HTTP_CURL
    sse_ctx_t *c = (sse_ctx_t *)ctx;
    if (!c || !c->url || !data)
        return HU_ERR_INVALID_ARGUMENT;
    hu_allocator_t alloc = hu_system_allocator();
    hu_http_response_t resp = {0};
    hu_error_t err = hu_http_post_json(&alloc, c->url, NULL, data, len, &resp);
    if (resp.owned && resp.body)
        hu_http_response_free(&alloc, &resp);
    return err;
#else
    (void)ctx;
    (void)data;
    (void)len;
    return HU_ERR_NOT_SUPPORTED;
#endif
}

static hu_error_t sse_recv(void *ctx, hu_allocator_t *alloc, char **out, size_t *out_len) {
    (void)ctx;
    (void)alloc;
    (void)out;
    (void)out_len;
    return HU_ERR_NOT_SUPPORTED;
}

static void sse_close(void *ctx, hu_allocator_t *alloc) {
    sse_ctx_t *c = (sse_ctx_t *)ctx;
    if (!c || !alloc)
        return;
    if (c->url) {
        alloc->free(alloc->ctx, c->url, strlen(c->url) + 1);
        c->url = NULL;
    }
    alloc->free(alloc->ctx, c, sizeof(*c));
}

hu_error_t hu_mcp_transport_sse_create(hu_allocator_t *alloc, const char *url, size_t url_len,
                                       hu_mcp_transport_t *out) {
#ifdef HU_HTTP_CURL
    if (!alloc || !out || !url)
        return HU_ERR_INVALID_ARGUMENT;
    sse_ctx_t *c = (sse_ctx_t *)alloc->alloc(alloc->ctx, sizeof(*c));
    if (!c)
        return HU_ERR_OUT_OF_MEMORY;
    c->url = hu_strndup(alloc, url, url_len);
    if (!c->url) {
        alloc->free(alloc->ctx, c, sizeof(*c));
        return HU_ERR_OUT_OF_MEMORY;
    }
    out->ctx = c;
    out->send = sse_send;
    out->recv = sse_recv;
    out->close = sse_close;
    return HU_OK;
#else
    (void)alloc;
    (void)url;
    (void)url_len;
    (void)out;
    return HU_ERR_NOT_SUPPORTED;
#endif
}
