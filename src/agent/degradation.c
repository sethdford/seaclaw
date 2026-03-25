#include "human/agent/degradation.h"
#include <string.h>

#ifndef HU_IS_TEST
#define HU_IS_TEST 0
#endif

static const char HONEST_FAILURE[] =
    "I'm having trouble connecting to my AI provider right now. Please try again shortly.";

char *hu_degradation_honest_failure_msg(hu_allocator_t *alloc, size_t *out_len) {
    if (!alloc || !out_len) return NULL;
    size_t len = sizeof(HONEST_FAILURE) - 1;
    char *msg = (char *)alloc->alloc(alloc->ctx, len + 1);
    if (msg) {
        memcpy(msg, HONEST_FAILURE, len);
        msg[len] = '\0';
    }
    *out_len = len;
    return msg;
}

static hu_error_t try_chat(hu_provider_t *provider, hu_allocator_t *alloc,
                           const hu_chat_request_t *request,
                           const char *model, size_t model_len,
                           double temperature, hu_chat_response_t *resp) {
    if (!provider->vtable || !provider->vtable->chat)
        return HU_ERR_NOT_SUPPORTED;
    return provider->vtable->chat(provider->ctx, alloc, request,
                                  model, model_len, temperature, resp);
}

hu_error_t hu_provider_degrade_chat(hu_provider_degradation_config_t *config,
                                    hu_provider_t *provider, hu_allocator_t *alloc,
                                    const hu_chat_request_t *request,
                                    const char *model, size_t model_len,
                                    double temperature,
                                    hu_degradation_result_t *out) {
    if (!provider || !alloc || !request || !out)
        return HU_ERR_INVALID_ARGUMENT;

    memset(out, 0, sizeof(*out));

    /* Passthrough when disabled or no config */
    if (!config || !config->enabled) {
        hu_error_t err = try_chat(provider, alloc, request, model, model_len,
                                  temperature, &out->response);
        out->strategy_used = HU_DEGRADE_PRIMARY;
        out->attempts = 1;
        return err;
    }

    uint32_t max_retries = config->max_retries > 0 ? config->max_retries : 1;

    /* Check circuit breaker first */
    if (!hu_circuit_breaker_allow(&config->breaker)) {
        out->strategy_used = HU_DEGRADE_HONEST_FAILURE;
        out->attempts = 0;
        char *msg = hu_degradation_honest_failure_msg(alloc, &out->response.content_len);
        if (!msg) return HU_ERR_OUT_OF_MEMORY;
        out->response.content = msg;
        /* Non-OK so CLI/scripts exit non-zero while still returning user-visible text in out->response. */
        return HU_ERR_PROVIDER_RESPONSE;
    }

    /* Try primary model */
    for (uint32_t attempt = 0; attempt < max_retries; attempt++) {
        hu_error_t err = try_chat(provider, alloc, request, model, model_len,
                                  temperature, &out->response);
        out->attempts = attempt + 1;
        if (err == HU_OK) {
            out->strategy_used = HU_DEGRADE_PRIMARY;
            hu_circuit_breaker_record_success(&config->breaker);
            return HU_OK;
        }
    }

    /* Primary failed — record failure and try fallback */
    hu_circuit_breaker_record_failure(&config->breaker);

    if (config->fallback_model && config->fallback_model_len > 0) {
        memset(&out->response, 0, sizeof(out->response));
        for (uint32_t attempt = 0; attempt < max_retries; attempt++) {
            hu_error_t err = try_chat(provider, alloc, request,
                                      config->fallback_model, config->fallback_model_len,
                                      temperature, &out->response);
            out->attempts += 1;
            if (err == HU_OK) {
                out->strategy_used = HU_DEGRADE_FALLBACK;
                hu_circuit_breaker_record_success(&config->breaker);
                return HU_OK;
            }
        }
        hu_circuit_breaker_record_failure(&config->breaker);
    }

    /* All attempts failed — honest failure */
    memset(&out->response, 0, sizeof(out->response));
    out->strategy_used = HU_DEGRADE_HONEST_FAILURE;
    char *msg = hu_degradation_honest_failure_msg(alloc, &out->response.content_len);
    if (!msg) return HU_ERR_OUT_OF_MEMORY;
    out->response.content = msg;
    return HU_ERR_PROVIDER_RESPONSE;
}
