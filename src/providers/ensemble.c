#include "human/providers/ensemble.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    hu_ensemble_spec_t config;
    size_t round_robin_idx;
} ensemble_ctx_t;

static bool msg_has_kw(const char *message, size_t msg_len, const char *kw, size_t kw_len) {
    if (!message || msg_len < kw_len || kw_len == 0)
        return false;
    for (size_t i = 0; i + kw_len <= msg_len; i++) {
        if (memcmp(message + i, kw, kw_len) == 0)
            return true;
    }
    return false;
}

/* Task classification heuristics for routing */
static size_t classify_best_provider(const ensemble_ctx_t *ctx, const char *message, size_t msg_len) {
    if (ctx->config.provider_count == 0)
        return 0;

    bool needs_code = msg_has_kw(message, msg_len, "code", 4) ||
                      msg_has_kw(message, msg_len, "func", 4) ||
                      msg_has_kw(message, msg_len, "impl", 4) ||
                      msg_has_kw(message, msg_len, "bug", 3);
    bool needs_reasoning = msg_has_kw(message, msg_len, "why", 3) ||
                           msg_has_kw(message, msg_len, "math", 4) ||
                           msg_has_kw(message, msg_len, "prov", 4) ||
                           msg_has_kw(message, msg_len, "logic", 5);
    bool needs_creative = msg_has_kw(message, msg_len, "write", 5) ||
                          msg_has_kw(message, msg_len, "story", 5) ||
                          msg_has_kw(message, msg_len, "creat", 5);

    for (size_t p = 0; p < ctx->config.provider_count; p++) {
        const char *name = ctx->config.providers[p].vtable && ctx->config.providers[p].vtable->get_name
                               ? ctx->config.providers[p].vtable->get_name(ctx->config.providers[p].ctx)
                               : "";
        if (!name)
            continue;
        if (needs_code && (strstr(name, "openai") != NULL || strstr(name, "anthropic") != NULL))
            return p;
        if (needs_reasoning &&
            (strstr(name, "anthropic") != NULL || strstr(name, "deepseek") != NULL))
            return p;
        if (needs_creative &&
            (strstr(name, "google") != NULL || strstr(name, "gemini") != NULL))
            return p;
    }
    return 0;
}

static hu_error_t ensemble_chat_with_system(void *ctx, hu_allocator_t *alloc,
                                            const char *system_prompt, size_t system_prompt_len,
                                            const char *message, size_t message_len, const char *model,
                                            size_t model_len, double temperature, char **out,
                                            size_t *out_len) {
    ensemble_ctx_t *ectx = (ensemble_ctx_t *)ctx;
    (void)system_prompt;
    (void)system_prompt_len;
    if (ectx->config.provider_count == 0)
        return HU_ERR_INVALID_ARGUMENT;

    size_t idx;
    if (ectx->config.strategy == HU_ENSEMBLE_ROUND_ROBIN) {
        idx = ectx->round_robin_idx % ectx->config.provider_count;
        ectx->round_robin_idx++;
    } else if (ectx->config.strategy == HU_ENSEMBLE_CONSENSUS) {
        char *responses[HU_ENSEMBLE_MAX_PROVIDERS] = {0};
        size_t resp_lens[HU_ENSEMBLE_MAX_PROVIDERS] = {0};
        size_t valid = 0;

        for (size_t p = 0; p < ectx->config.provider_count; p++) {
            hu_provider_t *prov = &ectx->config.providers[p];
            if (prov->vtable && prov->vtable->chat_with_system) {
                hu_error_t err = prov->vtable->chat_with_system(
                    prov->ctx, alloc, system_prompt, system_prompt_len, message, message_len, model,
                    model_len, temperature, &responses[p], &resp_lens[p]);
                if (err == HU_OK && responses[p])
                    valid++;
            }
        }

        if (valid == 0)
            return HU_ERR_IO;

        if (valid == 1) {
            /* Only one response — use it directly */
            for (size_t p = 0; p < ectx->config.provider_count; p++) {
                if (responses[p]) {
                    *out = responses[p];
                    *out_len = resp_lens[p];
                    responses[p] = NULL;
                    break;
                }
            }
        } else {
            /* LLM rerank: ask an LLM to pick the best response */
            size_t fallback_best = 0;
            size_t fallback_len = 0;
            for (size_t p = 0; p < ectx->config.provider_count; p++) {
                if (responses[p] && resp_lens[p] > fallback_len) {
                    fallback_best = p;
                    fallback_len = resp_lens[p];
                }
            }
            size_t best = fallback_best;

            char rerank_prompt[8192];
            int rp_len = snprintf(rerank_prompt, sizeof(rerank_prompt),
                                  "Given this user message: \"%.*s\"\n\n"
                                  "Choose the BEST response from these candidates:\n",
                                  (int)(message_len < 500 ? message_len : 500), message);

            size_t candidate_idx[HU_ENSEMBLE_MAX_PROVIDERS];
            size_t num_candidates = 0;
            for (size_t p = 0; p < ectx->config.provider_count && rp_len < (int)sizeof(rerank_prompt) - 256;
                 p++) {
                if (!responses[p])
                    continue;
                candidate_idx[num_candidates] = p;
                num_candidates++;
                int added = snprintf(rerank_prompt + rp_len, sizeof(rerank_prompt) - (size_t)rp_len,
                                     "\n--- Response %zu ---\n%.*s\n", num_candidates,
                                     (int)(resp_lens[p] < 1000 ? resp_lens[p] : 1000), responses[p]);
                if (added > 0)
                    rp_len += added;
            }
            int added = snprintf(rerank_prompt + rp_len, sizeof(rerank_prompt) - (size_t)rp_len,
                                 "\nReply with ONLY the number (1-%zu) of the best response.",
                                 num_candidates);
            if (added > 0)
                rp_len += added;

            /* Use first available provider for reranking */
            for (size_t p = 0; p < ectx->config.provider_count; p++) {
                hu_provider_t *rprov = &ectx->config.providers[p];
                if (!rprov->vtable || !rprov->vtable->chat_with_system)
                    continue;
                static const char rank_sys[] =
                    "You are a response quality judge. Output ONLY a single number.";
                char *rank_out = NULL;
                size_t rank_out_len = 0;
                hu_error_t rerr = rprov->vtable->chat_with_system(
                    rprov->ctx, alloc, rank_sys, sizeof(rank_sys) - 1, rerank_prompt, (size_t)rp_len,
                    model, model_len, 0.0, &rank_out, &rank_out_len);
                if (rerr == HU_OK && rank_out && rank_out_len > 0) {
                    /* Parse the number from the response */
                    size_t choice = 0;
                    for (size_t ci = 0; ci < rank_out_len; ci++) {
                        if (rank_out[ci] >= '1' && rank_out[ci] <= '9') {
                            choice = (size_t)(rank_out[ci] - '0');
                            break;
                        }
                    }
                    if (choice >= 1 && choice <= num_candidates)
                        best = candidate_idx[choice - 1];
                    alloc->free(alloc->ctx, rank_out, rank_out_len + 1);
                } else if (rank_out) {
                    alloc->free(alloc->ctx, rank_out, rank_out_len + 1);
                }
                break; /* only use one provider for reranking */
            }

            *out = responses[best];
            *out_len = resp_lens[best];
            responses[best] = NULL;
        }

        for (size_t p = 0; p < ectx->config.provider_count; p++) {
            if (responses[p])
                alloc->free(alloc->ctx, responses[p], resp_lens[p] + 1);
        }
        return HU_OK;
    } else {
        idx = classify_best_provider(ectx, message, message_len);
    }

    hu_provider_t *selected = &ectx->config.providers[idx];
    if (!selected->vtable || !selected->vtable->chat_with_system)
        return HU_ERR_INVALID_ARGUMENT;
    return selected->vtable->chat_with_system(selected->ctx, alloc, system_prompt, system_prompt_len,
                                              message, message_len, model, model_len, temperature,
                                              out, out_len);
}

static void last_user_message(const hu_chat_request_t *request, const char **msg, size_t *msg_len) {
    *msg = "";
    *msg_len = 0;
    if (!request || request->messages_count == 0)
        return;
    for (size_t i = request->messages_count; i > 0; i--) {
        size_t j = i - 1;
        if (request->messages[j].role == HU_ROLE_USER && request->messages[j].content) {
            *msg = request->messages[j].content;
            *msg_len = request->messages[j].content_len;
            return;
        }
    }
}

static hu_error_t ensemble_chat(void *ctx, hu_allocator_t *alloc, const hu_chat_request_t *request,
                                const char *model, size_t model_len, double temperature,
                                hu_chat_response_t *out) {
    ensemble_ctx_t *ectx = (ensemble_ctx_t *)ctx;
    if (ectx->config.provider_count == 0)
        return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));

    if (ectx->config.strategy == HU_ENSEMBLE_CONSENSUS) {
        hu_chat_response_t responses[HU_ENSEMBLE_MAX_PROVIDERS];
        memset(responses, 0, sizeof(responses));
        size_t valid = 0;

        for (size_t p = 0; p < ectx->config.provider_count; p++) {
            hu_provider_t *prov = &ectx->config.providers[p];
            if (!prov->vtable || !prov->vtable->chat)
                continue;
            hu_error_t e =
                prov->vtable->chat(prov->ctx, alloc, request, model, model_len, temperature, &responses[p]);
            if (e != HU_OK) {
                hu_chat_response_free(alloc, &responses[p]);
                continue;
            }
            if (responses[p].content)
                valid++;
        }

        if (valid == 0) {
            for (size_t p = 0; p < ectx->config.provider_count; p++)
                hu_chat_response_free(alloc, &responses[p]);
            return HU_ERR_IO;
        }

        const char *umsg = NULL;
        size_t ulen = 0;
        last_user_message(request, &umsg, &ulen);

        size_t best = 0;
        if (valid == 1) {
            for (size_t p = 0; p < ectx->config.provider_count; p++) {
                if (responses[p].content) {
                    best = p;
                    break;
                }
            }
        } else {
            size_t fallback_best = 0;
            size_t fallback_len = 0;
            for (size_t p = 0; p < ectx->config.provider_count; p++) {
                if (!responses[p].content)
                    continue;
                if (responses[p].content_len > fallback_len) {
                    fallback_len = responses[p].content_len;
                    fallback_best = p;
                }
            }
            best = fallback_best;

            char rerank_prompt[8192];
            int rp_len = snprintf(rerank_prompt, sizeof(rerank_prompt),
                                  "Given this user message: \"%.*s\"\n\n"
                                  "Choose the BEST response from these candidates:\n",
                                  (int)(ulen < 500 ? ulen : 500), umsg);

            size_t candidate_idx[HU_ENSEMBLE_MAX_PROVIDERS];
            size_t num_candidates = 0;
            for (size_t p = 0; p < ectx->config.provider_count && rp_len < (int)sizeof(rerank_prompt) - 256;
                 p++) {
                if (!responses[p].content)
                    continue;
                candidate_idx[num_candidates] = p;
                num_candidates++;
                size_t clen = responses[p].content_len;
                int added = snprintf(rerank_prompt + rp_len, sizeof(rerank_prompt) - (size_t)rp_len,
                                     "\n--- Response %zu ---\n%.*s\n", num_candidates,
                                     (int)(clen < 1000 ? clen : 1000), responses[p].content);
                if (added > 0)
                    rp_len += added;
            }
            int added = snprintf(rerank_prompt + rp_len, sizeof(rerank_prompt) - (size_t)rp_len,
                                 "\nReply with ONLY the number (1-%zu) of the best response.",
                                 num_candidates);
            if (added > 0)
                rp_len += added;

            for (size_t p = 0; p < ectx->config.provider_count; p++) {
                hu_provider_t *rprov = &ectx->config.providers[p];
                if (!rprov->vtable || !rprov->vtable->chat_with_system)
                    continue;
                static const char rank_sys[] =
                    "You are a response quality judge. Output ONLY a single number.";
                char *rank_out = NULL;
                size_t rank_out_len = 0;
                hu_error_t rerr = rprov->vtable->chat_with_system(
                    rprov->ctx, alloc, rank_sys, sizeof(rank_sys) - 1, rerank_prompt, (size_t)rp_len,
                    model, model_len, 0.0, &rank_out, &rank_out_len);
                if (rerr == HU_OK && rank_out && rank_out_len > 0) {
                    size_t choice = 0;
                    for (size_t ci = 0; ci < rank_out_len; ci++) {
                        if (rank_out[ci] >= '1' && rank_out[ci] <= '9') {
                            choice = (size_t)(rank_out[ci] - '0');
                            break;
                        }
                    }
                    if (choice >= 1 && choice <= num_candidates)
                        best = candidate_idx[choice - 1];
                    alloc->free(alloc->ctx, rank_out, rank_out_len + 1);
                } else if (rank_out) {
                    alloc->free(alloc->ctx, rank_out, rank_out_len + 1);
                }
                break;
            }
        }

        for (size_t p = 0; p < ectx->config.provider_count; p++) {
            if (p != best)
                hu_chat_response_free(alloc, &responses[p]);
        }
        *out = responses[best];
        memset(&responses[best], 0, sizeof(responses[best]));
        return HU_OK;
    }

    size_t idx;
    if (ectx->config.strategy == HU_ENSEMBLE_ROUND_ROBIN) {
        idx = ectx->round_robin_idx % ectx->config.provider_count;
        ectx->round_robin_idx++;
    } else {
        const char *umsg = NULL;
        size_t ulen = 0;
        last_user_message(request, &umsg, &ulen);
        idx = classify_best_provider(ectx, umsg, ulen);
    }

    hu_provider_t *sel = &ectx->config.providers[idx];
    if (!sel->vtable || !sel->vtable->chat)
        return HU_ERR_INVALID_ARGUMENT;
    return sel->vtable->chat(sel->ctx, alloc, request, model, model_len, temperature, out);
}

static bool ensemble_supports_native_tools(void *ctx) {
    ensemble_ctx_t *ectx = (ensemble_ctx_t *)ctx;
    for (size_t i = 0; i < ectx->config.provider_count; i++) {
        if (ectx->config.providers[i].vtable &&
            ectx->config.providers[i].vtable->supports_native_tools &&
            ectx->config.providers[i].vtable->supports_native_tools(ectx->config.providers[i].ctx))
            return true;
    }
    return false;
}

static const char *ensemble_get_name(void *ctx) {
    (void)ctx;
    return "ensemble";
}

static void ensemble_deinit(void *ctx, hu_allocator_t *alloc) {
    ensemble_ctx_t *ectx = (ensemble_ctx_t *)ctx;
    for (size_t i = 0; i < ectx->config.provider_count; i++) {
        if (ectx->config.providers[i].vtable && ectx->config.providers[i].vtable->deinit)
            ectx->config.providers[i].vtable->deinit(ectx->config.providers[i].ctx, alloc);
    }
    alloc->free(alloc->ctx, ectx, sizeof(ensemble_ctx_t));
}

static bool ensemble_supports_vision(void *ctx) {
    ensemble_ctx_t *ectx = (ensemble_ctx_t *)ctx;
    for (size_t i = 0; i < ectx->config.provider_count; i++) {
        if (ectx->config.providers[i].vtable &&
            ectx->config.providers[i].vtable->supports_vision &&
            ectx->config.providers[i].vtable->supports_vision(ectx->config.providers[i].ctx))
            return true;
    }
    return false;
}

static const hu_provider_vtable_t ensemble_vtable = {
    .chat_with_system = ensemble_chat_with_system,
    .chat = ensemble_chat,
    .supports_native_tools = ensemble_supports_native_tools,
    .get_name = ensemble_get_name,
    .deinit = ensemble_deinit,
    .supports_vision = ensemble_supports_vision,
};

hu_error_t hu_ensemble_create(hu_allocator_t *alloc, const hu_ensemble_spec_t *config,
                              hu_provider_t *out) {
    if (!alloc || !config || !out || config->provider_count == 0 ||
        config->provider_count > HU_ENSEMBLE_MAX_PROVIDERS)
        return HU_ERR_INVALID_ARGUMENT;
    ensemble_ctx_t *ectx = (ensemble_ctx_t *)alloc->alloc(alloc->ctx, sizeof(ensemble_ctx_t));
    if (!ectx)
        return HU_ERR_OUT_OF_MEMORY;
    ectx->config = *config;
    ectx->round_robin_idx = 0;
    out->vtable = &ensemble_vtable;
    out->ctx = ectx;
    return HU_OK;
}
