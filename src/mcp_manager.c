#include "human/mcp_manager.h"
#include "human/config.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/mcp.h"
#include "human/mcp_transport.h"
#include "human/oauth.h"
#include "human/tool.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Internal server slot ─────────────────────────────────────────────── */

typedef struct hu_mcp_mgr_slot {
    char *name;
    hu_mcp_server_t *server;
    hu_mcp_server_config_t config;
    hu_mcp_transport_t transport;  /* For HTTP/SSE transports */
    char *transport_type;          /* "stdio", "http", or "sse" */
    char *url;                     /* For HTTP/SSE transports */
    bool connected;
    bool auto_connect;
    uint32_t timeout_ms;
    size_t tool_count;
    /* OAuth2 PKCE authentication (optional) */
    char *oauth_client_id;
    char *oauth_auth_url;
    char *oauth_token_url;
    char *oauth_scopes;
    char *oauth_redirect_uri;
    hu_oauth_token_t oauth_token;  /* Cached token */
} hu_mcp_mgr_slot_t;

struct hu_mcp_manager {
    hu_allocator_t *alloc;
    hu_mcp_mgr_slot_t slots[HU_MCP_MANAGER_MAX_SERVERS];
    size_t slot_count;
};

/* ── Helpers ──────────────────────────────────────────────────────────── */

static char *dup_str(hu_allocator_t *alloc, const char *s) {
    if (!s)
        return NULL;
    size_t len = strlen(s);
    char *d = (char *)alloc->alloc(alloc->ctx, len + 1);
    if (d)
        memcpy(d, s, len + 1);
    return d;
}

static void free_str(hu_allocator_t *alloc, char *s) {
    if (s)
        alloc->free(alloc->ctx, s, strlen(s) + 1);
}

static void slot_destroy(hu_allocator_t *alloc, hu_mcp_mgr_slot_t *slot) {
    if (slot->server)
        hu_mcp_server_destroy(slot->server);
    if (slot->transport.ctx || slot->transport.send || slot->transport.recv || slot->transport.close)
        hu_mcp_transport_destroy(&slot->transport, alloc);
    free_str(alloc, slot->name);
    free_str(alloc, slot->transport_type);
    free_str(alloc, slot->url);
    /* Free OAuth fields */
    free_str(alloc, slot->oauth_client_id);
    free_str(alloc, slot->oauth_auth_url);
    free_str(alloc, slot->oauth_token_url);
    free_str(alloc, slot->oauth_scopes);
    free_str(alloc, slot->oauth_redirect_uri);
    hu_mcp_oauth_token_free(alloc, &slot->oauth_token);
    memset(slot, 0, sizeof(*slot));
}

/* ── Lifecycle ────────────────────────────────────────────────────────── */

hu_error_t hu_mcp_manager_create(hu_allocator_t *alloc,
                                 const struct hu_mcp_server_entry *entries, size_t count,
                                 hu_mcp_manager_t **out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;

    hu_mcp_manager_t *mgr =
        (hu_mcp_manager_t *)alloc->alloc(alloc->ctx, sizeof(hu_mcp_manager_t));
    if (!mgr)
        return HU_ERR_OUT_OF_MEMORY;
    memset(mgr, 0, sizeof(*mgr));
    mgr->alloc = alloc;

    if (!entries || count == 0) {
        *out = mgr;
        return HU_OK;
    }

    size_t n = count < HU_MCP_MANAGER_MAX_SERVERS ? count : HU_MCP_MANAGER_MAX_SERVERS;
    for (size_t i = 0; i < n; i++) {
        const struct hu_mcp_server_entry *e = &entries[i];
        if (!e->name)
            continue;

        /* Determine transport type and validate required fields */
        const char *transport_type = e->transport_type ? e->transport_type : "stdio";
        bool is_stdio = (strcmp(transport_type, "stdio") == 0);
        bool is_http = (strcmp(transport_type, "http") == 0);
        bool is_sse = (strcmp(transport_type, "sse") == 0);

        if (!is_stdio && !is_http && !is_sse) {
            fprintf(stderr, "mcp_manager: invalid transport_type '%s' for server '%s'\n",
                    transport_type, e->name);
            continue;
        }

        if (is_stdio && !e->command)
            continue;  /* stdio requires command */
        if ((is_http || is_sse) && !e->url)
            continue;  /* http/sse require url */

        hu_mcp_mgr_slot_t *slot = &mgr->slots[mgr->slot_count];
        slot->name = dup_str(alloc, e->name);
        if (!slot->name)
            goto fail;

        slot->transport_type = dup_str(alloc, transport_type);
        if (!slot->transport_type) {
            free_str(alloc, slot->name);
            goto fail;
        }

        if (is_stdio) {
            hu_mcp_server_config_t cfg = {
                .command = e->command,
                .args = (const char **)e->args,
                .args_count = e->args_count,
            };
            slot->config = cfg;
            slot->server = NULL;
        } else {
            /* For HTTP/SSE, store the URL */
            slot->url = dup_str(alloc, e->url);
            if (!slot->url) {
                free_str(alloc, slot->transport_type);
                free_str(alloc, slot->name);
                goto fail;
            }
            memset(&slot->config, 0, sizeof(slot->config));
            slot->server = NULL;
        }

        memset(&slot->transport, 0, sizeof(slot->transport));
        slot->auto_connect = e->auto_connect;
        slot->timeout_ms = e->timeout_ms > 0 ? e->timeout_ms : HU_MCP_MANAGER_DEFAULT_TIMEOUT;
        slot->connected = false;
        slot->tool_count = 0;

        /* Copy OAuth2 config (if present) */
        slot->oauth_client_id = dup_str(alloc, e->oauth_client_id);
        slot->oauth_auth_url = dup_str(alloc, e->oauth_auth_url);
        slot->oauth_token_url = dup_str(alloc, e->oauth_token_url);
        slot->oauth_scopes = dup_str(alloc, e->oauth_scopes);
        slot->oauth_redirect_uri = dup_str(alloc, e->oauth_redirect_uri);
        memset(&slot->oauth_token, 0, sizeof(slot->oauth_token));

        mgr->slot_count++;
    }

    *out = mgr;
    return HU_OK;

fail:
    for (size_t i = 0; i < mgr->slot_count; i++)
        slot_destroy(alloc, &mgr->slots[i]);
    alloc->free(alloc->ctx, mgr, sizeof(*mgr));
    return HU_ERR_OUT_OF_MEMORY;
}

void hu_mcp_manager_destroy(hu_mcp_manager_t *mgr) {
    if (!mgr)
        return;
    hu_allocator_t *alloc = mgr->alloc;
    for (size_t i = 0; i < mgr->slot_count; i++)
        slot_destroy(alloc, &mgr->slots[i]);
    alloc->free(alloc->ctx, mgr, sizeof(*mgr));
}

/* ── OAuth Token Loading ───────────────────────────────────────────────── */

/**
 * Load cached OAuth token for this slot (if configured).
 * Logs message if token is needed but not found or expired.
 * Returns HU_OK even if token loading fails (graceful degradation).
 */
static void slot_load_oauth_token(hu_allocator_t *alloc, hu_mcp_mgr_slot_t *slot) {
    if (!slot->oauth_client_id || !slot->oauth_token_url)
        return;  /* OAuth not configured for this server */

    /* Try to load cached token from ~/.human/oauth_tokens.json */
    const char *home = getenv("HOME");
    if (!home)
        home = ".";

    /* Build path: ~/.human/oauth_tokens.json */
    char token_path[512];
    int written = snprintf(token_path, sizeof(token_path), "%s/.human/oauth_tokens.json", home);
    if (written < 0 || (size_t)written >= sizeof(token_path)) {
        fprintf(stderr, "mcp_manager: token path too long for server '%s'\n", slot->name);
        return;
    }

    hu_error_t err = hu_mcp_oauth_token_load(alloc, token_path, slot->name, &slot->oauth_token);
    if (err == HU_OK) {
        if (hu_mcp_oauth_token_is_expired(&slot->oauth_token)) {
            fprintf(stderr,
                    "mcp_manager: OAuth token for '%s' has expired. "
                    "Please re-authenticate.\n",
                    slot->name);
            hu_mcp_oauth_token_free(alloc, &slot->oauth_token);
        } else {
            fprintf(stderr,
                    "mcp_manager: loaded cached OAuth token for '%s' (valid until %lld)\n",
                    slot->name, (long long)slot->oauth_token.expires_at);
        }
    } else if (err == HU_ERR_NOT_FOUND) {
        fprintf(stderr,
                "mcp_manager: no cached OAuth token for '%s'. "
                "Please authenticate via: human oauth %s\n",
                slot->name, slot->name);
    } else {
        fprintf(stderr, "mcp_manager: failed to load OAuth token for '%s': %d\n", slot->name,
                (int)err);
    }
}

/* ── Connection ───────────────────────────────────────────────────────── */

static hu_error_t connect_slot(hu_allocator_t *alloc, hu_mcp_mgr_slot_t *slot) {
    if (slot->connected)
        return HU_OK;

    /* Handle stdio transport (traditional MCP server) */
    if (!slot->transport_type || strcmp(slot->transport_type, "stdio") == 0) {
        if (slot->server)
            return HU_OK;

        hu_mcp_server_t *srv = hu_mcp_server_create(alloc, &slot->config);
        if (!srv)
            return HU_ERR_IO;

        hu_error_t err = hu_mcp_server_connect(srv);
        if (err != HU_OK) {
            hu_mcp_server_destroy(srv);
            return err;
        }

        slot->server = srv;
        slot->connected = true;
        return HU_OK;
    }

    /* Handle HTTP transport */
    if (strcmp(slot->transport_type, "http") == 0) {
        if (!slot->url)
            return HU_ERR_INVALID_ARGUMENT;

        /* Load OAuth token if configured */
        slot_load_oauth_token(alloc, slot);

        hu_error_t err =
            hu_mcp_transport_http_create(alloc, slot->url, strlen(slot->url), &slot->transport);
        if (err != HU_OK)
            return err;

        /* TODO: When hu_mcp_transport_http_create supports auth_header parameter,
           pass "Authorization: Bearer {token}" if oauth_token.access_token is set */

        slot->connected = true;
        return HU_OK;
    }

    /* Handle SSE transport */
    if (strcmp(slot->transport_type, "sse") == 0) {
        if (!slot->url)
            return HU_ERR_INVALID_ARGUMENT;

        /* Load OAuth token if configured */
        slot_load_oauth_token(alloc, slot);

        hu_error_t err =
            hu_mcp_transport_sse_create(alloc, slot->url, strlen(slot->url), &slot->transport);
        if (err != HU_OK)
            return err;

        /* TODO: When hu_mcp_transport_sse_create supports auth_header parameter,
           pass "Authorization: Bearer {token}" if oauth_token.access_token is set */

        slot->connected = true;
        return HU_OK;
    }

    return HU_ERR_INVALID_ARGUMENT;
}

hu_error_t hu_mcp_manager_connect_auto(hu_mcp_manager_t *mgr) {
    if (!mgr)
        return HU_ERR_INVALID_ARGUMENT;

    for (size_t i = 0; i < mgr->slot_count; i++) {
        if (!mgr->slots[i].auto_connect)
            continue;
        hu_error_t err = connect_slot(mgr->alloc, &mgr->slots[i]);
        if (err != HU_OK)
            fprintf(stderr, "mcp_manager: failed to connect server '%s': %d\n",
                    mgr->slots[i].name ? mgr->slots[i].name : "?", (int)err);
    }
    return HU_OK;
}

hu_error_t hu_mcp_manager_connect_server(hu_mcp_manager_t *mgr, const char *server_name) {
    if (!mgr || !server_name)
        return HU_ERR_INVALID_ARGUMENT;

    for (size_t i = 0; i < mgr->slot_count; i++) {
        if (mgr->slots[i].name && strcmp(mgr->slots[i].name, server_name) == 0)
            return connect_slot(mgr->alloc, &mgr->slots[i]);
    }
    return HU_ERR_NOT_FOUND;
}

/* ── Tool wrapper ─────────────────────────────────────────────────────── */

typedef struct hu_mcp_mgr_tool_wrapper {
    hu_allocator_t *alloc;
    hu_mcp_manager_t *mgr;
    size_t slot_index;
    char *original_name;   /* tool name on the server */
    char *prefixed_name;   /* mcp__<server>__<tool> */
    char *desc;
    char *params_json;
} hu_mcp_mgr_tool_wrapper_t;

static hu_error_t mgr_tool_execute(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args,
                                   hu_tool_result_t *out) {
    hu_mcp_mgr_tool_wrapper_t *w = (hu_mcp_mgr_tool_wrapper_t *)ctx;
    if (!w || !w->mgr || w->slot_index >= w->mgr->slot_count) {
        *out = hu_tool_result_fail("MCP manager tool wrapper invalid", 33);
        return HU_OK;
    }

    hu_mcp_mgr_slot_t *slot = &w->mgr->slots[w->slot_index];
    if (!slot->connected) {
        *out = hu_tool_result_fail("MCP server not connected", 24);
        return HU_OK;
    }

    if (!slot->server) {
        /* Transport-based execution not yet fully supported */
        *out = hu_tool_result_fail("Transport-based tool execution not yet supported", 48);
        return HU_OK;
    }

    char *args_json = NULL;
    size_t args_len = 0;
    bool args_allocated = false;
    if (args && args->type == HU_JSON_OBJECT) {
        hu_error_t err = hu_json_stringify(alloc, args, &args_json, &args_len);
        if (err != HU_OK) {
            *out = hu_tool_result_fail("Failed to serialize args", 24);
            return HU_OK;
        }
        args_allocated = true;
    }
    if (!args_json)
        args_json = "{}";

    char *result = NULL;
    size_t result_len = 0;
    hu_error_t err = hu_mcp_server_call_tool(slot->server, alloc, w->original_name, args_json,
                                             &result, &result_len);
    if (args_allocated && args_json)
        alloc->free(alloc->ctx, args_json, args_len + 1);

    if (err != HU_OK) {
        char buf[256];
        int n = snprintf(buf, sizeof(buf), "MCP tool '%s' on server '%s' failed: error %d",
                         w->original_name, slot->name ? slot->name : "?", (int)err);
        if (n < 0)
            n = 0;
        char *msg = (char *)alloc->alloc(alloc->ctx, (size_t)n + 1);
        if (msg) {
            memcpy(msg, buf, (size_t)n + 1);
            *out = hu_tool_result_fail_owned(msg, (size_t)n);
        } else {
            *out = hu_tool_result_fail("MCP tool call failed", 20);
        }
        return HU_OK;
    }

    /* Enforce max response size */
    if (result_len > HU_MCP_MANAGER_MAX_RESPONSE) {
        alloc->free(alloc->ctx, result, result_len + 1);
        *out = hu_tool_result_fail("MCP response exceeds 100MB limit", 32);
        return HU_OK;
    }

    *out = hu_tool_result_ok_owned(result, result_len);
    return HU_OK;
}

static const char *mgr_tool_name(void *ctx) {
    return ((hu_mcp_mgr_tool_wrapper_t *)ctx)->prefixed_name;
}

static const char *mgr_tool_description(void *ctx) {
    return ((hu_mcp_mgr_tool_wrapper_t *)ctx)->desc;
}

static const char *mgr_tool_parameters_json(void *ctx) {
    return ((hu_mcp_mgr_tool_wrapper_t *)ctx)->params_json;
}

static void mgr_tool_deinit(void *ctx, hu_allocator_t *alloc) {
    hu_mcp_mgr_tool_wrapper_t *w = (hu_mcp_mgr_tool_wrapper_t *)ctx;
    free_str(alloc, w->original_name);
    free_str(alloc, w->prefixed_name);
    free_str(alloc, w->desc);
    free_str(alloc, w->params_json);
    alloc->free(alloc->ctx, w, sizeof(*w));
}

static const hu_tool_vtable_t mgr_tool_vtable = {
    .execute = mgr_tool_execute,
    .name = mgr_tool_name,
    .description = mgr_tool_description,
    .parameters_json = mgr_tool_parameters_json,
    .deinit = mgr_tool_deinit,
};

/* ── Tool Discovery ───────────────────────────────────────────────────── */

hu_error_t hu_mcp_manager_load_tools(hu_mcp_manager_t *mgr, hu_allocator_t *alloc,
                                     hu_tool_t **out_tools, size_t *out_count) {
    if (!mgr || !alloc || !out_tools || !out_count)
        return HU_ERR_INVALID_ARGUMENT;
    *out_tools = NULL;
    *out_count = 0;

    hu_tool_t *tools = NULL;
    size_t total = 0;
    size_t tools_cap = 0;

    for (size_t si = 0; si < mgr->slot_count; si++) {
        hu_mcp_mgr_slot_t *slot = &mgr->slots[si];
        if (!slot->connected)
            continue;

        /* Only stdio servers are currently supported for tool discovery */
        if (!slot->server) {
            fprintf(stderr, "mcp_manager: tool discovery not yet supported for %s transport\n",
                    slot->transport_type ? slot->transport_type : "unknown");
            continue;
        }

        char **names = NULL, **descs = NULL, **params = NULL;
        size_t n = 0;
        hu_error_t err = hu_mcp_server_list_tools(slot->server, alloc, &names, &descs, &params, &n);
        if (err != HU_OK || n == 0)
            continue;

        slot->tool_count = n;

        /* Ensure capacity */
        if (total + n > tools_cap) {
            size_t new_cap = (total + n) * 2;
            if (new_cap < 8)
                new_cap = 8;
            hu_tool_t *nb = (hu_tool_t *)alloc->realloc(
                alloc->ctx, tools, tools_cap * sizeof(hu_tool_t), new_cap * sizeof(hu_tool_t));
            if (!nb) {
                /* Free this batch's string arrays */
                for (size_t j = 0; j < n; j++) {
                    free_str(alloc, names[j]);
                    free_str(alloc, descs[j]);
                    if (params[j])
                        free_str(alloc, params[j]);
                }
                alloc->free(alloc->ctx, names, n * sizeof(char *));
                alloc->free(alloc->ctx, descs, n * sizeof(char *));
                alloc->free(alloc->ctx, params, n * sizeof(char *));
                goto fail;
            }
            tools = nb;
            tools_cap = new_cap;
        }

        for (size_t j = 0; j < n; j++) {
            if (!names[j])
                continue;

            /* Build prefixed name: mcp__<server_name>__<tool_name> */
            const char *srv_name = slot->name ? slot->name : "unknown";
            size_t pref_len = 6 + strlen(srv_name) + 2 + strlen(names[j]); /* mcp__ + srv + __ + tool */
            char *prefixed = (char *)alloc->alloc(alloc->ctx, pref_len + 1);
            if (!prefixed) {
                /* Free remaining strings in this batch */
                for (size_t k = j; k < n; k++) {
                    free_str(alloc, names[k]);
                    free_str(alloc, descs[k]);
                    if (params[k])
                        free_str(alloc, params[k]);
                }
                alloc->free(alloc->ctx, names, n * sizeof(char *));
                alloc->free(alloc->ctx, descs, n * sizeof(char *));
                alloc->free(alloc->ctx, params, n * sizeof(char *));
                goto fail;
            }
            snprintf(prefixed, pref_len + 1, "mcp__%s__%s", srv_name, names[j]);

            hu_mcp_mgr_tool_wrapper_t *w = (hu_mcp_mgr_tool_wrapper_t *)alloc->alloc(
                alloc->ctx, sizeof(hu_mcp_mgr_tool_wrapper_t));
            if (!w) {
                alloc->free(alloc->ctx, prefixed, pref_len + 1);
                for (size_t k = j; k < n; k++) {
                    free_str(alloc, names[k]);
                    free_str(alloc, descs[k]);
                    if (params[k])
                        free_str(alloc, params[k]);
                }
                alloc->free(alloc->ctx, names, n * sizeof(char *));
                alloc->free(alloc->ctx, descs, n * sizeof(char *));
                alloc->free(alloc->ctx, params, n * sizeof(char *));
                goto fail;
            }
            w->alloc = alloc;
            w->mgr = mgr;
            w->slot_index = si;
            w->original_name = names[j];
            w->prefixed_name = prefixed;
            w->desc = descs[j];
            w->params_json = (params && params[j]) ? params[j] : dup_str(alloc, "{}");

            tools[total].ctx = w;
            tools[total].vtable = &mgr_tool_vtable;
            total++;
        }

        /* Free the arrays themselves (strings are now owned by wrappers) */
        alloc->free(alloc->ctx, names, n * sizeof(char *));
        alloc->free(alloc->ctx, descs, n * sizeof(char *));
        if (params)
            alloc->free(alloc->ctx, params, n * sizeof(char *));
    }

    /* Shrink to exact size */
    if (total > 0 && total < tools_cap) {
        hu_tool_t *shrunk = (hu_tool_t *)alloc->realloc(
            alloc->ctx, tools, tools_cap * sizeof(hu_tool_t), total * sizeof(hu_tool_t));
        if (shrunk)
            tools = shrunk;
    }

    *out_tools = tools;
    *out_count = total;
    return HU_OK;

fail:
    if (tools) {
        for (size_t i = 0; i < total; i++) {
            if (tools[i].vtable && tools[i].vtable->deinit)
                tools[i].vtable->deinit(tools[i].ctx, alloc);
        }
        alloc->free(alloc->ctx, tools, tools_cap * sizeof(hu_tool_t));
    }
    return HU_ERR_OUT_OF_MEMORY;
}

void hu_mcp_manager_free_tools(hu_allocator_t *alloc, hu_tool_t *tools, size_t count) {
    if (!alloc || !tools)
        return;
    for (size_t i = 0; i < count; i++) {
        if (tools[i].vtable && tools[i].vtable->deinit)
            tools[i].vtable->deinit(tools[i].ctx, alloc);
    }
    alloc->free(alloc->ctx, tools, count * sizeof(hu_tool_t));
}

/* ── Direct Tool Call ─────────────────────────────────────────────────── */

hu_error_t hu_mcp_manager_call_tool(hu_mcp_manager_t *mgr, hu_allocator_t *alloc,
                                    const char *server_name, const char *tool_name,
                                    const char *args_json, char **out_result,
                                    size_t *out_result_len) {
    if (!mgr || !alloc || !server_name || !tool_name || !out_result || !out_result_len)
        return HU_ERR_INVALID_ARGUMENT;

    for (size_t i = 0; i < mgr->slot_count; i++) {
        if (!mgr->slots[i].name || strcmp(mgr->slots[i].name, server_name) != 0)
            continue;
        if (!mgr->slots[i].connected)
            return HU_ERR_IO;

        /* stdio-based server */
        if (mgr->slots[i].server) {
            return hu_mcp_server_call_tool(mgr->slots[i].server, alloc, tool_name, args_json,
                                           out_result, out_result_len);
        }

        /* Transport-based (HTTP/SSE) — not yet implemented */
        return HU_ERR_NOT_SUPPORTED;
    }
    return HU_ERR_NOT_FOUND;
}

/* ── Queries ──────────────────────────────────────────────────────────── */

size_t hu_mcp_manager_server_count(const hu_mcp_manager_t *mgr) {
    return mgr ? mgr->slot_count : 0;
}

hu_error_t hu_mcp_manager_server_info(const hu_mcp_manager_t *mgr, size_t index,
                                      hu_mcp_server_info_t *out) {
    if (!mgr || !out || index >= mgr->slot_count)
        return HU_ERR_NOT_FOUND;

    const hu_mcp_mgr_slot_t *slot = &mgr->slots[index];
    out->name = slot->name;
    out->command = slot->config.command;
    out->connected = slot->connected;
    out->tool_count = slot->tool_count;
    out->timeout_ms = slot->timeout_ms;
    return HU_OK;
}

hu_error_t hu_mcp_manager_find_server(const hu_mcp_manager_t *mgr, const char *name,
                                      size_t *out_index) {
    if (!mgr || !name || !out_index)
        return HU_ERR_INVALID_ARGUMENT;

    for (size_t i = 0; i < mgr->slot_count; i++) {
        if (mgr->slots[i].name && strcmp(mgr->slots[i].name, name) == 0) {
            *out_index = i;
            return HU_OK;
        }
    }
    return HU_ERR_NOT_FOUND;
}
