#ifndef HU_WEBSOCKET_H
#define HU_WEBSOCKET_H

#define HU_WS_OP_CONTINUATION 0
#define HU_WS_OP_TEXT         1
#define HU_WS_OP_BINARY       2
#define HU_WS_OP_CLOSE        8
#define HU_WS_OP_PING         9
#define HU_WS_OP_PONG         10

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>

typedef struct hu_ws_client hu_ws_client_t;

hu_error_t hu_ws_connect(hu_allocator_t *alloc, const char *url, hu_ws_client_t **out);

/* extra_headers: "Header: value\r\n..." or NULL — appended after Sec-WebSocket-Version before final blank line */
hu_error_t hu_ws_connect_with_headers(hu_allocator_t *alloc, const char *url,
                                      const char *extra_headers, hu_ws_client_t **out);

hu_error_t hu_ws_send(hu_ws_client_t *ws, const char *data, size_t data_len);

/* timeout_ms: -1 = block until a data frame arrives; 0 = poll once; >0 = wait up to N ms per frame */
hu_error_t hu_ws_recv(hu_ws_client_t *ws, hu_allocator_t *alloc, char **data_out,
                      size_t *data_len_out, int timeout_ms);

void hu_ws_close(hu_ws_client_t *ws, hu_allocator_t *alloc);

/* Close and free the client; use after hu_ws_connect when done */
void hu_ws_client_free(hu_ws_client_t *ws, hu_allocator_t *alloc);

/* Frame helpers (for testing and custom use) */
size_t hu_ws_build_frame(char *buf, size_t buf_size, unsigned opcode, const char *payload,
                         size_t payload_len, const unsigned char mask_key[4]);

void hu_ws_apply_mask(char *payload, size_t len, const unsigned char mask_key[4]);

typedef struct {
    unsigned opcode;
    int fin;
    int masked;
    unsigned long long payload_len;
    size_t header_bytes;
} hu_ws_parsed_header_t;

int hu_ws_parse_header(const char *bytes, size_t bytes_len, hu_ws_parsed_header_t *out);

#endif
