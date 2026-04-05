#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/json.h"
#include "human/tool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TOOL_NAME "workflow"
#define TOOL_DESC                                                                            \
    "Multi-step workflow engine with approval gates. Actions: create (define workflow with " \
    "ordered steps), run (execute a workflow), status (check workflow progress), approve "   \
    "(approve a pending step), list (show all workflows), cancel (abort a running workflow)."
#define TOOL_PARAMS                                                                                \
    "{\"type\":\"object\",\"properties\":{\"action\":{\"type\":\"string\",\"enum\":[\"create\","   \
    "\"run\",\"status\",\"approve\",\"list\",\"cancel\"]},\"workflow_id\":{\"type\":\"string\"},"  \
    "\"name\":{\"type\":\"string\"},\"steps\":{\"type\":\"array\",\"items\":{\"type\":\"object\"," \
    "\"properties\":{\"name\":{\"type\":\"string\"},\"tool\":{\"type\":\"string\"},"               \
    "\"args\":{\"type\":\"object\"},\"requires_approval\":{\"type\":\"boolean\"}}}},\"step_id\""   \
    ":{\"type\":\"string\"},\"trigger\":{\"type\":\"string\",\"description\":"                     \
    "\"cron expression or event name for auto-trigger\"}},\"required\":[\"action\"]}"

#define WF_MAX       16
#define WF_STEPS_MAX 32

typedef enum {
    WF_STATUS_CREATED,
    WF_STATUS_RUNNING,
    WF_STATUS_WAITING_APPROVAL,
    WF_STATUS_COMPLETED,
    WF_STATUS_CANCELLED,
    WF_STATUS_FAILED
} wf_status_t;

typedef struct {
    char name[128];
    char tool[64];
    bool requires_approval;
    bool completed;
    bool approved;
} wf_step_t;

typedef struct {
    char id[32];
    char name[128];
    wf_step_t steps[WF_STEPS_MAX];
    size_t step_count;
    size_t current_step;
    wf_status_t status;
} wf_def_t;

typedef struct {
    hu_allocator_t *alloc;
    wf_def_t workflows[WF_MAX];
    size_t count;
    uint32_t next_id;
} workflow_ctx_t;

static hu_error_t wf_json_set_str(hu_allocator_t *alloc, hu_json_value_t *obj, const char *key,
                                  const char *s, size_t slen) {
    hu_json_value_t *v = hu_json_string_new(alloc, s, slen);
    if (!v)
        return HU_ERR_OUT_OF_MEMORY;
    if (hu_json_object_set(alloc, obj, key, v) != HU_OK) {
        hu_json_free(alloc, v);
        return HU_ERR_OUT_OF_MEMORY;
    }
    return HU_OK;
}

static hu_error_t wf_json_set_num(hu_allocator_t *alloc, hu_json_value_t *obj, const char *key,
                                  double n) {
    hu_json_value_t *v = hu_json_number_new(alloc, n);
    if (!v)
        return HU_ERR_OUT_OF_MEMORY;
    if (hu_json_object_set(alloc, obj, key, v) != HU_OK) {
        hu_json_free(alloc, v);
        return HU_ERR_OUT_OF_MEMORY;
    }
    return HU_OK;
}

static void wf_emit_tool_json(hu_allocator_t *alloc, hu_tool_result_t *out, hu_json_value_t *root) {
    char *msg = NULL;
    size_t msg_len = 0;
    hu_error_t jerr = hu_json_stringify(alloc, root, &msg, &msg_len);
    hu_json_free(alloc, root);
    if (jerr != HU_OK || !msg) {
        *out = hu_tool_result_fail("out of memory", 13);
        return;
    }
    *out = hu_tool_result_ok_owned(msg, msg_len);
}

static const char *wf_status_str(wf_status_t s) {
    switch (s) {
    case WF_STATUS_CREATED:
        return "created";
    case WF_STATUS_RUNNING:
        return "running";
    case WF_STATUS_WAITING_APPROVAL:
        return "waiting_approval";
    case WF_STATUS_COMPLETED:
        return "completed";
    case WF_STATUS_CANCELLED:
        return "cancelled";
    case WF_STATUS_FAILED:
        return "failed";
    }
    return "unknown";
}

static wf_def_t *wf_find(workflow_ctx_t *c, const char *id) {
    for (size_t i = 0; i < c->count; i++)
        if (strcmp(c->workflows[i].id, id) == 0)
            return &c->workflows[i];
    return NULL;
}

static hu_error_t workflow_execute(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args,
                                   hu_tool_result_t *out) {
    workflow_ctx_t *c = (workflow_ctx_t *)ctx;
    if (!out)
        return HU_ERR_INVALID_ARGUMENT;
    if (!args) {
        *out = hu_tool_result_fail("invalid args", 12);
        return HU_ERR_INVALID_ARGUMENT;
    }
    const char *action = hu_json_get_string(args, "action");
    if (!action) {
        *out = hu_tool_result_fail("missing action", 14);
        return HU_OK;
    }

    if (strcmp(action, "create") == 0) {
        if (c->count >= WF_MAX) {
            *out = hu_tool_result_fail("workflow limit reached", 21);
            return HU_OK;
        }
        const char *name = hu_json_get_string(args, "name");
        wf_def_t *wf = &c->workflows[c->count];
        memset(wf, 0, sizeof(*wf));
        snprintf(wf->id, sizeof(wf->id), "wf_%u", c->next_id++);
        if (name) {
            size_t nl = strlen(name);
            if (nl > sizeof(wf->name) - 1)
                nl = sizeof(wf->name) - 1;
            memcpy(wf->name, name, nl);
            wf->name[nl] = '\0';
        }

        hu_json_value_t *steps = hu_json_object_get((hu_json_value_t *)args, "steps");
        if (steps && steps->type == HU_JSON_ARRAY) {
            for (size_t i = 0; i < steps->data.array.len && wf->step_count < WF_STEPS_MAX; i++) {
                hu_json_value_t *s = steps->data.array.items[i];
                if (!s)
                    continue;
                wf_step_t *step = &wf->steps[wf->step_count];
                memset(step, 0, sizeof(*step));
                const char *sname = hu_json_get_string(s, "name");
                const char *stool = hu_json_get_string(s, "tool");
                if (sname) {
                    size_t sl = strlen(sname);
                    if (sl > sizeof(step->name) - 1)
                        sl = sizeof(step->name) - 1;
                    memcpy(step->name, sname, sl);
                }
                if (stool) {
                    size_t tl = strlen(stool);
                    if (tl > sizeof(step->tool) - 1)
                        tl = sizeof(step->tool) - 1;
                    memcpy(step->tool, stool, tl);
                }
                hu_json_value_t *ra = hu_json_object_get(s, "requires_approval");
                step->requires_approval = (ra && ra->type == HU_JSON_BOOL && ra->data.boolean);
                wf->step_count++;
            }
        }
        wf->status = WF_STATUS_CREATED;
        c->count++;
        hu_json_value_t *cj = hu_json_object_new(alloc);
        if (!cj) {
            *out = hu_tool_result_fail("out of memory", 13);
            return HU_OK;
        }
        hu_json_value_t *t = hu_json_bool_new(alloc, true);
        if (!t || hu_json_object_set(alloc, cj, "created", t) != HU_OK) {
            if (t)
                hu_json_free(alloc, t);
            hu_json_free(alloc, cj);
            *out = hu_tool_result_fail("out of memory", 13);
            return HU_OK;
        }
        if (wf_json_set_str(alloc, cj, "workflow_id", wf->id,
                            strnlen(wf->id, sizeof(wf->id))) != HU_OK ||
            wf_json_set_num(alloc, cj, "steps", (double)wf->step_count) != HU_OK) {
            hu_json_free(alloc, cj);
            *out = hu_tool_result_fail("out of memory", 13);
            return HU_OK;
        }
        wf_emit_tool_json(alloc, out, cj);
        return HU_OK;
    }

    if (strcmp(action, "run") == 0) {
        const char *wf_id = hu_json_get_string(args, "workflow_id");
        if (!wf_id) {
            *out = hu_tool_result_fail("missing workflow_id", 18);
            return HU_OK;
        }
        wf_def_t *wf = wf_find(c, wf_id);
        if (!wf) {
            *out = hu_tool_result_fail("workflow not found", 18);
            return HU_OK;
        }
        if (wf->status != WF_STATUS_CREATED) {
            *out = hu_tool_result_fail("workflow already started", 24);
            return HU_OK;
        }
        wf->status = WF_STATUS_RUNNING;
        wf->current_step = 0;
        while (wf->current_step < wf->step_count) {
            wf_step_t *step = &wf->steps[wf->current_step];
            if (step->requires_approval && !step->approved) {
                wf->status = WF_STATUS_WAITING_APPROVAL;
                hu_json_value_t *rj = hu_json_object_new(alloc);
                if (!rj) {
                    *out = hu_tool_result_fail("out of memory", 13);
                    return HU_OK;
                }
                if (wf_json_set_str(alloc, rj, "status", "waiting_approval",
                                    sizeof("waiting_approval") - 1) != HU_OK ||
                    wf_json_set_str(alloc, rj, "step", step->name,
                                    strnlen(step->name, sizeof(step->name))) != HU_OK ||
                    wf_json_set_num(alloc, rj, "step_index", (double)wf->current_step) != HU_OK) {
                    hu_json_free(alloc, rj);
                    *out = hu_tool_result_fail("out of memory", 13);
                    return HU_OK;
                }
                wf_emit_tool_json(alloc, out, rj);
                return HU_OK;
            }
            step->completed = true;
            wf->current_step++;
        }
        wf->status = WF_STATUS_COMPLETED;
        *out = hu_tool_result_ok("{\"status\":\"completed\"}", 23);
        return HU_OK;
    }

    if (strcmp(action, "approve") == 0) {
        const char *wf_id = hu_json_get_string(args, "workflow_id");
        if (!wf_id) {
            *out = hu_tool_result_fail("missing workflow_id", 18);
            return HU_OK;
        }
        wf_def_t *wf = wf_find(c, wf_id);
        if (!wf) {
            *out = hu_tool_result_fail("workflow not found", 18);
            return HU_OK;
        }
        if (wf->status != WF_STATUS_WAITING_APPROVAL) {
            *out = hu_tool_result_fail("workflow not waiting for approval", 32);
            return HU_OK;
        }
        wf->steps[wf->current_step].approved = true;
        wf->steps[wf->current_step].completed = true;
        wf->current_step++;
        while (wf->current_step < wf->step_count) {
            wf_step_t *step = &wf->steps[wf->current_step];
            if (step->requires_approval && !step->approved) {
                wf->status = WF_STATUS_WAITING_APPROVAL;
                hu_json_value_t *aj = hu_json_object_new(alloc);
                if (!aj) {
                    *out = hu_tool_result_fail("out of memory", 13);
                    return HU_OK;
                }
                hu_json_value_t *ap = hu_json_bool_new(alloc, true);
                if (!ap || hu_json_object_set(alloc, aj, "approved", ap) != HU_OK) {
                    if (ap)
                        hu_json_free(alloc, ap);
                    hu_json_free(alloc, aj);
                    *out = hu_tool_result_fail("out of memory", 13);
                    return HU_OK;
                }
                if (wf_json_set_str(alloc, aj, "next_step", step->name,
                                    strnlen(step->name, sizeof(step->name))) != HU_OK ||
                    wf_json_set_str(alloc, aj, "status", "waiting_approval",
                                    sizeof("waiting_approval") - 1) != HU_OK) {
                    hu_json_free(alloc, aj);
                    *out = hu_tool_result_fail("out of memory", 13);
                    return HU_OK;
                }
                wf_emit_tool_json(alloc, out, aj);
                return HU_OK;
            }
            step->completed = true;
            wf->current_step++;
        }
        wf->status = WF_STATUS_COMPLETED;
        *out = hu_tool_result_ok("{\"approved\":true,\"status\":\"completed\"}", 40);
        return HU_OK;
    }

    if (strcmp(action, "status") == 0) {
        const char *wf_id = hu_json_get_string(args, "workflow_id");
        if (!wf_id) {
            *out = hu_tool_result_fail("missing workflow_id", 18);
            return HU_OK;
        }
        wf_def_t *wf = wf_find(c, wf_id);
        if (!wf) {
            *out = hu_tool_result_fail("workflow not found", 18);
            return HU_OK;
        }
        hu_json_value_t *sj = hu_json_object_new(alloc);
        if (!sj) {
            *out = hu_tool_result_fail("out of memory", 13);
            return HU_OK;
        }
        const char *st = wf_status_str(wf->status);
        if (wf_json_set_str(alloc, sj, "workflow_id", wf->id,
                            strnlen(wf->id, sizeof(wf->id))) != HU_OK ||
            wf_json_set_str(alloc, sj, "name", wf->name,
                            strnlen(wf->name, sizeof(wf->name))) != HU_OK ||
            wf_json_set_str(alloc, sj, "status", st, strlen(st)) != HU_OK ||
            wf_json_set_num(alloc, sj, "current_step", (double)wf->current_step) != HU_OK ||
            wf_json_set_num(alloc, sj, "total_steps", (double)wf->step_count) != HU_OK) {
            hu_json_free(alloc, sj);
            *out = hu_tool_result_fail("out of memory", 13);
            return HU_OK;
        }
        wf_emit_tool_json(alloc, out, sj);
        return HU_OK;
    }

    if (strcmp(action, "list") == 0) {
        hu_json_value_t *lj = hu_json_object_new(alloc);
        if (!lj) {
            *out = hu_tool_result_fail("out of memory", 13);
            return HU_OK;
        }
        hu_json_value_t *warr = hu_json_array_new(alloc);
        if (!warr) {
            hu_json_free(alloc, lj);
            *out = hu_tool_result_fail("out of memory", 13);
            return HU_OK;
        }
        for (size_t i = 0; i < c->count; i++) {
            wf_def_t *wf = &c->workflows[i];
            hu_json_value_t *el = hu_json_object_new(alloc);
            if (!el) {
                hu_json_free(alloc, warr);
                hu_json_free(alloc, lj);
                *out = hu_tool_result_fail("out of memory", 13);
                return HU_OK;
            }
            const char *st = wf_status_str(wf->status);
            if (wf_json_set_str(alloc, el, "id", wf->id, strnlen(wf->id, sizeof(wf->id))) !=
                    HU_OK ||
                wf_json_set_str(alloc, el, "name", wf->name,
                                strnlen(wf->name, sizeof(wf->name))) != HU_OK ||
                wf_json_set_str(alloc, el, "status", st, strlen(st)) != HU_OK ||
                wf_json_set_num(alloc, el, "steps", (double)wf->step_count) != HU_OK) {
                hu_json_free(alloc, el);
                hu_json_free(alloc, warr);
                hu_json_free(alloc, lj);
                *out = hu_tool_result_fail("out of memory", 13);
                return HU_OK;
            }
            if (hu_json_array_push(alloc, warr, el) != HU_OK) {
                hu_json_free(alloc, el);
                hu_json_free(alloc, warr);
                hu_json_free(alloc, lj);
                *out = hu_tool_result_fail("out of memory", 13);
                return HU_OK;
            }
        }
        if (hu_json_object_set(alloc, lj, "workflows", warr) != HU_OK) {
            hu_json_free(alloc, warr);
            hu_json_free(alloc, lj);
            *out = hu_tool_result_fail("out of memory", 13);
            return HU_OK;
        }
        wf_emit_tool_json(alloc, out, lj);
        return HU_OK;
    }

    if (strcmp(action, "cancel") == 0) {
        const char *wf_id = hu_json_get_string(args, "workflow_id");
        if (!wf_id) {
            *out = hu_tool_result_fail("missing workflow_id", 18);
            return HU_OK;
        }
        wf_def_t *wf = wf_find(c, wf_id);
        if (!wf) {
            *out = hu_tool_result_fail("workflow not found", 18);
            return HU_OK;
        }
        wf->status = WF_STATUS_CANCELLED;
        *out = hu_tool_result_ok("{\"cancelled\":true}", 19);
        return HU_OK;
    }

    *out = hu_tool_result_fail("unknown action", 14);
    return HU_OK;
}

static const char *workflow_name(void *ctx) {
    (void)ctx;
    return TOOL_NAME;
}
static const char *workflow_desc(void *ctx) {
    (void)ctx;
    return TOOL_DESC;
}
static const char *workflow_params(void *ctx) {
    (void)ctx;
    return TOOL_PARAMS;
}
static void workflow_deinit(void *ctx, hu_allocator_t *alloc) {
    if (ctx && alloc)
        alloc->free(alloc->ctx, ctx, sizeof(workflow_ctx_t));
}

static const hu_tool_vtable_t workflow_vtable = {
    .execute = workflow_execute,
    .name = workflow_name,
    .description = workflow_desc,
    .parameters_json = workflow_params,
    .deinit = workflow_deinit,
};

hu_error_t hu_workflow_create(hu_allocator_t *alloc, hu_tool_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    workflow_ctx_t *c = (workflow_ctx_t *)alloc->alloc(alloc->ctx, sizeof(*c));
    if (!c)
        return HU_ERR_OUT_OF_MEMORY;
    memset(c, 0, sizeof(*c));
    c->alloc = alloc;
    out->ctx = c;
    out->vtable = &workflow_vtable;
    return HU_OK;
}
