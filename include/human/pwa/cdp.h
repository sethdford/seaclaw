#ifndef HU_PWA_CDP_H
#define HU_PWA_CDP_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct hu_ws_client hu_ws_client_t;

typedef struct hu_cdp_session {
    hu_allocator_t *alloc;
    char *ws_url;
    size_t ws_url_len;
    int32_t next_id;
    bool connected;
    char *debug_url;
    size_t debug_url_len;
    hu_ws_client_t *ws;
} hu_cdp_session_t;

typedef struct hu_cdp_screenshot {
    char *data_base64;
    size_t data_len;
} hu_cdp_screenshot_t;

typedef struct hu_cdp_element {
    int x, y, width, height;
    char text[256];
    size_t text_len;
    char tag[32];
} hu_cdp_element_t;

hu_error_t hu_cdp_connect(hu_allocator_t *alloc, const char *host, uint16_t port,
                          hu_cdp_session_t *out);
void hu_cdp_disconnect(hu_cdp_session_t *session);
hu_error_t hu_cdp_navigate(hu_cdp_session_t *session, const char *url, size_t url_len);
hu_error_t hu_cdp_evaluate(hu_cdp_session_t *session, const char *expression, size_t expr_len,
                           char **out, size_t *out_len);
hu_error_t hu_cdp_screenshot(hu_cdp_session_t *session, hu_cdp_screenshot_t *out);
hu_error_t hu_cdp_click(hu_cdp_session_t *session, int x, int y);
hu_error_t hu_cdp_type(hu_cdp_session_t *session, const char *text, size_t text_len);
hu_error_t hu_cdp_get_title(hu_cdp_session_t *session, char **out, size_t *out_len);
hu_error_t hu_cdp_query_elements(hu_cdp_session_t *session, const char *selector,
                                 size_t selector_len, hu_cdp_element_t *out,
                                 size_t max_out, size_t *out_count);

#endif /* HU_PWA_CDP_H */
