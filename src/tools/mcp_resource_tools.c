#include "human/tools/mcp_resource_tools.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include <stdio.h>
#include <string.h>

/* ──────────────────────────────────────────────────────────────────────────
 * list_mcp_resources tool
 * ────────────────────────────────────────────────────────────────────────── */

#define LIST_RESOURCES_NAME "list_mcp_resources"
#define LIST_RESOURCES_DESC "List available resources from MCP servers"
#define LIST_RESOURCES_PARAMS                                                                 \
    "{\"type\":\"object\",\"properties\":{" \
    "\"server\":{\"type\":\"string\",\"description\":\"Optional server name to filter\"}" \
    "}}"

typedef struct {
    hu_allocator_t *alloc;
} mcp_resource_list_ctx_t;

static hu_error_t mcp_resource_list_execute(void *ctx, hu_allocator_t *alloc,
                                            const hu_json_value_t *args, hu_tool_result_t *out) {
    mcp_resource_list_ctx_t *c = (mcp_resource_list_ctx_t *)ctx;
    (void)c;
    (void)alloc;
    if (!out)
        return HU_ERR_INVALID_ARGUMENT;
    if (!args) {
        *out = hu_tool_result_fail("invalid args", 12);
        return HU_ERR_INVALID_ARGUMENT;
    }

    const char *server = hu_json_get_string(args, "server");
    (void)server;  /* Used in HU_IS_TEST block */

#if HU_IS_TEST
    /* Test response with mock MCP resources */
    char *response = hu_sprintf(
        alloc,
        "{\"resources\":["
        "{\"server\":\"filesystem\",\"uri\":\"file:///etc/passwd\",\"name\":\"passwd\",\"description\":\"System password file\",\"mimeType\":\"text/plain\"},"
        "{\"server\":\"filesystem\",\"uri\":\"file:///etc/hosts\",\"name\":\"hosts\",\"description\":\"System hosts file\",\"mimeType\":\"text/plain\"}"
        "]}");
    if (!response)
        return HU_ERR_OUT_OF_MEMORY;
    *out = hu_tool_result_ok_owned(response, strlen(response));
#else
    /* In production, would call MCP manager to list resources from servers.
     * For now, return informational message. */
    const char *msg = server ? "{\"resources\":[],\"note\":\"Resource listing requires MCP manager integration\"}"
                             : "{\"resources\":[],\"note\":\"No server specified and MCP manager not available\"}";
    *out = hu_tool_result_ok(msg, strlen(msg));
#endif
    return HU_OK;
}

static const char *mcp_resource_list_name(void *ctx) {
    (void)ctx;
    return LIST_RESOURCES_NAME;
}
static const char *mcp_resource_list_desc(void *ctx) {
    (void)ctx;
    return LIST_RESOURCES_DESC;
}
static const char *mcp_resource_list_params(void *ctx) {
    (void)ctx;
    return LIST_RESOURCES_PARAMS;
}
static void mcp_resource_list_deinit(void *ctx, hu_allocator_t *alloc) {
    if (ctx)
        alloc->free(alloc->ctx, ctx, sizeof(mcp_resource_list_ctx_t));
}

static const hu_tool_vtable_t mcp_resource_list_vtable = {
    .execute = mcp_resource_list_execute,
    .name = mcp_resource_list_name,
    .description = mcp_resource_list_desc,
    .parameters_json = mcp_resource_list_params,
    .deinit = mcp_resource_list_deinit,
};

hu_error_t hu_mcp_resource_list_tool_create(hu_allocator_t *alloc, hu_tool_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    mcp_resource_list_ctx_t *c = (mcp_resource_list_ctx_t *)alloc->alloc(alloc->ctx, sizeof(*c));
    if (!c)
        return HU_ERR_OUT_OF_MEMORY;
    memset(c, 0, sizeof(*c));
    c->alloc = alloc;
    out->ctx = c;
    out->vtable = &mcp_resource_list_vtable;
    return HU_OK;
}

/* ──────────────────────────────────────────────────────────────────────────
 * read_mcp_resource tool
 * ────────────────────────────────────────────────────────────────────────── */

#define READ_RESOURCE_NAME "read_mcp_resource"
#define READ_RESOURCE_DESC "Read a specific resource from an MCP server"
#define READ_RESOURCE_PARAMS                                                                 \
    "{\"type\":\"object\",\"properties\":{" \
    "\"server\":{\"type\":\"string\",\"description\":\"Server name\"}," \
    "\"uri\":{\"type\":\"string\",\"description\":\"Resource URI\"}" \
    "},\"required\":[\"server\",\"uri\"]}"

typedef struct {
    hu_allocator_t *alloc;
} mcp_resource_read_ctx_t;

static hu_error_t mcp_resource_read_execute(void *ctx, hu_allocator_t *alloc,
                                            const hu_json_value_t *args, hu_tool_result_t *out) {
    mcp_resource_read_ctx_t *c = (mcp_resource_read_ctx_t *)ctx;
    (void)c;
    if (!out)
        return HU_ERR_INVALID_ARGUMENT;
    if (!args) {
        *out = hu_tool_result_fail("invalid args", 12);
        return HU_ERR_INVALID_ARGUMENT;
    }

    const char *server = hu_json_get_string(args, "server");
    const char *uri = hu_json_get_string(args, "uri");

    if (!server) {
        *out = hu_tool_result_fail("missing server parameter", 24);
        return HU_OK;
    }
    if (!uri) {
        *out = hu_tool_result_fail("missing uri parameter", 21);
        return HU_OK;
    }

#if HU_IS_TEST
    /* Test response with mock resource content */
    char *response = hu_sprintf(
        alloc,
        "{\"uri\":\"%s\",\"mimeType\":\"text/plain\",\"content\":\"Mock resource content from %s for %s\"}",
        uri, server, uri);
    if (!response)
        return HU_ERR_OUT_OF_MEMORY;
    *out = hu_tool_result_ok_owned(response, strlen(response));
#else
    /* In production, would call MCP manager to read resource from server.
     * For now, return informational message. */
    char *response = hu_sprintf(alloc,
                                "{\"uri\":\"%s\",\"note\":\"Resource reading requires MCP manager integration\"}", uri);
    if (!response)
        return HU_ERR_OUT_OF_MEMORY;
    *out = hu_tool_result_ok_owned(response, strlen(response));
#endif
    return HU_OK;
}

static const char *mcp_resource_read_name(void *ctx) {
    (void)ctx;
    return READ_RESOURCE_NAME;
}
static const char *mcp_resource_read_desc(void *ctx) {
    (void)ctx;
    return READ_RESOURCE_DESC;
}
static const char *mcp_resource_read_params(void *ctx) {
    (void)ctx;
    return READ_RESOURCE_PARAMS;
}
static void mcp_resource_read_deinit(void *ctx, hu_allocator_t *alloc) {
    if (ctx)
        alloc->free(alloc->ctx, ctx, sizeof(mcp_resource_read_ctx_t));
}

static const hu_tool_vtable_t mcp_resource_read_vtable = {
    .execute = mcp_resource_read_execute,
    .name = mcp_resource_read_name,
    .description = mcp_resource_read_desc,
    .parameters_json = mcp_resource_read_params,
    .deinit = mcp_resource_read_deinit,
};

hu_error_t hu_mcp_resource_read_tool_create(hu_allocator_t *alloc, hu_tool_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    mcp_resource_read_ctx_t *c = (mcp_resource_read_ctx_t *)alloc->alloc(alloc->ctx, sizeof(*c));
    if (!c)
        return HU_ERR_OUT_OF_MEMORY;
    memset(c, 0, sizeof(*c));
    c->alloc = alloc;
    out->ctx = c;
    out->vtable = &mcp_resource_read_vtable;
    return HU_OK;
}
