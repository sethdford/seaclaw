#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include "human/tool.h"
#include <stdio.h>
#include <string.h>

#define HU_ASK_USER_NAME "ask_user"
#define HU_ASK_USER_DESC "Ask the user a question and wait for their response"
#define HU_ASK_USER_PARAMS                                                                         \
    "{\"type\":\"object\",\"properties\":{\"question\":{\"type\":\"string\",\"description\":"     \
    "\"The question to ask\"},\"options\":{\"type\":\"array\",\"items\":{\"type\":\"string\"},"   \
    "\"description\":\"Optional multiple-choice options\"}},\"required\":[\"question\"]}"
#define HU_ASK_USER_QUESTION_MAX 8192

typedef struct hu_ask_user_ctx {
    hu_allocator_t *alloc;
    bool (*approval_cb)(void *ctx, const char *question, size_t question_len, char **out_response);
    void *approval_ctx;
} hu_ask_user_ctx_t;

static hu_error_t ask_user_execute(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args,
                                   hu_tool_result_t *out) {
    hu_ask_user_ctx_t *c = (hu_ask_user_ctx_t *)ctx;
    if (!c || !args || !out) {
        *out = hu_tool_result_fail("invalid args", 12);
        return HU_ERR_INVALID_ARGUMENT;
    }

    const char *question = hu_json_get_string(args, "question");
    if (!question || strlen(question) == 0) {
        *out = hu_tool_result_fail("missing question parameter", 25);
        return HU_OK;
    }

    if (strlen(question) > HU_ASK_USER_QUESTION_MAX) {
        *out = hu_tool_result_fail("question too long", 17);
        return HU_OK;
    }

#if HU_IS_TEST
    /* Test mode: return stub response */
    char *msg = hu_strndup(alloc, "(ask_user stub: awaiting user response)", 39);
    if (!msg) {
        *out = hu_tool_result_fail("out of memory", 13);
        return HU_ERR_OUT_OF_MEMORY;
    }
    *out = hu_tool_result_ok_owned(msg, 39);
    return HU_OK;
#else
    size_t question_len = strlen(question);
    /* Non-test: use approval callback if available */
    if (!c->approval_cb) {
        /* No callback in non-interactive mode */
        char *msg = hu_strndup(alloc, "{\"waiting_for_response\": true, \"question\": \"", 44);
        if (!msg) {
            *out = hu_tool_result_fail("out of memory", 13);
            return HU_ERR_OUT_OF_MEMORY;
        }

        size_t buf_size = 44 + question_len + 3;
        char *buf = (char *)alloc->alloc(alloc->ctx, buf_size + 1);
        if (!buf) {
            alloc->free(alloc->ctx, msg, 45);
            *out = hu_tool_result_fail("out of memory", 13);
            return HU_ERR_OUT_OF_MEMORY;
        }

        snprintf(buf, buf_size + 1, "{\"waiting_for_response\": true, \"question\": \"%.*s\"}",
                 (int)question_len, question);

        alloc->free(alloc->ctx, msg, 45);
        *out = hu_tool_result_ok_owned(buf, strlen(buf));
        out->needs_approval = true;
        return HU_OK;
    }

    /* Call approval callback to get user response */
    char *response = NULL;
    if (!c->approval_cb(c->approval_ctx, question, question_len, &response)) {
        *out = hu_tool_result_fail("user denied or no response provided", 35);
        return HU_OK;
    }

    if (!response) {
        *out = hu_tool_result_fail("approval callback returned NULL", 31);
        return HU_OK;
    }

    *out = hu_tool_result_ok_owned(response, strlen(response));
    return HU_OK;
#endif
}

static const char *ask_user_name(void *ctx) {
    (void)ctx;
    return HU_ASK_USER_NAME;
}

static const char *ask_user_description(void *ctx) {
    (void)ctx;
    return HU_ASK_USER_DESC;
}

static const char *ask_user_parameters_json(void *ctx) {
    (void)ctx;
    return HU_ASK_USER_PARAMS;
}

static void ask_user_deinit(void *ctx, hu_allocator_t *alloc) {
    hu_ask_user_ctx_t *c = (hu_ask_user_ctx_t *)ctx;
    if (c) {
        alloc->free(alloc->ctx, c, sizeof(hu_ask_user_ctx_t));
    }
}

HU_TOOL_IMPL(hu_tool_ask_user, ask_user_execute, ask_user_name, ask_user_description,
             ask_user_parameters_json, ask_user_deinit);

hu_tool_t hu_tool_ask_user_create(hu_allocator_t *alloc,
                                  bool (*approval_cb)(void *ctx, const char *question,
                                                      size_t question_len, char **out_response)) {
    hu_ask_user_ctx_t *ctx = (hu_ask_user_ctx_t *)alloc->alloc(alloc->ctx, sizeof(hu_ask_user_ctx_t));
    if (!ctx) {
        return (hu_tool_t){.ctx = NULL, .vtable = NULL};
    }

    ctx->alloc = alloc;
    ctx->approval_cb = approval_cb;
    ctx->approval_ctx = NULL;

    return (hu_tool_t){.ctx = ctx, .vtable = &hu_tool_ask_user_vtable};
}
