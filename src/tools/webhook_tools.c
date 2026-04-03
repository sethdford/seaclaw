#include "human/tools/webhook_tools.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ──────────────────────────────────────────────────────────────────────────
 * webhook_register tool
 * ────────────────────────────────────────────────────────────────────────── */

#define WEBHOOK_REGISTER_NAME "webhook_register"
#define WEBHOOK_REGISTER_DESC "Register a webhook endpoint to receive events"
#define WEBHOOK_REGISTER_PARAMS                                           \
    "{\"type\":\"object\",\"properties\":{"                               \
    "\"path\":{\"type\":\"string\"}"                                      \
    "},\"required\":[\"path\"]}"

typedef struct {
    hu_allocator_t *alloc;
    hu_webhook_manager_t *mgr;
} webhook_register_ctx_t;

static hu_error_t webhook_register_execute(void *ctx, hu_allocator_t *alloc,
                                           const hu_json_value_t *args, hu_tool_result_t *out) {
    if (!out)
        return HU_ERR_INVALID_ARGUMENT;
    if (!args) {
        *out = hu_tool_result_fail("invalid args", 12);
        return HU_ERR_INVALID_ARGUMENT;
    }

    const char *path = hu_json_get_string(args, "path");
    if (!path) {
        *out = hu_tool_result_fail("missing path", 12);
        return HU_OK;
    }

#if HU_IS_TEST
    (void)ctx;
    char *result = hu_sprintf(alloc, "{\"id\":\"webhook_test_12345\"}");
    *out = hu_tool_result_ok_owned(result, result ? strlen(result) : 0);
    return HU_OK;
#else
    webhook_register_ctx_t *c = (webhook_register_ctx_t *)ctx;
    char *webhook_id = NULL;
    hu_error_t err = hu_webhook_register(alloc, c->mgr, path, &webhook_id);
    if (err != HU_OK) {
        *out = hu_tool_result_fail("failed to register webhook", 25);
        return HU_OK;
    }

    char *result = hu_sprintf(alloc, "{\"id\":\"%s\"}", webhook_id);
    if (!result) {
        hu_webhook_unregister(alloc, c->mgr, webhook_id);
        alloc->free(alloc->ctx, webhook_id, strlen(webhook_id) + 1);
        return HU_ERR_OUT_OF_MEMORY;
    }
    *out = hu_tool_result_ok_owned(result, strlen(result));
    return HU_OK;
#endif
}

static const char *webhook_register_name(void *ctx) {
    (void)ctx;
    return WEBHOOK_REGISTER_NAME;
}

static const char *webhook_register_desc(void *ctx) {
    (void)ctx;
    return WEBHOOK_REGISTER_DESC;
}

static const char *webhook_register_params(void *ctx) {
    (void)ctx;
    return WEBHOOK_REGISTER_PARAMS;
}

static void webhook_register_deinit(void *ctx, hu_allocator_t *alloc) {
    webhook_register_ctx_t *c = (webhook_register_ctx_t *)ctx;
    if (!c)
        return;
    alloc->free(alloc->ctx, c, sizeof(*c));
}

static const hu_tool_vtable_t webhook_register_vtable = {
    .execute = webhook_register_execute,
    .name = webhook_register_name,
    .description = webhook_register_desc,
    .parameters_json = webhook_register_params,
    .deinit = webhook_register_deinit,
};

hu_error_t hu_webhook_register_tool_create(hu_allocator_t *alloc, hu_webhook_manager_t *mgr,
                                           hu_tool_t *out) {
    if (!alloc || !mgr || !out)
        return HU_ERR_INVALID_ARGUMENT;
    webhook_register_ctx_t *c =
        (webhook_register_ctx_t *)alloc->alloc(alloc->ctx, sizeof(*c));
    if (!c)
        return HU_ERR_OUT_OF_MEMORY;
    c->alloc = alloc;
    c->mgr = mgr;
    out->ctx = c;
    out->vtable = &webhook_register_vtable;
    return HU_OK;
}

/* ──────────────────────────────────────────────────────────────────────────
 * webhook_poll tool
 * ────────────────────────────────────────────────────────────────────────── */

#define WEBHOOK_POLL_NAME "webhook_poll"
#define WEBHOOK_POLL_DESC "Poll for events on a registered webhook"
#define WEBHOOK_POLL_PARAMS                                               \
    "{\"type\":\"object\",\"properties\":{"                               \
    "\"id\":{\"type\":\"string\"}"                                        \
    "},\"required\":[\"id\"]}"

typedef struct {
    hu_allocator_t *alloc;
    hu_webhook_manager_t *mgr;
} webhook_poll_ctx_t;

static hu_error_t webhook_poll_execute(void *ctx, hu_allocator_t *alloc,
                                       const hu_json_value_t *args, hu_tool_result_t *out) {
    if (!out)
        return HU_ERR_INVALID_ARGUMENT;
    if (!args) {
        *out = hu_tool_result_fail("invalid args", 12);
        return HU_ERR_INVALID_ARGUMENT;
    }

    const char *webhook_id = hu_json_get_string(args, "id");
    if (!webhook_id) {
        *out = hu_tool_result_fail("missing id", 10);
        return HU_OK;
    }

#if HU_IS_TEST
    (void)ctx;
    char *result = hu_sprintf(alloc, "{\"events\":[]}");
    *out = hu_tool_result_ok_owned(result, result ? strlen(result) : 0);
    return HU_OK;
#else
    webhook_poll_ctx_t *c = (webhook_poll_ctx_t *)ctx;
    hu_webhook_event_t *events = NULL;
    size_t event_count = 0;
    hu_error_t err = hu_webhook_poll(alloc, c->mgr, webhook_id, &events, &event_count);
    if (err != HU_OK) {
        *out = hu_tool_result_fail("webhook not found", 16);
        return HU_OK;
    }

    size_t buf_cap = 4096;
    char *buf = (char *)alloc->alloc(alloc->ctx, buf_cap);
    if (!buf) {
        hu_webhook_free_events(alloc, events, event_count);
        return HU_ERR_OUT_OF_MEMORY;
    }

    size_t off = 0;
    off += (size_t)snprintf(buf + off, buf_cap - off, "{\"events\":[");
    int first = 1;

    for (size_t i = 0; i < event_count; i++) {
        if (!first && off < buf_cap - 2)
            buf[off++] = ',';
        off += (size_t)snprintf(buf + off, buf_cap - off - 1,
                                "{\"data\":\"%s\",\"received_at\":%ld}", events[i].event_data,
                                (long)events[i].received_at);
        first = 0;
    }

    off += (size_t)snprintf(buf + off, buf_cap - off - 1, "]}");
    *out = hu_tool_result_ok_owned(buf, off);
    hu_webhook_free_events(alloc, events, event_count);
    return HU_OK;
#endif
}

static const char *webhook_poll_name(void *ctx) {
    (void)ctx;
    return WEBHOOK_POLL_NAME;
}

static const char *webhook_poll_desc(void *ctx) {
    (void)ctx;
    return WEBHOOK_POLL_DESC;
}

static const char *webhook_poll_params(void *ctx) {
    (void)ctx;
    return WEBHOOK_POLL_PARAMS;
}

static void webhook_poll_deinit(void *ctx, hu_allocator_t *alloc) {
    webhook_poll_ctx_t *c = (webhook_poll_ctx_t *)ctx;
    if (!c)
        return;
    alloc->free(alloc->ctx, c, sizeof(*c));
}

static const hu_tool_vtable_t webhook_poll_vtable = {
    .execute = webhook_poll_execute,
    .name = webhook_poll_name,
    .description = webhook_poll_desc,
    .parameters_json = webhook_poll_params,
    .deinit = webhook_poll_deinit,
};

hu_error_t hu_webhook_poll_tool_create(hu_allocator_t *alloc, hu_webhook_manager_t *mgr,
                                       hu_tool_t *out) {
    if (!alloc || !mgr || !out)
        return HU_ERR_INVALID_ARGUMENT;
    webhook_poll_ctx_t *c = (webhook_poll_ctx_t *)alloc->alloc(alloc->ctx, sizeof(*c));
    if (!c)
        return HU_ERR_OUT_OF_MEMORY;
    c->alloc = alloc;
    c->mgr = mgr;
    out->ctx = c;
    out->vtable = &webhook_poll_vtable;
    return HU_OK;
}

/* ──────────────────────────────────────────────────────────────────────────
 * webhook_list tool
 * ────────────────────────────────────────────────────────────────────────── */

#define WEBHOOK_LIST_NAME "webhook_list"
#define WEBHOOK_LIST_DESC "List all registered webhooks"
#define WEBHOOK_LIST_PARAMS "{\"type\":\"object\",\"properties\":{}}"

typedef struct {
    hu_allocator_t *alloc;
    hu_webhook_manager_t *mgr;
} webhook_list_ctx_t;

static hu_error_t webhook_list_execute(void *ctx, hu_allocator_t *alloc,
                                       const hu_json_value_t *args, hu_tool_result_t *out) {
    (void)args;
    if (!out)
        return HU_ERR_INVALID_ARGUMENT;

#if HU_IS_TEST
    (void)ctx;
    char *result = hu_sprintf(alloc, "{\"webhooks\":[]}");
    *out = hu_tool_result_ok_owned(result, result ? strlen(result) : 0);
    return HU_OK;
#else
    webhook_list_ctx_t *c = (webhook_list_ctx_t *)ctx;
    hu_webhook_t *webhooks = NULL;
    size_t webhook_count = 0;
    hu_error_t err = hu_webhook_list(alloc, c->mgr, &webhooks, &webhook_count);
    if (err != HU_OK) {
        *out = hu_tool_result_fail("failed to list webhooks", 23);
        return HU_OK;
    }

    size_t buf_cap = 8192;
    char *buf = (char *)alloc->alloc(alloc->ctx, buf_cap);
    if (!buf) {
        hu_webhook_free_webhooks(alloc, webhooks, webhook_count);
        return HU_ERR_OUT_OF_MEMORY;
    }

    size_t off = 0;
    off += (size_t)snprintf(buf + off, buf_cap - off, "{\"webhooks\":[");
    int first = 1;

    for (size_t i = 0; i < webhook_count; i++) {
        if (!first && off < buf_cap - 2)
            buf[off++] = ',';
        off += (size_t)snprintf(buf + off, buf_cap - off - 1,
                                "{\"id\":\"%s\",\"path\":\"%s\",\"registered_at\":%ld}",
                                webhooks[i].id, webhooks[i].path, (long)webhooks[i].registered_at);
        first = 0;
    }

    off += (size_t)snprintf(buf + off, buf_cap - off - 1, "]}");
    *out = hu_tool_result_ok_owned(buf, off);
    hu_webhook_free_webhooks(alloc, webhooks, webhook_count);
    return HU_OK;
#endif
}

static const char *webhook_list_name(void *ctx) {
    (void)ctx;
    return WEBHOOK_LIST_NAME;
}

static const char *webhook_list_desc(void *ctx) {
    (void)ctx;
    return WEBHOOK_LIST_DESC;
}

static const char *webhook_list_params(void *ctx) {
    (void)ctx;
    return WEBHOOK_LIST_PARAMS;
}

static void webhook_list_deinit(void *ctx, hu_allocator_t *alloc) {
    webhook_list_ctx_t *c = (webhook_list_ctx_t *)ctx;
    if (!c)
        return;
    alloc->free(alloc->ctx, c, sizeof(*c));
}

static const hu_tool_vtable_t webhook_list_vtable = {
    .execute = webhook_list_execute,
    .name = webhook_list_name,
    .description = webhook_list_desc,
    .parameters_json = webhook_list_params,
    .deinit = webhook_list_deinit,
};

hu_error_t hu_webhook_list_tool_create(hu_allocator_t *alloc, hu_webhook_manager_t *mgr,
                                       hu_tool_t *out) {
    if (!alloc || !mgr || !out)
        return HU_ERR_INVALID_ARGUMENT;
    webhook_list_ctx_t *c = (webhook_list_ctx_t *)alloc->alloc(alloc->ctx, sizeof(*c));
    if (!c)
        return HU_ERR_OUT_OF_MEMORY;
    c->alloc = alloc;
    c->mgr = mgr;
    out->ctx = c;
    out->vtable = &webhook_list_vtable;
    return HU_OK;
}
