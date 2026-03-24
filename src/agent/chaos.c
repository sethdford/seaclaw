#include "human/agent/chaos.h"
#include <stdio.h>
#include <string.h>

static uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

void hu_chaos_engine_init(hu_chaos_engine_t *e, const hu_chaos_config_t *config) {
    if (!e)
        return;
    memset(e, 0, sizeof(*e));
    if (config)
        e->config = *config;
    else
        e->config = (hu_chaos_config_t)HU_CHAOS_CONFIG_DEFAULT;
    e->prng_state = e->config.seed ? e->config.seed : 42;
}

hu_chaos_fault_t hu_chaos_maybe_inject(hu_chaos_engine_t *e) {
    if (!e || !e->config.enabled)
        return HU_CHAOS_NONE;
    e->stats.total_calls++;

    if (e->config.forced_fault != HU_CHAOS_NONE) {
        e->stats.faults_injected++;
        return e->config.forced_fault;
    }

    uint32_t r = xorshift32(&e->prng_state);
    double normalized = (double)(r & 0xFFFF) / 65535.0;
    if (normalized >= e->config.fault_probability)
        return HU_CHAOS_NONE;

    e->stats.faults_injected++;
    uint32_t fault_r = xorshift32(&e->prng_state);
    hu_chaos_fault_t fault = (hu_chaos_fault_t)(1 + (fault_r % (HU_CHAOS_FAULT_COUNT - 1)));

    switch (fault) {
    case HU_CHAOS_TIMEOUT:
        e->stats.timeouts++;
        break;
    case HU_CHAOS_EMPTY_RESPONSE:
        e->stats.empty_responses++;
        break;
    case HU_CHAOS_GARBAGE_RESPONSE:
        e->stats.garbage_responses++;
        break;
    case HU_CHAOS_PARTIAL_RESPONSE:
        e->stats.partial_responses++;
        break;
    case HU_CHAOS_ERROR_CODE:
        e->stats.error_codes++;
        break;
    case HU_CHAOS_SLOW_RESPONSE:
        e->stats.slow_responses++;
        break;
    case HU_CHAOS_CORRUPT_JSON:
        e->stats.corrupt_json++;
        break;
    default:
        break;
    }
    return fault;
}

hu_error_t hu_chaos_apply_to_response(hu_chaos_engine_t *e, hu_allocator_t *alloc,
                                      hu_chaos_fault_t fault, hu_chat_response_t *resp) {
    if (!e || !resp)
        return HU_ERR_INVALID_ARGUMENT;
    (void)alloc;

    switch (fault) {
    case HU_CHAOS_NONE:
        return HU_OK;
    case HU_CHAOS_TIMEOUT:
        return HU_ERR_TIMEOUT;
    case HU_CHAOS_EMPTY_RESPONSE:
        resp->content = NULL;
        resp->content_len = 0;
        return HU_OK;
    case HU_CHAOS_GARBAGE_RESPONSE:
        if (alloc && resp->content_len > 0) {
            char *garbage = (char *)alloc->alloc(alloc->ctx, resp->content_len + 1);
            if (garbage) {
                for (size_t i = 0; i < resp->content_len; i++)
                    garbage[i] = (char)(0x20 + (xorshift32(&e->prng_state) % 95));
                garbage[resp->content_len] = '\0';
                if (resp->content)
                    alloc->free(alloc->ctx, (void *)resp->content, resp->content_len + 1);
                resp->content = garbage;
            }
        }
        return HU_OK;
    case HU_CHAOS_PARTIAL_RESPONSE:
        if (resp->content_len > 2)
            resp->content_len = resp->content_len / 2;
        return HU_OK;
    case HU_CHAOS_ERROR_CODE:
        return HU_ERR_PROVIDER_RESPONSE;
    case HU_CHAOS_SLOW_RESPONSE:
        return HU_OK;
    case HU_CHAOS_CORRUPT_JSON:
        if (alloc) {
            static const char corrupt[] = "{\"broken\": true, \"data\": [1,2,";
            char *c = (char *)alloc->alloc(alloc->ctx, sizeof(corrupt));
            if (c) {
                memcpy(c, corrupt, sizeof(corrupt));
                if (resp->content)
                    alloc->free(alloc->ctx, (void *)resp->content, resp->content_len + 1);
                resp->content = c;
                resp->content_len = sizeof(corrupt) - 1;
            }
        }
        return HU_OK;
    default:
        return HU_OK;
    }
}

hu_error_t hu_chaos_apply_to_tool_result(hu_chaos_engine_t *e, hu_allocator_t *alloc,
                                         hu_chaos_fault_t fault, hu_tool_result_t *result) {
    if (!e || !result)
        return HU_ERR_INVALID_ARGUMENT;
    (void)alloc;

    switch (fault) {
    case HU_CHAOS_NONE:
        return HU_OK;
    case HU_CHAOS_TIMEOUT:
        result->success = false;
        result->output = "timeout";
        result->output_len = 7;
        return HU_OK;
    case HU_CHAOS_EMPTY_RESPONSE:
        result->output = "";
        result->output_len = 0;
        return HU_OK;
    case HU_CHAOS_ERROR_CODE:
        result->success = false;
        result->output = "chaos: simulated error";
        result->output_len = 22;
        return HU_OK;
    case HU_CHAOS_GARBAGE_RESPONSE:
        result->output = "\xff\xfe\xfd garbage data \x01\x02\x03";
        result->output_len = 27;
        return HU_OK;
    case HU_CHAOS_PARTIAL_RESPONSE:
        if (result->output_len > 4)
            result->output_len = result->output_len / 3;
        return HU_OK;
    default:
        return HU_OK;
    }
}

size_t hu_chaos_report(const hu_chaos_engine_t *e, char *buf, size_t buf_size) {
    if (!e || !buf || buf_size == 0)
        return 0;
    int n = snprintf(buf, buf_size,
                     "chaos: %zu calls, %zu faults (%.1f%%), "
                     "timeout=%zu empty=%zu garbage=%zu partial=%zu error=%zu slow=%zu json=%zu",
                     e->stats.total_calls, e->stats.faults_injected,
                     e->stats.total_calls > 0
                         ? 100.0 * (double)e->stats.faults_injected / (double)e->stats.total_calls
                         : 0.0,
                     e->stats.timeouts, e->stats.empty_responses, e->stats.garbage_responses,
                     e->stats.partial_responses, e->stats.error_codes, e->stats.slow_responses,
                     e->stats.corrupt_json);
    return (n > 0 && (size_t)n < buf_size) ? (size_t)n : 0;
}

const char *hu_chaos_fault_name(hu_chaos_fault_t fault) {
    switch (fault) {
    case HU_CHAOS_NONE:
        return "none";
    case HU_CHAOS_TIMEOUT:
        return "timeout";
    case HU_CHAOS_EMPTY_RESPONSE:
        return "empty_response";
    case HU_CHAOS_GARBAGE_RESPONSE:
        return "garbage_response";
    case HU_CHAOS_PARTIAL_RESPONSE:
        return "partial_response";
    case HU_CHAOS_ERROR_CODE:
        return "error_code";
    case HU_CHAOS_SLOW_RESPONSE:
        return "slow_response";
    case HU_CHAOS_CORRUPT_JSON:
        return "corrupt_json";
    default:
        return "unknown";
    }
}
