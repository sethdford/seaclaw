/*
 * Multi-tenant BFF memory — store/recall/list/forget via HTTPS JSON API.
 * Configure: BFF_BASE_URL (e.g. https://api.example.com), BFF_AUTH_TOKEN (Firebase ID token),
 * optional BFF_TENANT_ID (X-Tenant-ID when user has multiple tenants).
 */
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/http.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include "human/tool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HU_BFF_MEMORY_NAME "bff_memory"
#define HU_BFF_MEMORY_DESC                                                                 \
    "Cloud memory API (multi-tenant BFF). Actions: store, recall, list, forget. Requires " \
    "env BFF_BASE_URL and BFF_AUTH_TOKEN (Firebase ID token); optional BFF_TENANT_ID for " \
    "X-Tenant-ID. recall uses vector search when the BFF supports embeddings."
#define HU_BFF_MEMORY_PARAMS                                                                    \
    "{\"type\":\"object\",\"properties\":{\"action\":{\"type\":\"string\",\"enum\":[\"store\"," \
    "\"recall\",\"list\",\"forget\"]},\"key\":{\"type\":\"string\"},\"content\":{\"type\":"     \
    "\"string\"},\"query\":{\"type\":\"string\"},\"session_id\":{\"type\":\"string\"},"          \
    "\"limit\":{\"type\":\"number\"},\"category\":{\"type\":\"string\"}},\"required\":["       \
    "\"action\"]}"

typedef struct {
    char _unused;
} bff_memory_ctx_t;

#if !HU_IS_TEST
static void bff_strip_trailing_slash(char *s) {
    size_t n = strlen(s);
    while (n > 0 && s[n - 1] == '/')
        s[--n] = '\0';
}

static const char *bff_auth_bearer(void) {
    static char auth[4096];
    const char *tok = getenv("BFF_AUTH_TOKEN");
    if (!tok || !tok[0])
        return NULL;
    snprintf(auth, sizeof(auth), "Bearer %s", tok);
    return auth;
}

static const char *bff_base(void) {
    const char *b = getenv("BFF_BASE_URL");
    if (!b || !b[0])
        return NULL;
    return b;
}
#endif

static hu_error_t bff_memory_execute(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args,
                                     hu_tool_result_t *out) {
    (void)ctx;
    if (!args || !out) {
        *out = hu_tool_result_fail("invalid args", 12);
        return HU_ERR_INVALID_ARGUMENT;
    }
    const char *action = hu_json_get_string(args, "action");
    if (!action) {
        *out = hu_tool_result_fail("missing action", 14);
        return HU_OK;
    }

#if HU_IS_TEST
    (void)alloc;
    if (strcmp(action, "store") == 0)
        *out = hu_tool_result_ok("{\"ok\":true,\"chunk_id\":\"test\"}", 32);
    else if (strcmp(action, "recall") == 0)
        *out = hu_tool_result_ok("{\"rows\":[]}", 11);
    else if (strcmp(action, "list") == 0)
        *out = hu_tool_result_ok("{\"rows\":[]}", 11);
    else if (strcmp(action, "forget") == 0)
        *out = hu_tool_result_ok("{\"ok\":true}", 11);
    else
        *out = hu_tool_result_fail("unknown action", 14);
    return HU_OK;
#else
    const char *base = bff_base();
    const char *auth = bff_auth_bearer();
    if (!base || !auth) {
        *out = hu_tool_result_fail("set BFF_BASE_URL and BFF_AUTH_TOKEN", 35);
        return HU_OK;
    }
    char base_buf[512];
    strncpy(base_buf, base, sizeof(base_buf) - 1);
    base_buf[sizeof(base_buf) - 1] = '\0';
    bff_strip_trailing_slash(base_buf);

    const char *tenant = getenv("BFF_TENANT_ID");
    char xtenant[256];
    const char *post_extra = NULL;
    if (tenant && tenant[0]) {
        snprintf(xtenant, sizeof(xtenant), "X-Tenant-ID: %s\n", tenant);
        post_extra = xtenant;
    }
    char all_hdr[3072];
    int ah = snprintf(all_hdr, sizeof(all_hdr), "Authorization: %s\n", auth);
    if (tenant && tenant[0])
        ah += snprintf(all_hdr + ah, sizeof(all_hdr) - (size_t)ah, "X-Tenant-ID: %s\n", tenant);

    if (strcmp(action, "store") == 0) {
        const char *key = hu_json_get_string(args, "key");
        const char *content = hu_json_get_string(args, "content");
        if (!key || !content) {
            *out = hu_tool_result_fail("key and content required", 24);
            return HU_OK;
        }
        const char *sid = hu_json_get_string(args, "session_id");
        const char *cat = hu_json_get_string(args, "category");
        hu_json_buf_t jbuf = {0};
        if (hu_json_buf_init(&jbuf, alloc) != HU_OK) {
            *out = hu_tool_result_fail("out of memory", 12);
            return HU_ERR_OUT_OF_MEMORY;
        }
        hu_json_buf_append_raw(&jbuf, "{\"key\":", 7);
        hu_json_append_string(&jbuf, key, strlen(key));
        hu_json_buf_append_raw(&jbuf, ",\"content\":", 11);
        hu_json_append_string(&jbuf, content, strlen(content));
        if (sid && sid[0]) {
            hu_json_buf_append_raw(&jbuf, ",\"session_id\":", 14);
            hu_json_append_string(&jbuf, sid, strlen(sid));
        }
        if (cat && cat[0]) {
            hu_json_buf_append_raw(&jbuf, ",\"category\":", 12);
            hu_json_append_string(&jbuf, cat, strlen(cat));
        }
        hu_json_buf_append_raw(&jbuf, "}", 1);
        char *jb = jbuf.ptr;
        size_t cap = jbuf.len;
        char url[768];
        snprintf(url, sizeof(url), "%s/v1/memory/store", base_buf);
        hu_http_response_t resp = {0};
        hu_error_t err =
            hu_http_post_json_ex(alloc, url, auth, post_extra, jb, cap, &resp);
        hu_json_buf_free(&jbuf);
        if (err != HU_OK) {
            if (resp.owned && resp.body)
                hu_http_response_free(alloc, &resp);
            *out = hu_tool_result_fail("http request failed", 19);
            return HU_OK;
        }
        char *rb = hu_strndup(alloc, resp.body, resp.body_len);
        hu_http_response_free(alloc, &resp);
        if (!rb) {
            *out = hu_tool_result_fail("out of memory copying response", 30);
            return HU_OK;
        }
        *out = hu_tool_result_ok_owned(rb, strlen(rb));
        return HU_OK;
    }

    if (strcmp(action, "recall") == 0) {
        const char *query = hu_json_get_string(args, "query");
        if (!query) {
            *out = hu_tool_result_fail("query required for recall", 25);
            return HU_OK;
        }
        long lim = (long)hu_json_get_number(args, "limit", 20);
        const char *sid = hu_json_get_string(args, "session_id");
        hu_json_buf_t jbuf = {0};
        if (hu_json_buf_init(&jbuf, alloc) != HU_OK) {
            *out = hu_tool_result_fail("out of memory", 12);
            return HU_ERR_OUT_OF_MEMORY;
        }
        hu_json_buf_append_raw(&jbuf, "{\"query\":", 9);
        hu_json_append_string(&jbuf, query, strlen(query));
        char lim_str[32];
        int ln = snprintf(lim_str, sizeof(lim_str), ",\"limit\":%ld", lim);
        if (ln > 0)
            hu_json_buf_append_raw(&jbuf, lim_str, (size_t)ln);
        if (sid && sid[0]) {
            hu_json_buf_append_raw(&jbuf, ",\"session_id\":", 14);
            hu_json_append_string(&jbuf, sid, strlen(sid));
        }
        hu_json_buf_append_raw(&jbuf, "}", 1);
        char *jb = jbuf.ptr;
        size_t qcap = jbuf.len;
        char url[768];
        snprintf(url, sizeof(url), "%s/v1/memory/recall", base_buf);
        hu_http_response_t resp = {0};
        hu_error_t err =
            hu_http_post_json_ex(alloc, url, auth, post_extra, jb, qcap, &resp);
        hu_json_buf_free(&jbuf);
        if (err != HU_OK || resp.status_code < 200 || resp.status_code >= 300) {
            if (resp.owned && resp.body)
                hu_http_response_free(alloc, &resp);
            *out = hu_tool_result_fail("recall failed", 13);
            return HU_OK;
        }
        char *rb = hu_strndup(alloc, resp.body, resp.body_len);
        hu_http_response_free(alloc, &resp);
        if (!rb) {
            *out = hu_tool_result_fail("out of memory copying response", 30);
            return HU_OK;
        }
        *out = hu_tool_result_ok_owned(rb, strlen(rb));
        return HU_OK;
    }

    if (strcmp(action, "list") == 0) {
        const char *sid = hu_json_get_string(args, "session_id");
        char url[768];
        if (sid && sid[0])
            snprintf(url, sizeof(url), "%s/v1/memory/list?session_id=%s", base_buf, sid);
        else
            snprintf(url, sizeof(url), "%s/v1/memory/list", base_buf);
        hu_http_response_t resp = {0};
        hu_error_t err = hu_http_get_ex(alloc, url, all_hdr, &resp);
        (void)ah;
        if (err != HU_OK || resp.status_code < 200 || resp.status_code >= 300) {
            if (resp.owned && resp.body)
                hu_http_response_free(alloc, &resp);
            *out = hu_tool_result_fail("list failed", 11);
            return HU_OK;
        }
        char *rb = hu_strndup(alloc, resp.body, resp.body_len);
        hu_http_response_free(alloc, &resp);
        if (!rb) {
            *out = hu_tool_result_fail("out of memory copying response", 30);
            return HU_OK;
        }
        *out = hu_tool_result_ok_owned(rb, strlen(rb));
        return HU_OK;
    }

    if (strcmp(action, "forget") == 0) {
        const char *key = hu_json_get_string(args, "key");
        if (!key) {
            *out = hu_tool_result_fail("key required for forget", 23);
            return HU_OK;
        }
        char url[1024];
        snprintf(url, sizeof(url), "%s/v1/memory/forget?key=%s", base_buf, key);
        hu_http_response_t resp = {0};
        hu_error_t err = hu_http_request(alloc, url, "DELETE", all_hdr, NULL, 0, &resp);
        if (err != HU_OK || resp.status_code < 200 || resp.status_code >= 300) {
            if (resp.owned && resp.body)
                hu_http_response_free(alloc, &resp);
            *out = hu_tool_result_fail("forget failed", 13);
            return HU_OK;
        }
        char *rb = hu_strndup(alloc, resp.body, resp.body_len);
        hu_http_response_free(alloc, &resp);
        if (!rb) {
            *out = hu_tool_result_fail("out of memory copying response", 30);
            return HU_OK;
        }
        *out = hu_tool_result_ok_owned(rb, strlen(rb));
        return HU_OK;
    }

    *out = hu_tool_result_fail("unknown action", 14);
    return HU_OK;
#endif
}

static const char *bff_memory_name(void *ctx) {
    (void)ctx;
    return HU_BFF_MEMORY_NAME;
}
static const char *bff_memory_description(void *ctx) {
    (void)ctx;
    return HU_BFF_MEMORY_DESC;
}
static const char *bff_memory_parameters_json(void *ctx) {
    (void)ctx;
    return HU_BFF_MEMORY_PARAMS;
}
static void bff_memory_deinit(void *ctx, hu_allocator_t *alloc) {
    if (ctx && alloc)
        alloc->free(alloc->ctx, ctx, sizeof(bff_memory_ctx_t));
}

static const hu_tool_vtable_t bff_memory_vtable = {
    .execute = bff_memory_execute,
    .name = bff_memory_name,
    .description = bff_memory_description,
    .parameters_json = bff_memory_parameters_json,
    .deinit = bff_memory_deinit,
};

hu_error_t hu_bff_memory_create(hu_allocator_t *alloc, hu_tool_t *out) {
    bff_memory_ctx_t *ctx = (bff_memory_ctx_t *)alloc->alloc(alloc->ctx, sizeof(*ctx));
    if (!ctx)
        return HU_ERR_OUT_OF_MEMORY;
    memset(ctx, 0, sizeof(*ctx));
    out->ctx = ctx;
    out->vtable = &bff_memory_vtable;
    return HU_OK;
}
