#include "human/tools/canvas.h"
#include "human/agent.h"
#include "human/agent/tool_context.h"
#include "human/bus.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include "human/tool.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define TOOL_NAME "canvas"
#define TOOL_DESC                                                                                  \
    "Create and manage visual canvases for displaying HTML, SVG, and UI mockups"
#define HU_CANVAS_PARAMS                                                                           \
    "{\"type\":\"object\",\"properties\":{\"action\":{\"type\":\"string\","                       \
    "\"enum\":[\"create\",\"update\",\"clear\",\"close\"]},"                                     \
    "\"content\":{\"type\":\"string\"},\"title\":{\"type\":\"string\"},"                         \
    "\"canvas_id\":{\"type\":\"string\"},\"format\":{\"type\":\"string\","                       \
    "\"enum\":[\"html\",\"svg\",\"mockup\",\"react\",\"mermaid\",\"markdown\",\"code\"]},"      \
    "\"imports\":{\"type\":\"object\"},\"language\":{\"type\":\"string\"}},"                      \
    "\"required\":[\"action\"]}"

#define CANVAS_MAX          16u
#define CANVAS_MAX_CONTENT  262144u
#define CANVAS_MAX_ID_LEN   64u

#define CANVAS_MAX_VERSIONS 32u

typedef struct {
    char *canvas_id;
    char *title;
    char *format;   /* html | svg | mockup | react | mermaid | markdown | code */
    char *content;  /* may be empty */
    char *language; /* for code format */
    char *imports;  /* JSON object string for import map */
    /* Version history ring buffer */
    char *versions[CANVAS_MAX_VERSIONS];
    char *version_formats[CANVAS_MAX_VERSIONS];
    size_t version_count;
    size_t version_head;    /* write position in ring */
    size_t version_current; /* read position for undo/redo navigation */
    uint32_t version_seq;   /* monotonic version counter */
    /* User edit tracking */
    char *user_edited_content;
    bool user_edit_pending;
} canvas_entry_t;

typedef struct {
    hu_allocator_t *alloc;
    canvas_entry_t slots[CANVAS_MAX];
    size_t count;
    uint32_t next_seq;
} canvas_tool_ctx_t;

static int canvas_id_safe(const char *id) {
    if (!id || id[0] == '\0')
        return 0;
    size_t n = strlen(id);
    if (n == 0 || n > CANVAS_MAX_ID_LEN)
        return 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)id[i];
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
            ch == '_' || ch == '-' || ch == '.')
            continue;
        return 0;
    }
    return 1;
}

static size_t canvas_find_index(canvas_tool_ctx_t *c, const char *canvas_id) {
    if (!c || !canvas_id)
        return (size_t)-1;
    for (size_t i = 0; i < c->count; i++) {
        if (c->slots[i].canvas_id && strcmp(c->slots[i].canvas_id, canvas_id) == 0)
            return i;
    }
    return (size_t)-1;
}

static void canvas_free_str(hu_allocator_t *a, char **s) {
    if (*s) {
        a->free(a->ctx, *s, strlen(*s) + 1);
        *s = NULL;
    }
}

static void canvas_free_entry(canvas_tool_ctx_t *c, canvas_entry_t *e) {
    if (!c || !e)
        return;
    hu_allocator_t *a = c->alloc;
    canvas_free_str(a, &e->canvas_id);
    canvas_free_str(a, &e->title);
    canvas_free_str(a, &e->format);
    canvas_free_str(a, &e->content);
    canvas_free_str(a, &e->language);
    canvas_free_str(a, &e->imports);
    canvas_free_str(a, &e->user_edited_content);
    for (size_t i = 0; i < CANVAS_MAX_VERSIONS; i++) {
        canvas_free_str(a, &e->versions[i]);
        canvas_free_str(a, &e->version_formats[i]);
    }
    e->version_count = 0;
    e->version_head = 0;
    e->version_current = 0;
    e->version_seq = 0;
    e->user_edit_pending = false;
}

static void canvas_send_ui(hu_allocator_t *alloc, hu_json_value_t *evt_obj) {
#if HU_IS_TEST
    (void)alloc;
    hu_json_free(alloc, evt_obj);
#else
    hu_agent_t *ag = hu_agent_get_current_for_tools();
    if (!ag || !ag->ui_event_bus) {
        hu_json_free(alloc, evt_obj);
        return;
    }
    char *buf = NULL;
    size_t blen = 0;
    hu_error_t se = hu_json_stringify(alloc, evt_obj, &buf, &blen);
    hu_json_free(alloc, evt_obj);
    if (se != HU_OK || !buf)
        return;
    hu_bus_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = HU_BUS_CANVAS;
    ev.payload = buf;
    hu_bus_publish(ag->ui_event_bus, &ev);
    alloc->free(alloc->ctx, buf, blen + 1);
#endif
}

static hu_json_value_t *canvas_evt_new(hu_allocator_t *alloc, const char *action,
                                       const char *canvas_id) {
    hu_json_value_t *o = hu_json_object_new(alloc);
    if (!o)
        return NULL;
    hu_json_object_set(alloc, o, "action", hu_json_string_new(alloc, action, strlen(action)));
    if (canvas_id)
        hu_json_object_set(alloc, o, "canvas_id",
                           hu_json_string_new(alloc, canvas_id, strlen(canvas_id)));
    return o;
}

static const char *canvas_resolve_format(const char *fmt) {
    if (!fmt || !fmt[0])
        return "html";
    if (strcmp(fmt, "svg") == 0)
        return "svg";
    if (strcmp(fmt, "mockup") == 0)
        return "mockup";
    if (strcmp(fmt, "react") == 0)
        return "react";
    if (strcmp(fmt, "mermaid") == 0)
        return "mermaid";
    if (strcmp(fmt, "markdown") == 0)
        return "markdown";
    if (strcmp(fmt, "code") == 0)
        return "code";
    return "html";
}

static void canvas_push_version(canvas_tool_ctx_t *c, canvas_entry_t *e) {
    if (!c || !e || !e->content)
        return;
    hu_allocator_t *a = c->alloc;
    size_t pos = e->version_head;
    canvas_free_str(a, &e->versions[pos]);
    canvas_free_str(a, &e->version_formats[pos]);
    e->versions[pos] = hu_strndup(a, e->content, strlen(e->content));
    if (e->format)
        e->version_formats[pos] = hu_strndup(a, e->format, strlen(e->format));
    e->version_head = (pos + 1) % CANVAS_MAX_VERSIONS;
    if (e->version_count < CANVAS_MAX_VERSIONS)
        e->version_count++;
    e->version_current = e->version_head;
    e->version_seq++;
}

hu_error_t hu_canvas_entry_undo(canvas_tool_ctx_t *c, canvas_entry_t *e) {
    if (!c || !e || e->version_count == 0)
        return HU_ERR_NOT_FOUND;
    size_t prev;
    if (e->version_current == 0)
        prev = CANVAS_MAX_VERSIONS - 1;
    else
        prev = e->version_current - 1;
    if (!e->versions[prev])
        return HU_ERR_NOT_FOUND;
    /* Check we haven't gone past the oldest version */
    size_t oldest = (e->version_count < CANVAS_MAX_VERSIONS)
                        ? 0
                        : e->version_head;
    if (prev == oldest && e->version_current == oldest)
        return HU_ERR_NOT_FOUND;

    hu_allocator_t *a = c->alloc;
    canvas_free_str(a, &e->content);
    e->content = hu_strndup(a, e->versions[prev], strlen(e->versions[prev]));
    if (e->version_formats[prev]) {
        canvas_free_str(a, &e->format);
        e->format = hu_strndup(a, e->version_formats[prev], strlen(e->version_formats[prev]));
    }
    e->version_current = prev;
    return HU_OK;
}

hu_error_t hu_canvas_entry_redo(canvas_tool_ctx_t *c, canvas_entry_t *e) {
    if (!c || !e)
        return HU_ERR_NOT_FOUND;
    size_t next = (e->version_current + 1) % CANVAS_MAX_VERSIONS;
    if (next == e->version_head)
        return HU_ERR_NOT_FOUND;
    if (!e->versions[next])
        return HU_ERR_NOT_FOUND;

    hu_allocator_t *a = c->alloc;
    canvas_free_str(a, &e->content);
    e->content = hu_strndup(a, e->versions[next], strlen(e->versions[next]));
    if (e->version_formats[next]) {
        canvas_free_str(a, &e->format);
        e->format = hu_strndup(a, e->version_formats[next], strlen(e->version_formats[next]));
    }
    e->version_current = next;
    return HU_OK;
}

static hu_error_t canvas_execute(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args,
                                 hu_tool_result_t *out) {
    canvas_tool_ctx_t *c = (canvas_tool_ctx_t *)ctx;
    if (!out)
        return HU_ERR_INVALID_ARGUMENT;
    if (!c || !args) {
        *out = hu_tool_result_fail("invalid args", 12);
        return HU_ERR_INVALID_ARGUMENT;
    }

    const char *action = hu_json_get_string(args, "action");
    if (!action) {
        *out = hu_tool_result_fail("missing action", 14);
        return HU_OK;
    }

    if (strcmp(action, "create") == 0) {
        if (c->count >= CANVAS_MAX) {
            *out = hu_tool_result_fail("canvas limit reached", 20);
            return HU_OK;
        }
        const char *title_in = hu_json_get_string(args, "title");
        const char *fmt_in = hu_json_get_string(args, "format");
        const char *fmt = canvas_resolve_format(fmt_in);

        char idbuf[48];
        int nid = snprintf(idbuf, sizeof(idbuf), "cv_%u", c->next_seq++);
        if (nid < 0 || (size_t)nid >= sizeof(idbuf)) {
            *out = hu_tool_result_fail("internal id error", 17);
            return HU_OK;
        }

        const char *lang_in = hu_json_get_string(args, "language");
        hu_json_value_t *imports_val = hu_json_object_get((hu_json_value_t *)args, "imports");

        canvas_entry_t *e = &c->slots[c->count];
        memset(e, 0, sizeof(*e));
        e->canvas_id = hu_strndup(c->alloc, idbuf, (size_t)nid);
        e->title = title_in ? hu_strndup(c->alloc, title_in, strlen(title_in)) : NULL;
        e->format = hu_strndup(c->alloc, fmt, strlen(fmt));
        e->content = hu_strndup(c->alloc, "", 0);
        if (lang_in)
            e->language = hu_strndup(c->alloc, lang_in, strlen(lang_in));
        if (imports_val && imports_val->type == HU_JSON_OBJECT) {
            char *imp_str = NULL;
            size_t imp_len = 0;
            if (hu_json_stringify(c->alloc, imports_val, &imp_str, &imp_len) == HU_OK && imp_str) {
                e->imports = imp_str;
            }
        }
        if (!e->canvas_id || !e->format || !e->content) {
            canvas_free_entry(c, e);
            memset(e, 0, sizeof(*e));
            *out = hu_tool_result_fail("out of memory", 13);
            return HU_ERR_OUT_OF_MEMORY;
        }
        c->count++;

        hu_json_value_t *evt = canvas_evt_new(alloc, "create", e->canvas_id);
        if (evt) {
            if (e->title)
                hu_json_object_set(alloc, evt, "title",
                                   hu_json_string_new(alloc, e->title, strlen(e->title)));
            hu_json_object_set(alloc, evt, "format",
                               hu_json_string_new(alloc, e->format, strlen(e->format)));
            if (e->language)
                hu_json_object_set(alloc, evt, "language",
                                   hu_json_string_new(alloc, e->language, strlen(e->language)));
            if (e->imports)
                hu_json_object_set(alloc, evt, "imports",
                                   hu_json_string_new(alloc, e->imports, strlen(e->imports)));
            canvas_send_ui(alloc, evt);
        }

        char *msg = hu_sprintf(alloc, "{\"ok\":true,\"canvas_id\":\"%s\"}", e->canvas_id);
        if (!msg) {
            *out = hu_tool_result_fail("out of memory", 13);
            return HU_ERR_OUT_OF_MEMORY;
        }
        *out = hu_tool_result_ok_owned(msg, strlen(msg));
        return HU_OK;
    }

    const char *canvas_id = hu_json_get_string(args, "canvas_id");
    if (!canvas_id || !canvas_id_safe(canvas_id)) {
        *out = hu_tool_result_fail("missing or invalid canvas_id", 28);
        return HU_OK;
    }

    size_t idx = canvas_find_index(c, canvas_id);
    if (idx == (size_t)-1) {
        *out = hu_tool_result_fail("canvas not found", 16);
        return HU_OK;
    }
    canvas_entry_t *e = &c->slots[idx];

    if (strcmp(action, "update") == 0) {
        const char *content = hu_json_get_string(args, "content");
        if (!content) {
            *out = hu_tool_result_fail("content required for update", 27);
            return HU_OK;
        }
        size_t clen = strlen(content);
        if (clen > CANVAS_MAX_CONTENT) {
            *out = hu_tool_result_fail("content too large", 17);
            return HU_OK;
        }
        const char *fmt_in = hu_json_get_string(args, "format");
        const char *fmt = fmt_in ? canvas_resolve_format(fmt_in) : e->format;
        const char *lang_in = hu_json_get_string(args, "language");

        /* Push current state to version history before overwriting */
        canvas_push_version(c, e);

        char *newc = hu_strndup(c->alloc, content, clen);
        if (!newc) {
            *out = hu_tool_result_fail("out of memory", 13);
            return HU_ERR_OUT_OF_MEMORY;
        }
        if (e->content)
            c->alloc->free(c->alloc->ctx, e->content, strlen(e->content) + 1);
        e->content = newc;

        if (fmt_in) {
            char *nf = hu_strndup(c->alloc, fmt, strlen(fmt));
            if (!nf) {
                *out = hu_tool_result_fail("out of memory", 13);
                return HU_ERR_OUT_OF_MEMORY;
            }
            if (e->format)
                c->alloc->free(c->alloc->ctx, e->format, strlen(e->format) + 1);
            e->format = nf;
        }
        if (lang_in) {
            char *nl = hu_strndup(c->alloc, lang_in, strlen(lang_in));
            if (nl) {
                canvas_free_str(c->alloc, &e->language);
                e->language = nl;
            }
        }
        /* Clear user edit pending on agent update */
        e->user_edit_pending = false;
        canvas_free_str(c->alloc, &e->user_edited_content);

        hu_json_value_t *evt = canvas_evt_new(alloc, "update", e->canvas_id);
        if (evt) {
            hu_json_object_set(alloc, evt, "content",
                               hu_json_string_new(alloc, e->content, strlen(e->content)));
            hu_json_object_set(alloc, evt, "format",
                               hu_json_string_new(alloc, e->format, strlen(e->format)));
            hu_json_object_set(alloc, evt, "version_seq",
                               hu_json_number_new(alloc, (double)e->version_seq));
            if (e->language)
                hu_json_object_set(alloc, evt, "language",
                                   hu_json_string_new(alloc, e->language, strlen(e->language)));
            canvas_send_ui(alloc, evt);
        }

        char *msg = hu_sprintf(alloc, "{\"ok\":true,\"canvas_id\":\"%s\",\"version_seq\":%u}",
                               e->canvas_id, e->version_seq);
        if (!msg) {
            *out = hu_tool_result_fail("out of memory", 13);
            return HU_ERR_OUT_OF_MEMORY;
        }
        *out = hu_tool_result_ok_owned(msg, strlen(msg));
        return HU_OK;
    }

    if (strcmp(action, "clear") == 0) {
        if (e->content)
            c->alloc->free(c->alloc->ctx, e->content, strlen(e->content) + 1);
        e->content = hu_strndup(c->alloc, "", 0);
        if (!e->content) {
            *out = hu_tool_result_fail("out of memory", 13);
            return HU_ERR_OUT_OF_MEMORY;
        }

        hu_json_value_t *evt = canvas_evt_new(alloc, "clear", e->canvas_id);
        if (evt) {
            hu_json_object_set(alloc, evt, "format",
                               hu_json_string_new(alloc, e->format, strlen(e->format)));
            canvas_send_ui(alloc, evt);
        }

        char *msg = hu_sprintf(alloc, "{\"ok\":true,\"canvas_id\":\"%s\"}", e->canvas_id);
        if (!msg) {
            *out = hu_tool_result_fail("out of memory", 13);
            return HU_ERR_OUT_OF_MEMORY;
        }
        *out = hu_tool_result_ok_owned(msg, strlen(msg));
        return HU_OK;
    }

    if (strcmp(action, "close") == 0) {
        hu_json_value_t *evt = canvas_evt_new(alloc, "close", e->canvas_id);
        if (evt)
            canvas_send_ui(alloc, evt);

        canvas_free_entry(c, e);
        if (idx + 1 < c->count)
            c->slots[idx] = c->slots[c->count - 1];
        memset(&c->slots[c->count - 1], 0, sizeof(canvas_entry_t));
        c->count--;

        char *msg = hu_sprintf(alloc, "{\"ok\":true,\"closed\":true,\"canvas_id\":\"%s\"}",
                               canvas_id);
        if (!msg) {
            *out = hu_tool_result_fail("out of memory", 13);
            return HU_ERR_OUT_OF_MEMORY;
        }
        *out = hu_tool_result_ok_owned(msg, strlen(msg));
        return HU_OK;
    }

    *out = hu_tool_result_fail("unknown action", 14);
    return HU_OK;
}

static const char *canvas_name(void *ctx) {
    (void)ctx;
    return TOOL_NAME;
}

static const char *canvas_description(void *ctx) {
    (void)ctx;
    return TOOL_DESC;
}

static const char *canvas_parameters_json(void *ctx) {
    (void)ctx;
    return HU_CANVAS_PARAMS;
}

static void canvas_deinit(void *ctx, hu_allocator_t *alloc) {
    canvas_tool_ctx_t *c = (canvas_tool_ctx_t *)ctx;
    if (!c)
        return;
    for (size_t i = 0; i < c->count; i++)
        canvas_free_entry(c, &c->slots[i]);
    c->count = 0;
    alloc->free(alloc->ctx, c, sizeof(*c));
}

static const hu_tool_vtable_t canvas_vtable = {
    .execute = canvas_execute,
    .name = canvas_name,
    .description = canvas_description,
    .parameters_json = canvas_parameters_json,
    .deinit = canvas_deinit,
};

/* ---- Canvas store public API for RPC access ---- */

hu_canvas_store_t *hu_canvas_store_from_tool(hu_tool_t *tool) {
    if (!tool || !tool->ctx)
        return NULL;
    return (hu_canvas_store_t *)tool->ctx;
}

size_t hu_canvas_store_count(const hu_canvas_store_t *store) {
    const canvas_tool_ctx_t *c = (const canvas_tool_ctx_t *)store;
    return c ? c->count : 0;
}

bool hu_canvas_store_get(const hu_canvas_store_t *store, size_t index, hu_canvas_info_t *info) {
    const canvas_tool_ctx_t *c = (const canvas_tool_ctx_t *)store;
    if (!c || !info || index >= c->count)
        return false;
    const canvas_entry_t *e = &c->slots[index];
    info->canvas_id = e->canvas_id;
    info->title = e->title;
    info->format = e->format;
    info->content = e->content;
    info->language = e->language;
    info->imports = e->imports;
    info->version_seq = e->version_seq;
    info->version_count = e->version_count;
    info->user_edit_pending = e->user_edit_pending;
    info->user_edited_content = e->user_edited_content;
    return true;
}

bool hu_canvas_store_find(const hu_canvas_store_t *store, const char *canvas_id,
                          hu_canvas_info_t *info) {
    const canvas_tool_ctx_t *c = (const canvas_tool_ctx_t *)store;
    if (!c || !canvas_id || !info)
        return false;
    size_t idx = canvas_find_index((canvas_tool_ctx_t *)c, canvas_id);
    if (idx == (size_t)-1)
        return false;
    return hu_canvas_store_get(store, idx, info);
}

hu_error_t hu_canvas_store_edit(hu_canvas_store_t *store, const char *canvas_id,
                                const char *new_content) {
    canvas_tool_ctx_t *c = (canvas_tool_ctx_t *)store;
    if (!c || !canvas_id || !new_content)
        return HU_ERR_INVALID_ARGUMENT;
    size_t idx = canvas_find_index(c, canvas_id);
    if (idx == (size_t)-1)
        return HU_ERR_NOT_FOUND;
    canvas_entry_t *e = &c->slots[idx];
    canvas_free_str(c->alloc, &e->user_edited_content);
    e->user_edited_content = hu_strndup(c->alloc, new_content, strlen(new_content));
    e->user_edit_pending = true;
    return HU_OK;
}

hu_error_t hu_canvas_store_undo(hu_canvas_store_t *store, const char *canvas_id,
                                hu_canvas_info_t *info) {
    canvas_tool_ctx_t *c = (canvas_tool_ctx_t *)store;
    if (!c || !canvas_id)
        return HU_ERR_INVALID_ARGUMENT;
    size_t idx = canvas_find_index(c, canvas_id);
    if (idx == (size_t)-1)
        return HU_ERR_NOT_FOUND;
    canvas_entry_t *e = &c->slots[idx];
    hu_error_t err = hu_canvas_entry_undo(c, e);
    if (err != HU_OK)
        return err;
    if (info)
        hu_canvas_store_get(store, idx, info);
    return HU_OK;
}

hu_error_t hu_canvas_store_redo(hu_canvas_store_t *store, const char *canvas_id,
                                hu_canvas_info_t *info) {
    canvas_tool_ctx_t *c = (canvas_tool_ctx_t *)store;
    if (!c || !canvas_id)
        return HU_ERR_INVALID_ARGUMENT;
    size_t idx = canvas_find_index(c, canvas_id);
    if (idx == (size_t)-1)
        return HU_ERR_NOT_FOUND;
    canvas_entry_t *e = &c->slots[idx];
    hu_error_t err = hu_canvas_entry_redo(c, e);
    if (err != HU_OK)
        return err;
    if (info)
        hu_canvas_store_get(store, idx, info);
    return HU_OK;
}

hu_error_t hu_canvas_tool_create(hu_allocator_t *alloc, hu_tool_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    canvas_tool_ctx_t *c = (canvas_tool_ctx_t *)alloc->alloc(alloc->ctx, sizeof(*c));
    if (!c)
        return HU_ERR_OUT_OF_MEMORY;
    memset(c, 0, sizeof(*c));
    c->alloc = alloc;
    out->ctx = c;
    out->vtable = &canvas_vtable;
    return HU_OK;
}
