#include "human/core/string.h"
#include "human/mcp_transport.h"
#include <string.h>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

typedef struct stdio_ctx {
    int read_fd;
    int write_fd;
} stdio_ctx_t;

static hu_error_t stdio_send(void *ctx, const char *data, size_t len) {
    stdio_ctx_t *c = (stdio_ctx_t *)ctx;
    if (!c || !data)
        return HU_ERR_INVALID_ARGUMENT;
#if defined(__unix__) || defined(__APPLE__)
    size_t total = 0;
    while (total < len) {
        ssize_t n = write(c->write_fd, data + total, len - total);
        if (n < 0)
            return HU_ERR_IO;
        total += (size_t)n;
    }
    if (write(c->write_fd, "\n", 1) != 1)
        return HU_ERR_IO;
    return HU_OK;
#else
    (void)len;
    (void)c;
    return HU_ERR_NOT_SUPPORTED;
#endif
}

static hu_error_t stdio_recv(void *ctx, hu_allocator_t *alloc, char **out, size_t *out_len) {
    stdio_ctx_t *c = (stdio_ctx_t *)ctx;
    if (!c || !alloc || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
#if defined(__unix__) || defined(__APPLE__)
    size_t cap = 256;
    char *buf = (char *)alloc->alloc(alloc->ctx, cap);
    if (!buf)
        return HU_ERR_OUT_OF_MEMORY;
    size_t len = 0;
    for (;;) {
        if (len >= cap - 1) {
            size_t new_cap = cap * 2;
            char *nbuf = (char *)alloc->realloc(alloc->ctx, buf, cap, new_cap);
            if (!nbuf) {
                alloc->free(alloc->ctx, buf, cap);
                return HU_ERR_OUT_OF_MEMORY;
            }
            buf = nbuf;
            cap = new_cap;
        }
        unsigned char ch;
        ssize_t n = read(c->read_fd, &ch, 1);
        if (n <= 0)
            break;
        if (ch == '\n')
            break;
        buf[len++] = (char)ch;
    }
    buf[len] = '\0';
    *out = buf;
    *out_len = len;
    return HU_OK;
#else
    (void)c;
    (void)alloc;
    (void)out;
    (void)out_len;
    return HU_ERR_NOT_SUPPORTED;
#endif
}

static void stdio_close(void *ctx, hu_allocator_t *alloc) {
    if (!ctx || !alloc)
        return;
    alloc->free(alloc->ctx, ctx, sizeof(stdio_ctx_t));
}

hu_error_t hu_mcp_transport_stdio_create(hu_allocator_t *alloc, int read_fd, int write_fd,
                                         hu_mcp_transport_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    stdio_ctx_t *c = (stdio_ctx_t *)alloc->alloc(alloc->ctx, sizeof(*c));
    if (!c)
        return HU_ERR_OUT_OF_MEMORY;
    c->read_fd = read_fd;
    c->write_fd = write_fd;
    out->ctx = c;
    out->send = stdio_send;
    out->recv = stdio_recv;
    out->close = stdio_close;
    return HU_OK;
}

void hu_mcp_transport_destroy(hu_mcp_transport_t *t, hu_allocator_t *alloc) {
    if (!t || !alloc)
        return;
    if (t->close && t->ctx) {
        t->close(t->ctx, alloc);
    }
    t->ctx = NULL;
    t->send = NULL;
    t->recv = NULL;
    t->close = NULL;
}
