#include "seaclaw/agent/action_preview.h"
#include "seaclaw/core/json.h"
#include "seaclaw/core/string.h"
#include <stdio.h>
#include <string.h>

#define SC_ACTION_PREVIEW_MAX_DESC 80

static const char *risk_for_tool(const char *tool_name) {
    if (!tool_name)
        return "medium";
    if (strcmp(tool_name, "shell") == 0 || strcmp(tool_name, "spawn") == 0 ||
        strcmp(tool_name, "file_write") == 0 || strcmp(tool_name, "file_edit") == 0)
        return "high";
    if (strcmp(tool_name, "http_request") == 0 || strcmp(tool_name, "browser_open") == 0)
        return "medium";
    if (strcmp(tool_name, "file_read") == 0 || strcmp(tool_name, "memory_recall") == 0)
        return "low";
    return "medium";
}

sc_error_t sc_action_preview_generate(sc_allocator_t *alloc, const char *tool_name,
                                      const char *args_json, size_t args_json_len,
                                      sc_action_preview_t *out) {
    if (!alloc || !tool_name || !out)
        return SC_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    out->tool_name = tool_name;
    out->risk_level = risk_for_tool(tool_name);

    char *desc = NULL;
    sc_json_value_t *root = NULL;
    sc_error_t err = sc_json_parse(alloc, args_json, args_json_len, &root);
    if (err != SC_OK || !root || root->type != SC_JSON_OBJECT) {
        size_t n = strlen(tool_name) + 10;
        desc = (char *)alloc->alloc(alloc->ctx, n);
        if (desc)
            snprintf(desc, n, "%s call", tool_name);
        out->description = desc;
        return SC_OK;
    }

    const char *cmd = sc_json_get_string(root, "command");
    const char *path = sc_json_get_string(root, "path");
    const char *method = sc_json_get_string(root, "method");
    const char *url = sc_json_get_string(root, "url");

    if (strcmp(tool_name, "shell") == 0 || strcmp(tool_name, "spawn") == 0) {
        if (cmd && cmd[0]) {
            size_t len = strlen(cmd);
            if (len > SC_ACTION_PREVIEW_MAX_DESC)
                len = SC_ACTION_PREVIEW_MAX_DESC;
            size_t buf_len = len + 10;
            desc = (char *)alloc->alloc(alloc->ctx, buf_len);
            if (desc)
                snprintf(desc, buf_len, "Run: `%.*s%s`", (int)len, cmd,
                         strlen(cmd) > SC_ACTION_PREVIEW_MAX_DESC ? "..." : "");
        } else {
            desc = sc_strdup(alloc, "Run: `<command>`");
        }
    } else if (strcmp(tool_name, "file_write") == 0) {
        if (path && path[0]) {
            size_t buf_len = strlen(path) + 12;
            desc = (char *)alloc->alloc(alloc->ctx, buf_len);
            if (desc)
                snprintf(desc, buf_len, "Write to %s", path);
        } else {
            desc = sc_strdup(alloc, "Write to <path>");
        }
    } else if (strcmp(tool_name, "file_read") == 0) {
        if (path && path[0]) {
            size_t buf_len = strlen(path) + 8;
            desc = (char *)alloc->alloc(alloc->ctx, buf_len);
            if (desc)
                snprintf(desc, buf_len, "Read %s", path);
        } else {
            desc = sc_strdup(alloc, "Read <path>");
        }
    } else if (strcmp(tool_name, "file_edit") == 0) {
        if (path && path[0]) {
            size_t buf_len = strlen(path) + 8;
            desc = (char *)alloc->alloc(alloc->ctx, buf_len);
            if (desc)
                snprintf(desc, buf_len, "Edit %s", path);
        } else {
            desc = sc_strdup(alloc, "Edit <path>");
        }
    } else if (strcmp(tool_name, "http_request") == 0) {
        const char *m = method && method[0] ? method : "GET";
        const char *u = url && url[0] ? url : "<url>";
        size_t buf_len = strlen(m) + strlen(u) + 8;
        desc = (char *)alloc->alloc(alloc->ctx, buf_len);
        if (desc)
            snprintf(desc, buf_len, "HTTP %s %s", m, u);
    } else {
        size_t n = strlen(tool_name) + 10;
        desc = (char *)alloc->alloc(alloc->ctx, n);
        if (desc)
            snprintf(desc, n, "%s call", tool_name);
    }

    sc_json_free(alloc, root);
    out->description = desc ? desc : sc_strdup(alloc, "<tool_name> call");
    return SC_OK;
}

sc_error_t sc_action_preview_format(sc_allocator_t *alloc, const sc_action_preview_t *p, char **out,
                                    size_t *out_len) {
    if (!alloc || !p || !out)
        return SC_ERR_INVALID_ARGUMENT;
    *out = NULL;
    if (out_len)
        *out_len = 0;

    const char *risk = p->risk_level ? p->risk_level : "medium";
    const char *tool = p->tool_name ? p->tool_name : "unknown";
    const char *desc = p->description ? p->description : "";

    size_t need = 4 + strlen(risk) + 2 + strlen(tool) + 2 + strlen(desc) + 1;
    char *buf = (char *)alloc->alloc(alloc->ctx, need);
    if (!buf)
        return SC_ERR_OUT_OF_MEMORY;
    int n = snprintf(buf, need, "[%s] %s: %s", risk, tool, desc);
    if (n < 0 || (size_t)n >= need) {
        alloc->free(alloc->ctx, buf, need);
        return SC_ERR_INVALID_ARGUMENT;
    }
    *out = buf;
    if (out_len)
        *out_len = (size_t)n;
    return SC_OK;
}

void sc_action_preview_free(sc_allocator_t *alloc, sc_action_preview_t *p) {
    if (!alloc || !p)
        return;
    if (p->description) {
        alloc->free(alloc->ctx, p->description, strlen(p->description) + 1);
        p->description = NULL;
    }
}
