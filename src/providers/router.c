#include "human/providers/router.h"
#include "human/core/string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct hu_router_resolved {
    size_t provider_index;
    const char *model;
    size_t model_len;
} hu_router_resolved_t;

typedef struct hu_router_route_internal {
    char *hint;
    size_t hint_len;
    size_t provider_index;
    char *model;
    size_t model_len;
} hu_router_route_internal_t;

typedef struct hu_router_ctx {
    hu_allocator_t *alloc;
    hu_provider_t *providers;
    size_t provider_count;
    hu_router_route_internal_t *routes;
    size_t route_count;
    char *default_model;
    size_t default_model_len;
} hu_router_ctx_t;

static void resolve_model(hu_router_ctx_t *r, const char *model, size_t model_len,
                          hu_router_resolved_t *out) {
    if (model_len >= HU_HINT_PREFIX_LEN && memcmp(model, HU_HINT_PREFIX, HU_HINT_PREFIX_LEN) == 0) {
        const char *hint = model + HU_HINT_PREFIX_LEN;
        size_t hint_len = model_len - HU_HINT_PREFIX_LEN;
        for (size_t i = 0; i < r->route_count; i++) {
            if (r->routes[i].hint_len == hint_len &&
                memcmp(r->routes[i].hint, hint, hint_len) == 0) {
                out->provider_index = r->routes[i].provider_index;
                out->model = r->routes[i].model;
                out->model_len = r->routes[i].model_len;
                return;
            }
        }
    }
    out->provider_index = 0;
    out->model = model;
    out->model_len = model_len;
}

/* Estimate task complexity from message content for auto model selection.
 * Returns 0 (simple), 1 (medium), 2 (complex). */
static int estimate_complexity(const hu_chat_request_t *request) {
    if (!request || request->messages_count == 0)
        return 1;
    const hu_chat_message_t *last = &request->messages[request->messages_count - 1];
    if (!last->content || last->content_len == 0)
        return 1;
    size_t len = last->content_len;
    size_t word_count = 1;
    for (size_t i = 0; i < len; i++)
        if (last->content[i] == ' ') word_count++;

    bool has_tools = request->tools_count > 0;
    bool has_code = false;
    for (size_t i = 0; i + 3 < len; i++) {
        if (last->content[i] == '`' && last->content[i+1] == '`' && last->content[i+2] == '`') {
            has_code = true;
            break;
        }
    }

    if (word_count <= 8 && !has_tools && !has_code)
        return 0;
    if (has_tools || has_code || word_count > 100 || request->messages_count > 10)
        return 2;
    return 1;
}

/* Auto-select: route to fast model for simple tasks, or when budget is tight. */
static void auto_select_model(hu_router_ctx_t *r, const hu_chat_request_t *request,
                               hu_router_resolved_t *resolved) {
    int complexity = estimate_complexity(request);
    bool use_fast = (complexity == 0);

    if (!use_fast && request->budget_remaining_usd > 0.0 &&
        request->budget_remaining_usd < 0.10 && complexity <= 1) {
        use_fast = true;
    }

    if (use_fast) {
        for (size_t i = 0; i < r->route_count; i++) {
            if (r->routes[i].hint_len == 4 && memcmp(r->routes[i].hint, "fast", 4) == 0) {
                resolved->provider_index = r->routes[i].provider_index;
                resolved->model = r->routes[i].model;
                resolved->model_len = r->routes[i].model_len;
                return;
            }
        }
    }
}

static hu_error_t router_chat_with_system(void *ctx, hu_allocator_t *alloc,
                                          const char *system_prompt, size_t system_prompt_len,
                                          const char *message, size_t message_len,
                                          const char *model, size_t model_len, double temperature,
                                          char **out, size_t *out_len) {
    hu_router_ctx_t *r = (hu_router_ctx_t *)ctx;
    hu_router_resolved_t res;
    resolve_model(r, model, model_len, &res);
    if (res.provider_index >= r->provider_count)
        return HU_ERR_PROVIDER_RESPONSE;
    const hu_provider_vtable_t *vt = r->providers[res.provider_index].vtable;
    if (!vt || !vt->chat_with_system)
        return HU_ERR_INVALID_ARGUMENT;
    return vt->chat_with_system(r->providers[res.provider_index].ctx, alloc, system_prompt,
                                system_prompt_len, message, message_len, res.model, res.model_len,
                                temperature, out, out_len);
}

static hu_error_t router_chat(void *ctx, hu_allocator_t *alloc, const hu_chat_request_t *request,
                              const char *model, size_t model_len, double temperature,
                              hu_chat_response_t *out) {
    hu_router_ctx_t *r = (hu_router_ctx_t *)ctx;
    hu_router_resolved_t res;
    resolve_model(r, model, model_len, &res);
    auto_select_model(r, request, &res);
    if (res.provider_index >= r->provider_count)
        return HU_ERR_PROVIDER_RESPONSE;
    const hu_provider_vtable_t *vt = r->providers[res.provider_index].vtable;
    if (!vt || !vt->chat)
        return HU_ERR_INVALID_ARGUMENT;
    return vt->chat(r->providers[res.provider_index].ctx, alloc, request, res.model, res.model_len,
                    temperature, out);
}

static bool router_supports_native_tools(void *ctx) {
    hu_router_ctx_t *r = (hu_router_ctx_t *)ctx;
    if (r->provider_count == 0)
        return false;
    const hu_provider_vtable_t *vt = r->providers[0].vtable;
    return vt && vt->supports_native_tools && vt->supports_native_tools(r->providers[0].ctx);
}

static bool router_supports_vision(void *ctx) {
    hu_router_ctx_t *r = (hu_router_ctx_t *)ctx;
    if (r->provider_count == 0)
        return false;
    const hu_provider_vtable_t *vt = r->providers[0].vtable;
    if (vt && vt->supports_vision)
        return vt->supports_vision(r->providers[0].ctx);
    return false;
}

static bool router_supports_vision_for_model(void *ctx, const char *model, size_t model_len) {
    hu_router_ctx_t *r = (hu_router_ctx_t *)ctx;
    hu_router_resolved_t res;
    resolve_model(r, model, model_len, &res);
    if (res.provider_index >= r->provider_count)
        return false;
    const hu_provider_vtable_t *vt = r->providers[res.provider_index].vtable;
    return vt && vt->supports_vision_for_model &&
           vt->supports_vision_for_model(r->providers[res.provider_index].ctx, model, model_len);
}

static bool router_supports_streaming(void *ctx) {
    hu_router_ctx_t *r = (hu_router_ctx_t *)ctx;
    for (size_t i = 0; i < r->provider_count; i++) {
        const hu_provider_vtable_t *vt = r->providers[i].vtable;
        if (vt && vt->supports_streaming && vt->supports_streaming(r->providers[i].ctx))
            return true;
    }
    return false;
}

static hu_error_t router_stream_chat(void *ctx, hu_allocator_t *alloc,
                                     const hu_chat_request_t *request, const char *model,
                                     size_t model_len, double temperature,
                                     hu_stream_callback_t callback, void *callback_ctx,
                                     hu_stream_chat_result_t *out) {
    hu_router_ctx_t *r = (hu_router_ctx_t *)ctx;
    hu_router_resolved_t res;
    resolve_model(r, model, model_len, &res);
    auto_select_model(r, request, &res);
    if (res.provider_index >= r->provider_count)
        return HU_ERR_PROVIDER_RESPONSE;
    hu_provider_t *p = &r->providers[res.provider_index];
    if (!p->vtable || !p->vtable->stream_chat)
        return HU_ERR_NOT_SUPPORTED;
    return p->vtable->stream_chat(p->ctx, alloc, request, res.model, res.model_len,
                                   temperature, callback, callback_ctx, out);
}

static const char *router_get_name(void *ctx) {
    (void)ctx;
    return "router";
}

static void router_deinit(void *ctx, hu_allocator_t *alloc) {
    hu_router_ctx_t *r = (hu_router_ctx_t *)ctx;
    for (size_t i = 0; i < r->route_count; i++) {
        if (r->routes[i].hint)
            alloc->free(alloc->ctx, r->routes[i].hint, r->routes[i].hint_len + 1);
        if (r->routes[i].model)
            alloc->free(alloc->ctx, r->routes[i].model, r->routes[i].model_len + 1);
    }
    if (r->routes)
        alloc->free(alloc->ctx, r->routes, sizeof(hu_router_route_internal_t) * r->route_count);
    if (r->default_model)
        alloc->free(alloc->ctx, r->default_model, r->default_model_len + 1);
    if (r->providers)
        alloc->free(alloc->ctx, r->providers, sizeof(hu_provider_t) * r->provider_count);
    alloc->free(alloc->ctx, r, sizeof(*r));
}

static const hu_provider_vtable_t router_vtable = {
    .chat_with_system = router_chat_with_system,
    .chat = router_chat,
    .supports_native_tools = router_supports_native_tools,
    .get_name = router_get_name,
    .deinit = router_deinit,
    .warmup = NULL,
    .chat_with_tools = NULL,
    .supports_streaming = router_supports_streaming,
    .supports_vision = router_supports_vision,
    .supports_vision_for_model = router_supports_vision_for_model,
    .stream_chat = router_stream_chat,
};

hu_error_t hu_router_create(hu_allocator_t *alloc, const char *const *provider_names,
                            const size_t *provider_name_lens, size_t provider_count,
                            hu_provider_t *providers, const hu_router_route_entry_t *routes,
                            size_t route_count, const char *default_model, size_t default_model_len,
                            hu_provider_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    if (provider_count == 0 || !providers)
        return HU_ERR_INVALID_ARGUMENT;

    hu_router_ctx_t *r = (hu_router_ctx_t *)alloc->alloc(alloc->ctx, sizeof(*r));
    if (!r)
        return HU_ERR_OUT_OF_MEMORY;
    memset(r, 0, sizeof(*r));
    r->alloc = alloc;

    hu_provider_t *prov_copy =
        (hu_provider_t *)alloc->alloc(alloc->ctx, sizeof(hu_provider_t) * provider_count);
    if (!prov_copy) {
        alloc->free(alloc->ctx, r, sizeof(*r));
        return HU_ERR_OUT_OF_MEMORY;
    }
    memcpy(prov_copy, providers, sizeof(hu_provider_t) * provider_count);
    r->providers = prov_copy;
    r->provider_count = provider_count;

    r->default_model = hu_strndup(alloc, default_model ? default_model : "default",
                                  default_model_len ? default_model_len : 7);
    r->default_model_len = default_model_len ? default_model_len : 7;

    if (route_count > 0 && routes && provider_names && provider_name_lens) {
        hu_router_route_internal_t *ri = (hu_router_route_internal_t *)alloc->alloc(
            alloc->ctx, sizeof(hu_router_route_internal_t) * route_count);
        if (!ri) {
            alloc->free(alloc->ctx, prov_copy, sizeof(hu_provider_t) * provider_count);
            if (r->default_model)
                alloc->free(alloc->ctx, r->default_model, r->default_model_len + 1);
            alloc->free(alloc->ctx, r, sizeof(*r));
            return HU_ERR_OUT_OF_MEMORY;
        }
        memset(ri, 0, sizeof(hu_router_route_internal_t) * route_count);
        r->routes = ri;
        r->route_count = route_count;
        for (size_t i = 0; i < route_count; i++) {
            size_t hint_len = routes[i].hint_len;
            ri[i].hint = hu_strndup(alloc, routes[i].hint, hint_len);
            if (!ri[i].hint) {
                for (size_t k = 0; k < i; k++) {
                    if (ri[k].hint)
                        alloc->free(alloc->ctx, (void *)ri[k].hint, ri[k].hint_len + 1);
                    if (ri[k].model)
                        alloc->free(alloc->ctx, (void *)ri[k].model, ri[k].model_len + 1);
                }
                alloc->free(alloc->ctx, ri, route_count * sizeof(*ri));
                if (r->default_model)
                    alloc->free(alloc->ctx, r->default_model, r->default_model_len + 1);
                alloc->free(alloc->ctx, prov_copy, sizeof(hu_provider_t) * provider_count);
                alloc->free(alloc->ctx, r, sizeof(*r));
                return HU_ERR_OUT_OF_MEMORY;
            }
            ri[i].hint_len = hint_len;
            for (size_t j = 0; j < provider_count; j++) {
                if (provider_name_lens[j] == routes[i].route.provider_name_len &&
                    memcmp(provider_names[j], routes[i].route.provider_name,
                           provider_name_lens[j]) == 0) {
                    ri[i].provider_index = j;
                    break;
                }
            }
            ri[i].model = hu_strndup(alloc, routes[i].route.model, routes[i].route.model_len);
            if (!ri[i].model) {
                alloc->free(alloc->ctx, (void *)ri[i].hint, hint_len + 1);
                for (size_t k = 0; k < i; k++) {
                    if (ri[k].hint)
                        alloc->free(alloc->ctx, (void *)ri[k].hint, ri[k].hint_len + 1);
                    if (ri[k].model)
                        alloc->free(alloc->ctx, (void *)ri[k].model, ri[k].model_len + 1);
                }
                alloc->free(alloc->ctx, ri, route_count * sizeof(*ri));
                if (r->default_model)
                    alloc->free(alloc->ctx, r->default_model, r->default_model_len + 1);
                alloc->free(alloc->ctx, prov_copy, sizeof(hu_provider_t) * provider_count);
                alloc->free(alloc->ctx, r, sizeof(*r));
                return HU_ERR_OUT_OF_MEMORY;
            }
            ri[i].model_len = routes[i].route.model_len;
        }
    }

    out->ctx = r;
    out->vtable = &router_vtable;
    return HU_OK;
}

/* Multi-model router: delegates to fast/standard/powerful based on complexity. */
typedef struct hu_multi_model_ctx {
    hu_allocator_t *alloc;
    hu_multi_model_router_config_t config;
} hu_multi_model_ctx_t;

static const char *multi_model_get_name(void *ctx) {
    (void)ctx;
    return "router";
}

static hu_provider_t *multi_model_select(hu_multi_model_ctx_t *m, int approx_tokens) {
    if (approx_tokens < m->config.complexity_threshold_low && m->config.fast.ctx &&
        m->config.fast.vtable)
        return &m->config.fast;
    if (approx_tokens > m->config.complexity_threshold_high && m->config.powerful.ctx &&
        m->config.powerful.vtable)
        return &m->config.powerful;
    return m->config.standard.ctx ? &m->config.standard : NULL;
}

static hu_error_t multi_model_chat(void *ctx, hu_allocator_t *alloc,
                                   const hu_chat_request_t *request, const char *model,
                                   size_t model_len, double temperature, hu_chat_response_t *out) {
    hu_multi_model_ctx_t *m = (hu_multi_model_ctx_t *)ctx;
    int approx = 0;
    for (size_t i = 0; i < request->messages_count; i++)
        approx += (int)(request->messages[i].content_len / 4);
    hu_provider_t *p = multi_model_select(m, approx);
    if (!p || !p->vtable || !p->vtable->chat)
        return HU_ERR_PROVIDER_RESPONSE;
    return p->vtable->chat(p->ctx, alloc, request, model, model_len, temperature, out);
}

static void multi_model_deinit(void *ctx, hu_allocator_t *alloc) {
    hu_multi_model_ctx_t *m = (hu_multi_model_ctx_t *)ctx;
    if (m->config.fast.vtable && m->config.fast.vtable->deinit)
        m->config.fast.vtable->deinit(m->config.fast.ctx, alloc);
    if (m->config.standard.vtable && m->config.standard.vtable->deinit)
        m->config.standard.vtable->deinit(m->config.standard.ctx, alloc);
    if (m->config.powerful.vtable && m->config.powerful.vtable->deinit)
        m->config.powerful.vtable->deinit(m->config.powerful.ctx, alloc);
    alloc->free(alloc->ctx, m, sizeof(*m));
}

static const hu_provider_vtable_t multi_model_vtable = {
    .chat = multi_model_chat,
    .get_name = multi_model_get_name,
    .deinit = multi_model_deinit,
};

hu_error_t hu_multi_model_router_create(hu_allocator_t *alloc,
                                        const hu_multi_model_router_config_t *config,
                                        hu_provider_t *out) {
    if (!alloc || !config || !out)
        return HU_ERR_INVALID_ARGUMENT;
    if (!config->standard.ctx || !config->standard.vtable)
        return HU_ERR_INVALID_ARGUMENT;

    hu_multi_model_ctx_t *m = (hu_multi_model_ctx_t *)alloc->alloc(alloc->ctx, sizeof(*m));
    if (!m)
        return HU_ERR_OUT_OF_MEMORY;
    memset(m, 0, sizeof(*m));
    m->alloc = alloc;
    m->config = *config;

    out->ctx = m;
    out->vtable = &multi_model_vtable;
    return HU_OK;
}
