#include "human/providers/embedded.h"
#include <string.h>

typedef struct { hu_embedded_config_t config; } embedded_ctx_t;

static hu_error_t embedded_chat_with_system(void *ctx, hu_allocator_t *alloc,
    const char *system_prompt, size_t system_prompt_len,
    const char *message, size_t message_len,
    const char *model, size_t model_len,
    double temperature, char **out, size_t *out_len) {
    (void)ctx; (void)system_prompt; (void)system_prompt_len;
    (void)message; (void)message_len; (void)model; (void)model_len;
    (void)temperature;
    if (!alloc || !out || !out_len) return HU_ERR_INVALID_ARGUMENT;
#ifdef HU_IS_TEST
    const char *resp = "Mock embedded response";
    size_t rlen = strlen(resp);
    *out = alloc->alloc(alloc->ctx, rlen + 1);
    if (!*out) return HU_ERR_OUT_OF_MEMORY;
    memcpy(*out, resp, rlen + 1);
    *out_len = rlen;
    return HU_OK;
#else
    return HU_ERR_NOT_SUPPORTED;
#endif
}

static const char *embedded_get_name(void *ctx) { (void)ctx; return "embedded"; }
static bool embedded_supports_native_tools(void *ctx) { (void)ctx; return false; }
static void embedded_deinit(void *ctx, hu_allocator_t *alloc) {
    if (ctx) alloc->free(alloc->ctx, ctx, sizeof(embedded_ctx_t));
}

static const hu_provider_vtable_t embedded_vtable = {
    .chat_with_system = embedded_chat_with_system,
    .chat = NULL,
    .get_name = embedded_get_name,
    .supports_native_tools = embedded_supports_native_tools,
    .deinit = embedded_deinit,
};

hu_error_t hu_embedded_provider_create(hu_allocator_t *alloc, const hu_embedded_config_t *config, hu_provider_t *out) {
    if (!alloc || !config || !out) return HU_ERR_INVALID_ARGUMENT;
    embedded_ctx_t *ctx = alloc->alloc(alloc->ctx, sizeof(embedded_ctx_t));
    if (!ctx) return HU_ERR_OUT_OF_MEMORY;
    memset(ctx, 0, sizeof(*ctx));
    ctx->config = *config;
    out->ctx = ctx;
    out->vtable = &embedded_vtable;
    return HU_OK;
}
