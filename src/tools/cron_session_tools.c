#include "human/tools/cron_session_tools.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* ──────────────────────────────────────────────────────────────────────────
 * cron_create tool: Create a scheduled job
 * ────────────────────────────────────────────────────────────────────────── */

#define CRON_CREATE_NAME "cron_create"
#define CRON_CREATE_DESC "Create a new scheduled cron job"
#define CRON_CREATE_PARAMS                                                                 \
    "{\"type\":\"object\",\"properties\":{" \
    "\"cron_expr\":{\"type\":\"string\",\"description\":\"Cron expression (e.g., \\\"*/5 * * * *\\\")\"}," \
    "\"prompt\":{\"type\":\"string\",\"description\":\"Prompt to execute\"}," \
    "\"recurring\":{\"type\":\"boolean\",\"description\":\"If true, repeat; if false, one-shot\",\"default\":true}" \
    "},\"required\":[\"cron_expr\",\"prompt\"]}"

typedef struct {
    hu_allocator_t *alloc;
    hu_cron_scheduler_t *scheduler;
} cron_session_ctx_t;

static hu_error_t cron_create_ok_json(hu_allocator_t *alloc, uint64_t job_id, const char *cron_expr,
                                      const char *prompt, bool with_recurring, bool recurring_val,
                                      hu_tool_result_t *out) {
    hu_json_value_t *root = hu_json_object_new(alloc);
    if (!root) {
        *out = hu_tool_result_fail("out of memory", 13);
        return HU_OK;
    }
#define CRON_SET(field, val_expr)                                                                  \
    do {                                                                                           \
        hu_json_value_t *_v = (val_expr);                                                          \
        if (!_v) {                                                                                 \
            hu_json_free(alloc, root);                                                             \
            *out = hu_tool_result_fail("out of memory", 13);                                       \
            return HU_OK;                                                                          \
        }                                                                                          \
        if (hu_json_object_set(alloc, root, (field), _v) != HU_OK) {                               \
            hu_json_free(alloc, _v);                                                               \
            hu_json_free(alloc, root);                                                             \
            *out = hu_tool_result_fail("out of memory", 13);                                       \
            return HU_OK;                                                                          \
        }                                                                                          \
    } while (0)

    CRON_SET("id", hu_json_number_new(alloc, (double)job_id));
    CRON_SET("cron_expr", hu_json_string_new(alloc, cron_expr ? cron_expr : "",
                                             cron_expr ? strlen(cron_expr) : 0));
    CRON_SET("prompt", hu_json_string_new(alloc, prompt ? prompt : "", prompt ? strlen(prompt) : 0));
    if (with_recurring)
        CRON_SET("recurring", hu_json_bool_new(alloc, recurring_val));
    CRON_SET("message", hu_json_string_new(alloc, "Job created", 11));
#undef CRON_SET

    char *msg = NULL;
    size_t msg_len = 0;
    hu_error_t jerr = hu_json_stringify(alloc, root, &msg, &msg_len);
    hu_json_free(alloc, root);
    if (jerr != HU_OK || !msg) {
        *out = hu_tool_result_fail("out of memory", 13);
        return HU_OK;
    }
    *out = hu_tool_result_ok_owned(msg, msg_len);
    return HU_OK;
}

static hu_error_t cron_create_execute(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args,
                                      hu_tool_result_t *out) {
    cron_session_ctx_t *c = (cron_session_ctx_t *)ctx;
    if (!out)
        return HU_ERR_INVALID_ARGUMENT;
    if (!args) {
        *out = hu_tool_result_fail("invalid args", 12);
        return HU_ERR_INVALID_ARGUMENT;
    }

    const char *cron_expr = hu_json_get_string(args, "cron_expr");
    const char *prompt = hu_json_get_string(args, "prompt");
    double recurring_d = hu_json_get_number(args, "recurring", 1.0);

    if (!cron_expr || !cron_expr[0]) {
        *out = hu_tool_result_fail("missing cron_expr", 17);
        return HU_OK;
    }
    if (!prompt || !prompt[0]) {
        *out = hu_tool_result_fail("missing prompt", 14);
        return HU_OK;
    }

#if HU_IS_TEST
    (void)c;
    return cron_create_ok_json(alloc, 1u, cron_expr, prompt, true, recurring_d > 0.5, out);
#else
    (void)recurring_d;
    /* Production: use scheduler */
    if (!c->scheduler) {
        *out = hu_tool_result_fail("scheduler not available", 23);
        return HU_OK;
    }

    uint64_t job_id = 0;
    /* Agent job: prompt is stored as HU_CRON_JOB_AGENT data, not executed via /bin/sh. */
    hu_error_t err =
        hu_cron_add_agent_job(c->scheduler, alloc, cron_expr, prompt, "", NULL, &job_id);
    if (err != HU_OK) {
        *out = hu_tool_result_fail("failed to add job", 17);
        return HU_OK;
    }

    return cron_create_ok_json(alloc, job_id, cron_expr, prompt, false, false, out);
#endif
}

static const char *cron_create_name(void *ctx) {
    (void)ctx;
    return CRON_CREATE_NAME;
}
static const char *cron_create_desc(void *ctx) {
    (void)ctx;
    return CRON_CREATE_DESC;
}
static const char *cron_create_params(void *ctx) {
    (void)ctx;
    return CRON_CREATE_PARAMS;
}
static void cron_create_deinit(void *ctx, hu_allocator_t *alloc) {
    if (!ctx || !alloc)
        return;
    alloc->free(alloc->ctx, ctx, sizeof(cron_session_ctx_t));
}

static const hu_tool_vtable_t cron_create_vtable = {
    .execute = cron_create_execute,
    .name = cron_create_name,
    .description = cron_create_desc,
    .parameters_json = cron_create_params,
    .deinit = cron_create_deinit,
};

hu_error_t hu_cron_create_session_tool_create(hu_allocator_t *alloc,
                                              hu_cron_scheduler_t *scheduler, hu_tool_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    cron_session_ctx_t *c = (cron_session_ctx_t *)alloc->alloc(alloc->ctx, sizeof(*c));
    if (!c)
        return HU_ERR_OUT_OF_MEMORY;
    memset(c, 0, sizeof(*c));
    c->alloc = alloc;
    c->scheduler = scheduler;
    out->ctx = c;
    out->vtable = &cron_create_vtable;
    return HU_OK;
}

/* ──────────────────────────────────────────────────────────────────────────
 * cron_delete tool: Remove a scheduled job
 * ────────────────────────────────────────────────────────────────────────── */

#define CRON_DELETE_NAME "cron_delete"
#define CRON_DELETE_DESC "Remove a scheduled cron job by ID"
#define CRON_DELETE_PARAMS                                                                 \
    "{\"type\":\"object\",\"properties\":{" \
    "\"id\":{\"type\":\"integer\",\"description\":\"Job ID to delete\"}" \
    "},\"required\":[\"id\"]}"

static hu_error_t cron_delete_execute(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args,
                                      hu_tool_result_t *out) {
    cron_session_ctx_t *c = (cron_session_ctx_t *)ctx;
    if (!out)
        return HU_ERR_INVALID_ARGUMENT;
    if (!args) {
        *out = hu_tool_result_fail("invalid args", 12);
        return HU_ERR_INVALID_ARGUMENT;
    }

    double id_d = hu_json_get_number(args, "id", -1.0);
    if (id_d < 0) {
        *out = hu_tool_result_fail("missing id", 10);
        return HU_OK;
    }
    uint64_t job_id = (uint64_t)id_d;

#if HU_IS_TEST
    (void)c;
    /* Test: simulate job deletion */
    char *response = hu_sprintf(alloc, "{\"id\":%llu,\"message\":\"Job deleted\"}", (unsigned long long)job_id);
    if (!response)
        return HU_ERR_OUT_OF_MEMORY;
    *out = hu_tool_result_ok_owned(response, strlen(response));
#else
    /* Production: use scheduler */
    if (!c->scheduler) {
        *out = hu_tool_result_fail("scheduler not available", 23);
        return HU_OK;
    }

    hu_error_t err = hu_cron_remove_job(c->scheduler, job_id);
    if (err != HU_OK) {
        *out = hu_tool_result_fail("failed to delete job", 20);
        return HU_OK;
    }

    char *response = hu_sprintf(alloc, "{\"id\":%llu,\"message\":\"Job deleted\"}", (unsigned long long)job_id);
    if (!response)
        return HU_ERR_OUT_OF_MEMORY;
    *out = hu_tool_result_ok_owned(response, strlen(response));
#endif
    return HU_OK;
}

static const char *cron_delete_name(void *ctx) {
    (void)ctx;
    return CRON_DELETE_NAME;
}
static const char *cron_delete_desc(void *ctx) {
    (void)ctx;
    return CRON_DELETE_DESC;
}
static const char *cron_delete_params(void *ctx) {
    (void)ctx;
    return CRON_DELETE_PARAMS;
}
static void cron_delete_deinit(void *ctx, hu_allocator_t *alloc) {
    if (!ctx || !alloc)
        return;
    alloc->free(alloc->ctx, ctx, sizeof(cron_session_ctx_t));
}

static const hu_tool_vtable_t cron_delete_vtable = {
    .execute = cron_delete_execute,
    .name = cron_delete_name,
    .description = cron_delete_desc,
    .parameters_json = cron_delete_params,
    .deinit = cron_delete_deinit,
};

hu_error_t hu_cron_delete_session_tool_create(hu_allocator_t *alloc,
                                              hu_cron_scheduler_t *scheduler, hu_tool_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    cron_session_ctx_t *c = (cron_session_ctx_t *)alloc->alloc(alloc->ctx, sizeof(*c));
    if (!c)
        return HU_ERR_OUT_OF_MEMORY;
    memset(c, 0, sizeof(*c));
    c->alloc = alloc;
    c->scheduler = scheduler;
    out->ctx = c;
    out->vtable = &cron_delete_vtable;
    return HU_OK;
}

/* ──────────────────────────────────────────────────────────────────────────
 * cron_list tool: List all active jobs
 * ────────────────────────────────────────────────────────────────────────── */

#define CRON_LIST_NAME "cron_list"
#define CRON_LIST_DESC "List all active scheduled cron jobs"
#define CRON_LIST_PARAMS "{\"type\":\"object\",\"properties\":{}}"

static hu_error_t cron_list_execute(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args,
                                    hu_tool_result_t *out) {
    cron_session_ctx_t *c = (cron_session_ctx_t *)ctx;
    (void)args;
    (void)alloc;
    if (!out)
        return HU_ERR_INVALID_ARGUMENT;

#if HU_IS_TEST
    (void)c;
    /* Test: return mock job list */
    const char *response = "{\"jobs\":[{\"id\":1,\"cron_expr\":\"*/5 * * * *\",\"command\":\"test\",\"next_run\":1234567890}]}";
    *out = hu_tool_result_ok(response, strlen(response));
#else
    /* Production: use scheduler */
    if (!c->scheduler) {
        *out = hu_tool_result_ok("{\"jobs\":[]}", 12);
        return HU_OK;
    }

    size_t job_count = 0;
    const hu_cron_job_t *jobs = hu_cron_list_jobs(c->scheduler, &job_count);

    /* Build JSON response */
    hu_json_buf_t buf;
    if (hu_json_buf_init(&buf, alloc) != HU_OK)
        return HU_ERR_OUT_OF_MEMORY;

    hu_error_t err = hu_json_buf_append_raw(&buf, "{\"jobs\":[", 10);
    if (err == HU_OK) {
        for (size_t i = 0; i < job_count && i < 100; i++) {
            if (i > 0)
                err = hu_json_buf_append_raw(&buf, ",", 1);
            if (err != HU_OK)
                break;
            err = hu_json_buf_append_raw(&buf, "{", 1);
            if (err == HU_OK)
                err = hu_json_append_key_int(&buf, "id", 2, (long long)jobs[i].id);
            if (err == HU_OK)
                err = hu_json_buf_append_raw(&buf, ",", 1);
            if (err == HU_OK) {
                const char *ex = jobs[i].expression ? jobs[i].expression : "";
                err = hu_json_append_key_value(&buf, "expression", 10, ex, strlen(ex));
            }
            if (err == HU_OK)
                err = hu_json_buf_append_raw(&buf, ",", 1);
            if (err == HU_OK) {
                const char *cmd = jobs[i].command ? jobs[i].command : "";
                err = hu_json_append_key_value(&buf, "command", 7, cmd, strlen(cmd));
            }
            if (err == HU_OK)
                err = hu_json_buf_append_raw(&buf, ",", 1);
            if (err == HU_OK)
                err = hu_json_append_key_int(&buf, "next_run", 8, (long long)jobs[i].next_run_secs);
            if (err == HU_OK)
                err = hu_json_buf_append_raw(&buf, "}", 1);
        }
    }
    if (err == HU_OK)
        err = hu_json_buf_append_raw(&buf, "]}", 2);

    if (err != HU_OK) {
        hu_json_buf_free(&buf);
        *out = hu_tool_result_fail("failed to build response", 24);
        return HU_OK;
    }

    char *response = buf.ptr;
    size_t response_len = buf.len;
    *out = hu_tool_result_ok_owned(response, response_len);
#endif
    return HU_OK;
}

static const char *cron_list_name(void *ctx) {
    (void)ctx;
    return CRON_LIST_NAME;
}
static const char *cron_list_desc(void *ctx) {
    (void)ctx;
    return CRON_LIST_DESC;
}
static const char *cron_list_params(void *ctx) {
    (void)ctx;
    return CRON_LIST_PARAMS;
}
static void cron_list_deinit(void *ctx, hu_allocator_t *alloc) {
    if (!ctx || !alloc)
        return;
    alloc->free(alloc->ctx, ctx, sizeof(cron_session_ctx_t));
}

static const hu_tool_vtable_t cron_list_vtable = {
    .execute = cron_list_execute,
    .name = cron_list_name,
    .description = cron_list_desc,
    .parameters_json = cron_list_params,
    .deinit = cron_list_deinit,
};

hu_error_t hu_cron_list_session_tool_create(hu_allocator_t *alloc, hu_cron_scheduler_t *scheduler,
                                            hu_tool_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    cron_session_ctx_t *c = (cron_session_ctx_t *)alloc->alloc(alloc->ctx, sizeof(*c));
    if (!c)
        return HU_ERR_OUT_OF_MEMORY;
    memset(c, 0, sizeof(*c));
    c->alloc = alloc;
    c->scheduler = scheduler;
    out->ctx = c;
    out->vtable = &cron_list_vtable;
    return HU_OK;
}
