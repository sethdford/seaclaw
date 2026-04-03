#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include "human/tool.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "human/tools/schema_common.h"
#define HU_TOOL_SEARCH_NAME   "tool_search"
#define HU_TOOL_SEARCH_DESC   "Search available tools by name or keyword"
#define HU_TOOL_SEARCH_PARAMS \
    "{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\",\"description\":\"Search query to match against tool names and descriptions\"}},\"required\":[\"query\"]}"

#define HU_TOOL_SEARCH_MAX_RESULTS 20

typedef struct hu_tool_search_ctx {
    hu_tool_t *tools;
    size_t tools_count;
} hu_tool_search_ctx_t;

/* Case-insensitive substring match */
static int hu_tool_search_match(const char *haystack, const char *needle) {
    if (!haystack || !needle)
        return 0;
    if (!*needle)
        return 1;

    for (; *haystack; haystack++) {
        const char *h = haystack, *n = needle;
        while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
            h++;
            n++;
        }
        if (!*n)
            return 1;
    }
    return 0;
}

static hu_error_t tool_search_execute(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args,
                                      hu_tool_result_t *out) {
    hu_tool_search_ctx_t *c = (hu_tool_search_ctx_t *)ctx;
    if (!c || !out || !args) {
        *out = hu_tool_result_fail("invalid args", 12);
        return HU_ERR_INVALID_ARGUMENT;
    }

    /* Extract query string from JSON args */
    if (args->type != HU_JSON_OBJECT) {
        *out = hu_tool_result_fail("args must be object", 19);
        return HU_OK;
    }

    const char *query = NULL;
    size_t query_len = 0;

    for (size_t i = 0; i < args->data.object.len; i++) {
        if (strcmp(args->data.object.pairs[i].key, "query") == 0) {
            const hu_json_value_t *q_val = args->data.object.pairs[i].value;
            if (q_val && q_val->type == HU_JSON_STRING) {
                query = q_val->data.string.ptr;
                query_len = q_val->data.string.len;
            }
            break;
        }
    }

    if (!query || query_len == 0) {
        *out = hu_tool_result_fail("query parameter required", 23);
        return HU_OK;
    }

    if (!c->tools || c->tools_count == 0) {
        char *msg = hu_strndup(alloc, "[]", 2);
        if (!msg) {
            *out = hu_tool_result_fail("out of memory", 13);
            return HU_ERR_OUT_OF_MEMORY;
        }
        *out = hu_tool_result_ok_owned(msg, 2);
        return HU_OK;
    }

    /* Allocate result buffer: rough estimate */
    size_t cap = 4096 + (HU_TOOL_SEARCH_MAX_RESULTS * 512);
    char *buf = (char *)alloc->alloc(alloc->ctx, cap);
    if (!buf) {
        *out = hu_tool_result_fail("out of memory", 13);
        return HU_ERR_OUT_OF_MEMORY;
    }

    size_t pos = 0;
    size_t count = 0;

    buf[pos++] = '[';

    for (size_t i = 0; i < c->tools_count && count < HU_TOOL_SEARCH_MAX_RESULTS; i++) {
        hu_tool_t *tool = &c->tools[i];
        if (!tool->vtable)
            continue;

        const char *name = tool->vtable->name(tool->ctx);
        const char *desc = tool->vtable->description(tool->ctx);

        if (!name)
            continue;

        int match = hu_tool_search_match(name, query) || (desc && hu_tool_search_match(desc, query));
        if (!match)
            continue;

        /* Add comma before object if not first */
        if (count > 0 && pos < cap - 1)
            buf[pos++] = ',';

        /* Build JSON object: {"name":"...", "description":"...", "permission":"read_only"} */
        const char *desc_str = desc ? desc : "";
        int wrote = snprintf(
            buf + pos, cap - pos,
            "{\"name\":\"%s\",\"description\":\"%s\",\"permission\":\"read_only\"}",
            name, desc_str);

        if (wrote > 0 && (size_t)wrote < cap - pos)
            pos += (size_t)wrote;

        count++;
    }

    if (pos < cap - 1)
        buf[pos++] = ']';
    buf[pos] = '\0';

    *out = hu_tool_result_ok_owned(buf, pos);
    return HU_OK;
}

static const char *tool_search_name(void *ctx) {
    (void)ctx;
    return HU_TOOL_SEARCH_NAME;
}

static const char *tool_search_description(void *ctx) {
    (void)ctx;
    return HU_TOOL_SEARCH_DESC;
}

static const char *tool_search_parameters_json(void *ctx) {
    (void)ctx;
    return HU_TOOL_SEARCH_PARAMS;
}

static void tool_search_deinit(void *ctx, hu_allocator_t *alloc) {
    if (ctx && alloc)
        alloc->free(alloc->ctx, ctx, sizeof(hu_tool_search_ctx_t));
}

static const hu_tool_vtable_t tool_search_vtable = {
    .execute = tool_search_execute,
    .name = tool_search_name,
    .description = tool_search_description,
    .parameters_json = tool_search_parameters_json,
    .deinit = tool_search_deinit,
};

hu_error_t hu_tool_search_create(hu_allocator_t *alloc, hu_tool_t *tools, size_t tools_count,
                                 hu_tool_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;

    hu_tool_search_ctx_t *c = (hu_tool_search_ctx_t *)alloc->alloc(alloc->ctx, sizeof(*c));
    if (!c)
        return HU_ERR_OUT_OF_MEMORY;

    memset(c, 0, sizeof(*c));
    c->tools = tools;
    c->tools_count = tools_count;

    out->ctx = c;
    out->vtable = &tool_search_vtable;
    return HU_OK;
}
