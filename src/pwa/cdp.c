#include "human/pwa/cdp.h"
#include "human/core/string.h"
#include <stdio.h>
#include <string.h>

#if HU_IS_TEST

hu_error_t hu_cdp_connect(hu_allocator_t *alloc, const char *host, uint16_t port,
                          hu_cdp_session_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    out->alloc = alloc;
    out->connected = true;
    out->next_id = 1;

    char url[256];
    int n = snprintf(url, sizeof(url), "ws://%s:%u/devtools/page/mock",
                     host ? host : "localhost", port);
    if (n > 0)
        out->ws_url = hu_strndup(alloc, url, (size_t)n);
    out->ws_url_len = out->ws_url ? (size_t)n : 0;

    char dbg[128];
    int dn = snprintf(dbg, sizeof(dbg), "http://%s:%u", host ? host : "localhost", port);
    if (dn > 0)
        out->debug_url = hu_strndup(alloc, dbg, (size_t)dn);
    out->debug_url_len = out->debug_url ? (size_t)dn : 0;

    return HU_OK;
}

void hu_cdp_disconnect(hu_cdp_session_t *session) {
    if (!session)
        return;
    if (session->ws_url)
        session->alloc->free(session->alloc->ctx, session->ws_url, session->ws_url_len + 1);
    if (session->debug_url)
        session->alloc->free(session->alloc->ctx, session->debug_url, session->debug_url_len + 1);
    session->connected = false;
}

hu_error_t hu_cdp_navigate(hu_cdp_session_t *session, const char *url, size_t url_len) {
    if (!session || !session->connected || !url)
        return HU_ERR_INVALID_ARGUMENT;
    (void)url_len;
    return HU_OK;
}

hu_error_t hu_cdp_evaluate(hu_cdp_session_t *session, const char *expression, size_t expr_len,
                           char **out, size_t *out_len) {
    if (!session || !session->connected || !expression || !out)
        return HU_ERR_INVALID_ARGUMENT;
    (void)expr_len;
    const char *result = "\"mock_eval_result\"";
    *out = hu_strdup(session->alloc, result);
    *out_len = *out ? strlen(*out) : 0;
    return HU_OK;
}

hu_error_t hu_cdp_screenshot(hu_cdp_session_t *session, hu_cdp_screenshot_t *out) {
    if (!session || !session->connected || !out)
        return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    const char *mock_data = "iVBORw0KGgoAAAANSUhEUg==";
    out->data_base64 = hu_strdup(session->alloc, mock_data);
    out->data_len = out->data_base64 ? strlen(out->data_base64) : 0;
    return HU_OK;
}

hu_error_t hu_cdp_click(hu_cdp_session_t *session, int x, int y) {
    if (!session || !session->connected)
        return HU_ERR_INVALID_ARGUMENT;
    (void)x;
    (void)y;
    return HU_OK;
}

hu_error_t hu_cdp_type(hu_cdp_session_t *session, const char *text, size_t text_len) {
    if (!session || !session->connected || !text)
        return HU_ERR_INVALID_ARGUMENT;
    (void)text_len;
    return HU_OK;
}

hu_error_t hu_cdp_get_title(hu_cdp_session_t *session, char **out, size_t *out_len) {
    if (!session || !session->connected || !out)
        return HU_ERR_INVALID_ARGUMENT;
    *out = hu_strdup(session->alloc, "Mock Page Title");
    *out_len = *out ? strlen(*out) : 0;
    return HU_OK;
}

hu_error_t hu_cdp_query_elements(hu_cdp_session_t *session, const char *selector,
                                 size_t selector_len, hu_cdp_element_t *out,
                                 size_t max_out, size_t *out_count) {
    if (!session || !session->connected || !out || !out_count)
        return HU_ERR_INVALID_ARGUMENT;
    (void)selector;
    (void)selector_len;
    if (max_out >= 1) {
        memset(&out[0], 0, sizeof(out[0]));
        out[0].x = 100;
        out[0].y = 200;
        out[0].width = 80;
        out[0].height = 32;
        memcpy(out[0].text, "Submit", 6);
        out[0].text_len = 6;
        memcpy(out[0].tag, "button", 6);
        *out_count = 1;
    } else {
        *out_count = 0;
    }
    return HU_OK;
}

#else /* !HU_IS_TEST */

#ifdef HU_ENABLE_CURL
#include "human/core/http.h"
#include "human/core/json.h"

hu_error_t hu_cdp_connect(hu_allocator_t *alloc, const char *host, uint16_t port,
                          hu_cdp_session_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    out->alloc = alloc;
    out->next_id = 1;

    char url[256];
    int n = snprintf(url, sizeof(url), "http://%s:%u/json/version",
                     host ? host : "localhost", port);
    if (n <= 0)
        return HU_ERR_INVALID_ARGUMENT;

    out->debug_url = hu_sprintf(alloc, "http://%s:%u", host ? host : "localhost", port);
    out->debug_url_len = out->debug_url ? strlen(out->debug_url) : 0;

    hu_http_response_t resp;
    memset(&resp, 0, sizeof(resp));
    hu_error_t err = hu_http_get(alloc, url, NULL, &resp);
    if (err != HU_OK)
        return err;
    if (!resp.body || resp.body_len == 0) {
        hu_http_response_free(alloc, &resp);
        return HU_ERR_NOT_FOUND;
    }

    hu_json_value_t *json = NULL;
    err = hu_json_parse(alloc, resp.body, resp.body_len, &json);
    hu_http_response_free(alloc, &resp);
    if (err != HU_OK)
        return err;

    const char *ws = hu_json_get_string(json, "webSocketDebuggerUrl");
    if (ws) {
        out->ws_url = hu_strdup(alloc, ws);
        out->ws_url_len = strlen(ws);
        out->connected = true;
    }
    hu_json_free(alloc, json);

    return out->connected ? HU_OK : HU_ERR_NOT_FOUND;
}

#else /* !HU_ENABLE_CURL */

hu_error_t hu_cdp_connect(hu_allocator_t *alloc, const char *host, uint16_t port,
                          hu_cdp_session_t *out) {
    (void)alloc; (void)host; (void)port;
    if (out) memset(out, 0, sizeof(*out));
    return HU_ERR_NOT_SUPPORTED;
}

#endif /* HU_ENABLE_CURL */

void hu_cdp_disconnect(hu_cdp_session_t *session) {
    if (!session)
        return;
    if (session->ws_url)
        session->alloc->free(session->alloc->ctx, session->ws_url, session->ws_url_len + 1);
    if (session->debug_url)
        session->alloc->free(session->alloc->ctx, session->debug_url, session->debug_url_len + 1);
    session->connected = false;
}

hu_error_t hu_cdp_navigate(hu_cdp_session_t *s, const char *u, size_t l) {
    (void)s; (void)u; (void)l; return HU_ERR_NOT_SUPPORTED;
}
hu_error_t hu_cdp_evaluate(hu_cdp_session_t *s, const char *e, size_t l,
                           char **o, size_t *ol) {
    (void)s; (void)e; (void)l; (void)o; (void)ol; return HU_ERR_NOT_SUPPORTED;
}
hu_error_t hu_cdp_screenshot(hu_cdp_session_t *s, hu_cdp_screenshot_t *o) {
    (void)s; (void)o; return HU_ERR_NOT_SUPPORTED;
}
hu_error_t hu_cdp_click(hu_cdp_session_t *s, int x, int y) {
    (void)s; (void)x; (void)y; return HU_ERR_NOT_SUPPORTED;
}
hu_error_t hu_cdp_type(hu_cdp_session_t *s, const char *t, size_t l) {
    (void)s; (void)t; (void)l; return HU_ERR_NOT_SUPPORTED;
}
hu_error_t hu_cdp_get_title(hu_cdp_session_t *s, char **o, size_t *ol) {
    (void)s; (void)o; (void)ol; return HU_ERR_NOT_SUPPORTED;
}
hu_error_t hu_cdp_query_elements(hu_cdp_session_t *s, const char *sel, size_t sl,
                                 hu_cdp_element_t *o, size_t m, size_t *c) {
    (void)s; (void)sel; (void)sl; (void)o; (void)m; (void)c; return HU_ERR_NOT_SUPPORTED;
}

#endif /* HU_IS_TEST */
