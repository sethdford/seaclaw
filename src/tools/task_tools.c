#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include "human/task_manager.h"
#include "human/tool.h"
#include <stdio.h>
#include <string.h>

/* ──────────────────────────────────────────────────────────────────────────
 * task_create: Creates a new task
 * ────────────────────────────────────────────────────────────────────────── */

#define HU_TASK_CREATE_NAME "task_create"
#define HU_TASK_CREATE_DESC "Create a new task"
#define HU_TASK_CREATE_PARAMS                                                                      \
    "{\"type\":\"object\",\"properties\":{\"subject\":{\"type\":\"string\",\"description\":"      \
    "\"Task subject\"},\"description\":{\"type\":\"string\",\"description\":\"Task description\"" \
    "}},\"required\":[\"subject\",\"description\"]}"

typedef struct hu_task_create_ctx {
    hu_task_manager_t *task_manager;
    hu_allocator_t *alloc;
} hu_task_create_ctx_t;

static hu_error_t task_create_execute(void *ctx, hu_allocator_t *alloc,
                                      const hu_json_value_t *args, hu_tool_result_t *out) {
    hu_task_create_ctx_t *c = (hu_task_create_ctx_t *)ctx;
    if (!c || !args || !out) {
        *out = hu_tool_result_fail("invalid args", 12);
        return HU_ERR_INVALID_ARGUMENT;
    }

    const char *subject = hu_json_get_string(args, "subject");
    const char *description = hu_json_get_string(args, "description");

    if (!subject || strlen(subject) == 0 || !description || strlen(description) == 0) {
        *out = hu_tool_result_fail("missing subject or description", 30);
        return HU_OK;
    }

    uint32_t task_id = 0;
    hu_error_t err = hu_task_manager_add(c->task_manager, alloc, subject, strlen(subject),
                                         description, strlen(description), &task_id);
    if (err != HU_OK) {
        *out = hu_tool_result_fail("failed to create task", 21);
        return HU_OK;
    }

    hu_json_buf_t jb = {0};
    hu_error_t jerr = hu_json_buf_init(&jb, alloc);
    if (jerr != HU_OK)
        return jerr;
    char id_prefix[32];
    int pn = snprintf(id_prefix, sizeof(id_prefix), "{\"id\":%u,\"subject\":", task_id);
    if (pn > 0)
        hu_json_buf_append_raw(&jb, id_prefix, (size_t)pn);
    hu_json_append_string(&jb, subject, strlen(subject));
    hu_json_buf_append_raw(&jb, ",\"status\":\"pending\"}", 20);
    char *response = jb.ptr;
    size_t response_len = jb.len;
    *out = hu_tool_result_ok_owned(response, response_len);
    return HU_OK;
}

static const char *task_create_name(void *ctx) {
    (void)ctx;
    return HU_TASK_CREATE_NAME;
}

static const char *task_create_description(void *ctx) {
    (void)ctx;
    return HU_TASK_CREATE_DESC;
}

static const char *task_create_parameters_json(void *ctx) {
    (void)ctx;
    return HU_TASK_CREATE_PARAMS;
}

static void task_create_deinit(void *ctx, hu_allocator_t *alloc) {
    hu_task_create_ctx_t *c = (hu_task_create_ctx_t *)ctx;
    if (c) {
        alloc->free(alloc->ctx, c, sizeof(hu_task_create_ctx_t));
    }
}

HU_TOOL_IMPL(hu_tool_task_create, task_create_execute, task_create_name, task_create_description,
             task_create_parameters_json, task_create_deinit);

/* ──────────────────────────────────────────────────────────────────────────
 * task_update: Updates task status
 * ────────────────────────────────────────────────────────────────────────── */

#define HU_TASK_UPDATE_NAME "task_update"
#define HU_TASK_UPDATE_DESC "Update task status"
#define HU_TASK_UPDATE_PARAMS                                                                      \
    "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"number\",\"description\":"           \
    "\"Task ID\"},\"status\":{\"type\":\"string\",\"description\":\"New status: "                 \
    "pending/in_progress/completed\"}},\"required\":[\"id\",\"status\"]}"

typedef struct hu_task_update_ctx {
    hu_task_manager_t *task_manager;
    hu_allocator_t *alloc;
} hu_task_update_ctx_t;

static hu_error_t task_update_execute(void *ctx, hu_allocator_t *alloc,
                                      const hu_json_value_t *args, hu_tool_result_t *out) {
    hu_task_update_ctx_t *c = (hu_task_update_ctx_t *)ctx;
    if (!c || !args || !out) {
        *out = hu_tool_result_fail("invalid args", 12);
        return HU_ERR_INVALID_ARGUMENT;
    }

    double id_val = hu_json_get_number(args, "id", 0);
    const char *status_str = hu_json_get_string(args, "status");

    if (id_val <= 0 || !status_str || strlen(status_str) == 0) {
        *out = hu_tool_result_fail("missing id or status", 20);
        return HU_OK;
    }

    uint32_t task_id = (uint32_t)id_val;

    hu_task_status_t status;
    if (strcmp(status_str, "pending") == 0) {
        status = HU_TASK_PENDING;
    } else if (strcmp(status_str, "in_progress") == 0) {
        status = HU_TASK_IN_PROGRESS;
    } else if (strcmp(status_str, "completed") == 0) {
        status = HU_TASK_COMPLETED;
    } else {
        *out = hu_tool_result_fail("invalid status value", 20);
        return HU_OK;
    }

    hu_error_t err = hu_task_manager_update_status(c->task_manager, task_id, status);
    if (err != HU_OK) {
        *out = hu_tool_result_fail("task not found", 14);
        return HU_OK;
    }

    const hu_task_t *task = NULL;
    hu_task_manager_get(c->task_manager, task_id, &task);

    hu_json_buf_t jb = {0};
    hu_error_t jerr = hu_json_buf_init(&jb, alloc);
    if (jerr != HU_OK)
        return jerr;
    char id_prefix[32];
    int pn = snprintf(id_prefix, sizeof(id_prefix), "{\"id\":%u,\"status\":", task_id);
    if (pn > 0)
        hu_json_buf_append_raw(&jb, id_prefix, (size_t)pn);
    hu_json_append_string(&jb, status_str, strlen(status_str));
    hu_json_buf_append_raw(&jb, ",\"updated\":true}", 16);
    char *response = jb.ptr;
    size_t response_len = jb.len;
    *out = hu_tool_result_ok_owned(response, response_len);
    return HU_OK;
}

static const char *task_update_name(void *ctx) {
    (void)ctx;
    return HU_TASK_UPDATE_NAME;
}

static const char *task_update_description(void *ctx) {
    (void)ctx;
    return HU_TASK_UPDATE_DESC;
}

static const char *task_update_parameters_json(void *ctx) {
    (void)ctx;
    return HU_TASK_UPDATE_PARAMS;
}

static void task_update_deinit(void *ctx, hu_allocator_t *alloc) {
    hu_task_update_ctx_t *c = (hu_task_update_ctx_t *)ctx;
    if (c) {
        alloc->free(alloc->ctx, c, sizeof(hu_task_update_ctx_t));
    }
}

HU_TOOL_IMPL(hu_tool_task_update, task_update_execute, task_update_name, task_update_description,
             task_update_parameters_json, task_update_deinit);

/* ──────────────────────────────────────────────────────────────────────────
 * task_list: Lists all tasks
 * ────────────────────────────────────────────────────────────────────────── */

#define HU_TASK_LIST_NAME "task_list"
#define HU_TASK_LIST_DESC "List all tasks"
#define HU_TASK_LIST_PARAMS                                                                        \
    "{\"type\":\"object\",\"properties\":{},\"required\":[]}"

typedef struct hu_task_list_ctx {
    hu_task_manager_t *task_manager;
    hu_allocator_t *alloc;
} hu_task_list_ctx_t;

static hu_error_t task_list_execute(void *ctx, hu_allocator_t *alloc,
                                    const hu_json_value_t *args, hu_tool_result_t *out) {
    hu_task_list_ctx_t *c = (hu_task_list_ctx_t *)ctx;
    if (!c || !out) {
        *out = hu_tool_result_fail("invalid args", 12);
        return HU_ERR_INVALID_ARGUMENT;
    }

    (void)args; /* unused */

    char *json = NULL;
    size_t json_len = 0;
    hu_error_t err = hu_task_manager_list(c->task_manager, alloc, &json, &json_len);
    if (err != HU_OK) {
        *out = hu_tool_result_fail("failed to list tasks", 20);
        return HU_OK;
    }

    *out = hu_tool_result_ok_owned(json, json_len);
    return HU_OK;
}

static const char *task_list_name(void *ctx) {
    (void)ctx;
    return HU_TASK_LIST_NAME;
}

static const char *task_list_description(void *ctx) {
    (void)ctx;
    return HU_TASK_LIST_DESC;
}

static const char *task_list_parameters_json(void *ctx) {
    (void)ctx;
    return HU_TASK_LIST_PARAMS;
}

static void task_list_deinit(void *ctx, hu_allocator_t *alloc) {
    hu_task_list_ctx_t *c = (hu_task_list_ctx_t *)ctx;
    if (c) {
        alloc->free(alloc->ctx, c, sizeof(hu_task_list_ctx_t));
    }
}

HU_TOOL_IMPL(hu_tool_task_list, task_list_execute, task_list_name, task_list_description,
             task_list_parameters_json, task_list_deinit);

/* ──────────────────────────────────────────────────────────────────────────
 * task_get: Gets a specific task by ID
 * ────────────────────────────────────────────────────────────────────────── */

#define HU_TASK_GET_NAME "task_get"
#define HU_TASK_GET_DESC "Get a specific task by ID"
#define HU_TASK_GET_PARAMS                                                                         \
    "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"number\",\"description\":"           \
    "\"Task ID\"}},\"required\":[\"id\"]}"

typedef struct hu_task_get_ctx {
    hu_task_manager_t *task_manager;
    hu_allocator_t *alloc;
} hu_task_get_ctx_t;

static hu_error_t task_get_execute(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args,
                                   hu_tool_result_t *out) {
    hu_task_get_ctx_t *c = (hu_task_get_ctx_t *)ctx;
    if (!c || !args || !out) {
        *out = hu_tool_result_fail("invalid args", 12);
        return HU_ERR_INVALID_ARGUMENT;
    }

    double id_val = hu_json_get_number(args, "id", 0);
    if (id_val <= 0) {
        *out = hu_tool_result_fail("missing or invalid id", 21);
        return HU_OK;
    }

    uint32_t task_id = (uint32_t)id_val;
    const hu_task_t *task = NULL;
    hu_error_t err = hu_task_manager_get(c->task_manager, task_id, &task);
    if (err != HU_OK) {
        *out = hu_tool_result_fail("task not found", 14);
        return HU_OK;
    }

    const char *status_str;
    switch (task->status) {
    case HU_TASK_PENDING:
        status_str = "pending";
        break;
    case HU_TASK_IN_PROGRESS:
        status_str = "in_progress";
        break;
    case HU_TASK_COMPLETED:
        status_str = "completed";
        break;
    default:
        status_str = "unknown";
    }

    hu_json_buf_t jb = {0};
    hu_error_t jerr = hu_json_buf_init(&jb, alloc);
    if (jerr != HU_OK) {
        *out = hu_tool_result_fail("out of memory", 13);
        return HU_ERR_OUT_OF_MEMORY;
    }
    char id_prefix[32];
    int pn = snprintf(id_prefix, sizeof(id_prefix), "{\"id\":%u,\"subject\":", task->id);
    if (pn > 0)
        hu_json_buf_append_raw(&jb, id_prefix, (size_t)pn);
    const char *subj = task->subject ? task->subject : "";
    size_t subj_len = task->subject ? task->subject_len : 0;
    hu_json_append_string(&jb, subj, subj_len);
    hu_json_buf_append_raw(&jb, ",\"description\":", 15);
    const char *desc = task->description ? task->description : "";
    size_t desc_len = task->description ? task->description_len : 0;
    hu_json_append_string(&jb, desc, desc_len);
    hu_json_buf_append_raw(&jb, ",\"status\":", 10);
    hu_json_append_string(&jb, status_str, strlen(status_str));
    hu_json_buf_append_raw(&jb, "}", 1);
    *out = hu_tool_result_ok_owned(jb.ptr, jb.len);
    return HU_OK;
}

static const char *task_get_name(void *ctx) {
    (void)ctx;
    return HU_TASK_GET_NAME;
}

static const char *task_get_description(void *ctx) {
    (void)ctx;
    return HU_TASK_GET_DESC;
}

static const char *task_get_parameters_json(void *ctx) {
    (void)ctx;
    return HU_TASK_GET_PARAMS;
}

static void task_get_deinit(void *ctx, hu_allocator_t *alloc) {
    hu_task_get_ctx_t *c = (hu_task_get_ctx_t *)ctx;
    if (c) {
        alloc->free(alloc->ctx, c, sizeof(hu_task_get_ctx_t));
    }
}

HU_TOOL_IMPL(hu_tool_task_get, task_get_execute, task_get_name, task_get_description,
             task_get_parameters_json, task_get_deinit);

/* ──────────────────────────────────────────────────────────────────────────
 * Tool creators
 * ────────────────────────────────────────────────────────────────────────── */

hu_tool_t hu_tool_task_create(hu_allocator_t *alloc, hu_task_manager_t *task_manager) {
    hu_task_create_ctx_t *ctx =
        (hu_task_create_ctx_t *)alloc->alloc(alloc->ctx, sizeof(hu_task_create_ctx_t));
    if (!ctx) {
        return (hu_tool_t){.ctx = NULL, .vtable = NULL};
    }
    ctx->task_manager = task_manager;
    ctx->alloc = alloc;
    return (hu_tool_t){.ctx = ctx, .vtable = &hu_tool_task_create_vtable};
}

hu_tool_t hu_tool_task_update(hu_allocator_t *alloc, hu_task_manager_t *task_manager) {
    hu_task_update_ctx_t *ctx =
        (hu_task_update_ctx_t *)alloc->alloc(alloc->ctx, sizeof(hu_task_update_ctx_t));
    if (!ctx) {
        return (hu_tool_t){.ctx = NULL, .vtable = NULL};
    }
    ctx->task_manager = task_manager;
    ctx->alloc = alloc;
    return (hu_tool_t){.ctx = ctx, .vtable = &hu_tool_task_update_vtable};
}

hu_tool_t hu_tool_task_list(hu_allocator_t *alloc, hu_task_manager_t *task_manager) {
    hu_task_list_ctx_t *ctx =
        (hu_task_list_ctx_t *)alloc->alloc(alloc->ctx, sizeof(hu_task_list_ctx_t));
    if (!ctx) {
        return (hu_tool_t){.ctx = NULL, .vtable = NULL};
    }
    ctx->task_manager = task_manager;
    ctx->alloc = alloc;
    return (hu_tool_t){.ctx = ctx, .vtable = &hu_tool_task_list_vtable};
}

hu_tool_t hu_tool_task_get(hu_allocator_t *alloc, hu_task_manager_t *task_manager) {
    hu_task_get_ctx_t *ctx =
        (hu_task_get_ctx_t *)alloc->alloc(alloc->ctx, sizeof(hu_task_get_ctx_t));
    if (!ctx) {
        return (hu_tool_t){.ctx = NULL, .vtable = NULL};
    }
    ctx->task_manager = task_manager;
    ctx->alloc = alloc;
    return (hu_tool_t){.ctx = ctx, .vtable = &hu_tool_task_get_vtable};
}
