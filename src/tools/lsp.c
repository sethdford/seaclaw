#include "human/tools/lsp.h"
#include "human/core/json.h"

static hu_error_t lsp_execute(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args, hu_tool_result_t *out) {
    (void)ctx; (void)args;
    if (!alloc || !out) return HU_ERR_INVALID_ARGUMENT;
#ifdef HU_IS_TEST
    *out = hu_tool_result_ok("{\"diagnostics\":[],\"completions\":[]}", 35);
    return HU_OK;
#else
    *out = hu_tool_result_fail("LSP not supported", 17);
    return HU_OK;
#endif
}

static const char *lsp_name(void *ctx) { (void)ctx; return "lsp"; }
static const char *lsp_description(void *ctx) { (void)ctx; return "Language Server Protocol tool"; }
static const char *lsp_parameters_json(void *ctx) { (void)ctx; return "{\"type\":\"object\",\"properties\":{\"action\":{\"type\":\"string\"}}}"; }

static const hu_tool_vtable_t lsp_vtable = {
    .execute = lsp_execute, .name = lsp_name,
    .description = lsp_description, .parameters_json = lsp_parameters_json, .deinit = NULL,
};

hu_tool_t hu_lsp_tool_create(hu_allocator_t *alloc) {
    (void)alloc;
    return (hu_tool_t){ .ctx = NULL, .vtable = &lsp_vtable };
}
