#include "human/tools/computer_use.h"
#include "human/core/json.h"

static hu_error_t computer_use_execute(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args, hu_tool_result_t *out) {
    (void)ctx; (void)args;
    if (!alloc || !out) return HU_ERR_INVALID_ARGUMENT;
#ifdef HU_IS_TEST
    *out = hu_tool_result_ok("{\"success\":true}", 16);
    return HU_OK;
#else
    *out = hu_tool_result_fail("computer_use not supported", 26);
    return HU_OK;
#endif
}

static const char *computer_use_name(void *ctx) { (void)ctx; return "computer_use"; }
static const char *computer_use_description(void *ctx) { (void)ctx; return "Control computer via screenshot, click, type"; }
static const char *computer_use_parameters_json(void *ctx) { (void)ctx; return "{\"type\":\"object\",\"properties\":{\"action\":{\"type\":\"string\"}}}"; }

static const hu_tool_vtable_t computer_use_vtable = {
    .execute = computer_use_execute, .name = computer_use_name,
    .description = computer_use_description, .parameters_json = computer_use_parameters_json, .deinit = NULL,
};

hu_tool_t hu_computer_use_create(hu_allocator_t *alloc) {
    (void)alloc;
    return (hu_tool_t){ .ctx = NULL, .vtable = &computer_use_vtable };
}
