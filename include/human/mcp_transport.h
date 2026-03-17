#ifndef HU_MCP_TRANSPORT_H
#define HU_MCP_TRANSPORT_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>

typedef enum hu_mcp_transport_type {
    HU_MCP_TRANSPORT_STDIO,
    HU_MCP_TRANSPORT_SSE,
    HU_MCP_TRANSPORT_HTTP,
} hu_mcp_transport_type_t;

typedef struct hu_mcp_transport {
    void *ctx;
    hu_error_t (*send)(void *ctx, const char *data, size_t len);
    hu_error_t (*recv)(void *ctx, hu_allocator_t *alloc, char **out, size_t *out_len);
    void (*close)(void *ctx, hu_allocator_t *alloc);
} hu_mcp_transport_t;

hu_error_t hu_mcp_transport_stdio_create(hu_allocator_t *alloc, int read_fd, int write_fd,
                                          hu_mcp_transport_t *out);

/**
 * Create stdio transport from command — stores command for later spawn.
 * Call hu_mcp_transport_stdio_start to fork/exec the child. Requires HU_GATEWAY_POSIX.
 */
hu_error_t hu_mcp_transport_stdio_create_from_command(hu_allocator_t *alloc, const char *command,
                                                      const char *const *args, size_t args_count,
                                                      hu_mcp_transport_t *out);

/**
 * Start the child process (only for transport created from command).
 * Returns HU_ERR_NOT_SUPPORTED in HU_IS_TEST. Requires HU_GATEWAY_POSIX.
 */
hu_error_t hu_mcp_transport_stdio_start(hu_allocator_t *alloc, hu_mcp_transport_t *t);

hu_error_t hu_mcp_transport_sse_create(hu_allocator_t *alloc, const char *url, size_t url_len,
                                       hu_mcp_transport_t *out);
hu_error_t hu_mcp_transport_http_create(hu_allocator_t *alloc, const char *url, size_t url_len,
                                        hu_mcp_transport_t *out);
void hu_mcp_transport_destroy(hu_mcp_transport_t *t, hu_allocator_t *alloc);

#endif
