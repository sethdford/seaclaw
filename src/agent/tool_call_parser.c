/*
 * Text-based tool call parser — extracts tool calls from LLM text output.
 * Used when the provider does not support native tool calling (e.g. Ollama).
 */
#include "human/agent/tool_call_parser.h"
#include "human/core/json.h"
#include <stdio.h>
#include <string.h>

static const char TAG_OPEN[] = "<tool_call>";
static const size_t TAG_OPEN_LEN = 11;
static const char TAG_CLOSE[] = "</tool_call>";
static const size_t TAG_CLOSE_LEN = 12;

static char *alloc_strdup(hu_allocator_t *alloc, const char *s, size_t len) {
    if (!s || len == 0)
        return NULL;
    char *dup = alloc->alloc(alloc->ctx, len + 1);
    if (!dup)
        return NULL;
    memcpy(dup, s, len);
    dup[len] = '\0';
    return dup;
}

/* Find needle in haystack (not NUL-terminated safe). */
static const char *find_tag(const char *haystack, size_t haystack_len, const char *needle,
                            size_t needle_len) {
    if (needle_len > haystack_len)
        return NULL;
    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        if (memcmp(haystack + i, needle, needle_len) == 0)
            return haystack + i;
    }
    return NULL;
}

hu_error_t hu_text_tool_calls_parse(hu_allocator_t *alloc, const char *text, size_t text_len,
                                    hu_tool_call_t **out, size_t *out_count) {
    if (!alloc || !out || !out_count)
        return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_count = 0;

    if (!text || text_len == 0)
        return HU_OK;

    hu_tool_call_t calls[HU_TEXT_TOOL_CALL_MAX];
    memset(calls, 0, sizeof(calls));
    size_t count = 0;

    const char *cursor = text;
    size_t remaining = text_len;

    while (remaining > 0 && count < HU_TEXT_TOOL_CALL_MAX) {
        const char *open = find_tag(cursor, remaining, TAG_OPEN, TAG_OPEN_LEN);
        if (!open)
            break;

        const char *json_start = open + TAG_OPEN_LEN;
        size_t after_open = remaining - (size_t)(json_start - cursor);
        const char *close = find_tag(json_start, after_open, TAG_CLOSE, TAG_CLOSE_LEN);
        if (!close)
            break;

        size_t json_len = (size_t)(close - json_start);

        /* Skip whitespace around the JSON */
        while (json_len > 0 && (json_start[0] == ' ' || json_start[0] == '\n' ||
                                json_start[0] == '\r' || json_start[0] == '\t')) {
            json_start++;
            json_len--;
        }
        while (json_len > 0 && (json_start[json_len - 1] == ' ' ||
                                json_start[json_len - 1] == '\n' ||
                                json_start[json_len - 1] == '\r' ||
                                json_start[json_len - 1] == '\t')) {
            json_len--;
        }

        /* Parse JSON */
        hu_json_value_t *root = NULL;
        hu_error_t perr = hu_json_parse(alloc, json_start, json_len, &root);
        if (perr == HU_OK && root && root->type == HU_JSON_OBJECT) {
            const char *name = hu_json_get_string(root, "name");
            hu_json_value_t *args_val = hu_json_object_get(root, "arguments");

            if (name) {
                hu_tool_call_t *tc = &calls[count];
                tc->name = alloc_strdup(alloc, name, strlen(name));
                tc->name_len = tc->name ? strlen(tc->name) : 0;

                if (args_val) {
                    char *args_str = NULL;
                    size_t args_str_len = 0;
                    hu_error_t serr = hu_json_stringify(alloc, args_val, &args_str, &args_str_len);
                    if (serr == HU_OK && args_str) {
                        tc->arguments = args_str;
                        tc->arguments_len = args_str_len;
                    } else {
                        tc->arguments = alloc_strdup(alloc, "{}", 2);
                        tc->arguments_len = 2;
                    }
                } else {
                    tc->arguments = alloc_strdup(alloc, "{}", 2);
                    tc->arguments_len = 2;
                }

                /* Generate a synthetic tool call ID */
                char id_buf[32];
                int id_len = snprintf(id_buf, sizeof(id_buf), "tc_text_%zu", count);
                tc->id = alloc_strdup(alloc, id_buf, (size_t)id_len);
                tc->id_len = tc->id ? (size_t)id_len : 0;

                count++;
            }
        }
        if (root)
            hu_json_free(alloc, root);

        cursor = close + TAG_CLOSE_LEN;
        remaining = text_len - (size_t)(cursor - text);
    }

    if (count == 0)
        return HU_OK;

    hu_tool_call_t *result = alloc->alloc(alloc->ctx, count * sizeof(hu_tool_call_t));
    if (!result)
        return HU_ERR_OUT_OF_MEMORY;
    memcpy(result, calls, count * sizeof(hu_tool_call_t));
    *out = result;
    *out_count = count;
    return HU_OK;
}

void hu_text_tool_calls_free(hu_allocator_t *alloc, hu_tool_call_t *calls, size_t count) {
    if (!alloc || !calls)
        return;
    for (size_t i = 0; i < count; i++) {
        if (calls[i].id)
            alloc->free(alloc->ctx, (void *)calls[i].id, calls[i].id_len + 1);
        if (calls[i].name)
            alloc->free(alloc->ctx, (void *)calls[i].name, calls[i].name_len + 1);
        if (calls[i].arguments)
            alloc->free(alloc->ctx, (void *)calls[i].arguments, calls[i].arguments_len + 1);
    }
    alloc->free(alloc->ctx, calls, count * sizeof(hu_tool_call_t));
}

hu_error_t hu_text_tool_calls_strip(hu_allocator_t *alloc, const char *text, size_t text_len,
                                    char **out, size_t *out_len) {
    if (!alloc || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_len = 0;

    if (!text || text_len == 0)
        return HU_OK;

    char *buf = alloc->alloc(alloc->ctx, text_len + 1);
    if (!buf)
        return HU_ERR_OUT_OF_MEMORY;

    size_t buf_len = 0;
    const char *cursor = text;
    size_t remaining = text_len;

    while (remaining > 0) {
        const char *open = find_tag(cursor, remaining, TAG_OPEN, TAG_OPEN_LEN);
        if (!open) {
            memcpy(buf + buf_len, cursor, remaining);
            buf_len += remaining;
            break;
        }
        size_t prefix_len = (size_t)(open - cursor);
        if (prefix_len > 0) {
            memcpy(buf + buf_len, cursor, prefix_len);
            buf_len += prefix_len;
        }
        const char *after_open = open + TAG_OPEN_LEN;
        size_t after_len = remaining - (size_t)(after_open - cursor);
        const char *close = find_tag(after_open, after_len, TAG_CLOSE, TAG_CLOSE_LEN);
        if (!close) {
            memcpy(buf + buf_len, open, remaining - prefix_len);
            buf_len += remaining - prefix_len;
            break;
        }
        cursor = close + TAG_CLOSE_LEN;
        remaining = text_len - (size_t)(cursor - text);
    }

    /* Trim trailing whitespace */
    while (buf_len > 0 && (buf[buf_len - 1] == ' ' || buf[buf_len - 1] == '\n' ||
                           buf[buf_len - 1] == '\r' || buf[buf_len - 1] == '\t'))
        buf_len--;

    if (buf_len == 0) {
        alloc->free(alloc->ctx, buf, text_len + 1);
        return HU_OK;
    }

    buf[buf_len] = '\0';
    *out = buf;
    *out_len = buf_len;
    return HU_OK;
}
