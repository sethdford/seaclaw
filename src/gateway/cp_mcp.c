#ifdef HU_GATEWAY_POSIX

#include "cp_internal.h"
#include "human/mcp_resources.h"
#include <string.h>

hu_error_t cp_mcp_resources_list(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                                 const hu_control_protocol_t *proto, const hu_json_value_t *root,
                                 char **out, size_t *out_len) {
    (void)conn;
    (void)proto;
    (void)root;
    if (!alloc || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    if (!app || !app->mcp_resources) {
        static const char empty[] = "{\"resources\":[],\"templates\":[]}";
        *out_len = sizeof(empty) - 1;
        *out = (char *)alloc->alloc(alloc->ctx, *out_len + 1);
        if (!*out)
            return HU_ERR_OUT_OF_MEMORY;
        memcpy(*out, empty, *out_len + 1);
        return HU_OK;
    }
    return hu_mcp_resource_list_json(alloc, app->mcp_resources, out, out_len);
}

hu_error_t cp_mcp_prompts_list(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                               const hu_control_protocol_t *proto, const hu_json_value_t *root,
                               char **out, size_t *out_len) {
    (void)conn;
    (void)proto;
    (void)root;
    if (!alloc || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    if (!app || !app->mcp_prompts) {
        static const char empty[] = "{\"prompts\":[]}";
        *out_len = sizeof(empty) - 1;
        *out = (char *)alloc->alloc(alloc->ctx, *out_len + 1);
        if (!*out)
            return HU_ERR_OUT_OF_MEMORY;
        memcpy(*out, empty, *out_len + 1);
        return HU_OK;
    }
    return hu_mcp_prompt_list_json(alloc, app->mcp_prompts, out, out_len);
}

#endif /* HU_GATEWAY_POSIX */
