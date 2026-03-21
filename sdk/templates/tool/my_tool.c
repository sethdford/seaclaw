/*
 * Template: Custom Tool
 *
 * Implements hu_tool_t vtable. Required methods:
 *   - execute, name, description, parameters_json
 *
 * Optional (may be NULL; see include/human/tool.h):
 *   - deinit, execute_streaming
 */
#include "my_tool.h"
#include "human/tool.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include <string.h>

#define HU_MY_TOOL_NAME "my_tool"
#define HU_MY_TOOL_DESC "Does something useful. Describe what the tool does for the LLM."
#define HU_MY_TOOL_PARAMS "{\"type\":\"object\",\"properties\":{\"input\":{\"type\":\"string\"}},\"required\":[\"input\"]}"

typedef struct hu_my_tool_ctx {
    char reserved; /* replace with real fields when you add per-instance state */
} hu_my_tool_ctx_t;

static hu_error_t my_tool_execute(void *ctx, hu_allocator_t *alloc,
    const hu_json_value_t *args,
    hu_tool_result_t *out)
{
    hu_my_tool_ctx_t *c = (hu_my_tool_ctx_t *)ctx;
    if (!c || !args || !out) {
        *out = hu_tool_result_fail("invalid args", 12);
        return HU_ERR_INVALID_ARGUMENT;
    }

    const char *input = hu_json_get_string(args, "input");
    if (!input || strlen(input) == 0) {
        *out = hu_tool_result_fail("Missing required 'input' parameter", 31);
        return HU_OK;
    }

#if HU_IS_TEST
    /* In tests: no side effects (network, spawn, etc.) */
    char *msg = hu_strndup(alloc, "(my_tool stub)", 14);
    if (!msg) {
        *out = hu_tool_result_fail("out of memory", 12);
        return HU_ERR_OUT_OF_MEMORY;
    }
    *out = hu_tool_result_ok_owned(msg, 14);
    return HU_OK;
#else
    /* Implementation checklist (AGENTS.md §7.3 — Adding a Tool):
     * [ ] Validate and sanitize all inputs from args; return hu_tool_result_fail* on errors.
     * [ ] Use hu_tool_result_ok / hu_tool_result_ok_owned for success; fail* for tool errors.
     * [ ] HU_IS_TEST-guard any network, process spawn, or filesystem side effects.
     * [ ] Register in src/tools/factory.c; add execute/schema tests.
     * [ ] Caller frees owned fields via hu_tool_result_free when needed.
     */
    (void)alloc;
    (void)input;
    *out = hu_tool_result_fail("Not implemented", 14);
    return HU_OK;
#endif
}

static const char *my_tool_name(void *ctx) {
    (void)ctx;
    return HU_MY_TOOL_NAME;
}

static const char *my_tool_description(void *ctx) {
    (void)ctx;
    return HU_MY_TOOL_DESC;
}

static const char *my_tool_parameters_json(void *ctx) {
    (void)ctx;
    return HU_MY_TOOL_PARAMS;
}

static void my_tool_deinit(void *ctx, hu_allocator_t *alloc) {
    if (!ctx || !alloc) return;
    alloc->free(alloc->ctx, ctx, sizeof(hu_my_tool_ctx_t));
}

static const hu_tool_vtable_t my_tool_vtable = {
    .execute = my_tool_execute,
    .name = my_tool_name,
    .description = my_tool_description,
    .parameters_json = my_tool_parameters_json,
    .deinit = my_tool_deinit,
    .execute_streaming = NULL,
};

hu_error_t hu_my_tool_create(hu_allocator_t *alloc, hu_tool_t *out) {
    if (!alloc || !out) return HU_ERR_INVALID_ARGUMENT;

    hu_my_tool_ctx_t *ctx =
        (hu_my_tool_ctx_t *)alloc->alloc(alloc->ctx, sizeof(*ctx));
    if (!ctx) return HU_ERR_OUT_OF_MEMORY;
    memset(ctx, 0, sizeof(*ctx));

    out->ctx = ctx;
    out->vtable = &my_tool_vtable;
    return HU_OK;
}
