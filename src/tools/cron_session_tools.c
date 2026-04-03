#include "human/tools/cron_session_tools.h"
#include "human/core/json.h"
#include "human/core/string.h"
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
    (void)recurring_d;  /* Used in HU_IS_TEST */

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
    /* Test: simulate job creation with ID */
    char *response = hu_sprintf(alloc,
                                "{\"id\":1,\"cron_expr\":\"%s\",\"prompt\":\"%s\",\"recurring\":%s,\"message\":\"Job created\"}",
                                cron_expr, prompt, recurring_d > 0.5 ? "true" : "false");
    if (!response)
        return HU_ERR_OUT_OF_MEMORY;
    *out = hu_tool_result_ok_owned(response, strlen(response));
#else
    /* Production: use scheduler */
    if (!c->scheduler) {
        *out = hu_tool_result_fail("scheduler not available", 23);
        return HU_OK;
    }

    uint64_t job_id = 0;
    hu_error_t err = hu_cron_add_job(c->scheduler, alloc, cron_expr, prompt, "", &job_id);
    if (err != HU_OK) {
        *out = hu_tool_result_fail("failed to add job", 17);
        return HU_OK;
    }

    char *response = hu_sprintf(alloc, "{\"id\":%llu,\"cron_expr\":\"%s\",\"message\":\"Job created\"}",
                                (unsigned long long)job_id, cron_expr);
    if (!response)
        return HU_ERR_OUT_OF_MEMORY;
    *out = hu_tool_result_ok_owned(response, strlen(response));
#endif
    return HU_OK;
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
    if (ctx)
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
    if (ctx)
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
            if (err == HU_OK) {
                char job_json[512];
                int len = snprintf(job_json, sizeof(job_json),
                                   "{\"id\":%llu,\"expression\":\"%s\",\"command\":\"%s\",\"next_run\":%lld}",
                                   (unsigned long long)jobs[i].id, jobs[i].expression ? jobs[i].expression : "",
                                   jobs[i].command ? jobs[i].command : "", (long long)jobs[i].next_run_secs);
                if (len > 0)
                    err = hu_json_buf_append_raw(&buf, job_json, (size_t)len);
            }
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
    if (ctx)
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
