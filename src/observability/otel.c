#include "human/observability/otel.h"
#include "human/core/http.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>


#define MAX_ATTRS 32

typedef struct hu_otel_ctx {
    hu_allocator_t *alloc;
    char *endpoint;
    char *service_name;
    bool enable_traces;
    bool enable_metrics;
    bool enable_logs;
} hu_otel_ctx_t;

typedef struct hu_attr {
    char *key;
    char *str_val;
    int64_t int_val;
    double dbl_val;
    enum { ATTR_STR, ATTR_INT, ATTR_DBL } kind;
} hu_attr_t;

struct hu_span {
    hu_allocator_t *alloc;
    char *name;
    char trace_id[33];
    char span_id[17];
    char parent_span_id[17];
    uint64_t start_time_ns;
    uint64_t end_time_ns;
    int status;
    hu_attr_t attrs[MAX_ATTRS];
    size_t attr_count;
};

static hu_otel_config_t *g_otel_config;
static char *g_otel_endpoint;
static char *g_otel_service_name;

static uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static void generate_hex_id(char *buf, size_t len, int num_bytes) {
    (void)len;
    static uint32_t seed;
    static int seeded;
    if (!seeded) {
        seed = (uint32_t)time(NULL) ^ (uint32_t)(uintptr_t)&seed;
        seeded = 1;
    }
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < num_bytes; i++) {
        uint32_t r = xorshift32(&seed);
        buf[i * 2] = hex[(r >> 4) & 0xf];
        buf[i * 2 + 1] = hex[r & 0xf];
    }
    buf[num_bytes * 2] = '\0';
}

static uint64_t now_ns(void) {
#if (defined(__linux__) || defined(__APPLE__)) && defined(CLOCK_REALTIME)
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0)
        return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
    return (uint64_t)time(NULL) * 1000000000ULL;
}

#if !HU_IS_TEST && defined(HU_HTTP_CURL)
static const char *event_tag_str(hu_observer_event_tag_t tag) {
    switch (tag) {
    case HU_OBSERVER_EVENT_AGENT_START:
        return "agent.start";
    case HU_OBSERVER_EVENT_LLM_REQUEST:
        return "llm.request";
    case HU_OBSERVER_EVENT_LLM_RESPONSE:
        return "llm.response";
    case HU_OBSERVER_EVENT_AGENT_END:
        return "agent.end";
    case HU_OBSERVER_EVENT_TOOL_CALL_START:
        return "tool.call.start";
    case HU_OBSERVER_EVENT_TOOL_CALL:
        return "tool.call";
    case HU_OBSERVER_EVENT_TOOL_ITERATIONS_EXHAUSTED:
        return "tool.iterations.exhausted";
    case HU_OBSERVER_EVENT_TURN_COMPLETE:
        return "turn.complete";
    case HU_OBSERVER_EVENT_CHANNEL_MESSAGE:
        return "channel.message";
    case HU_OBSERVER_EVENT_HEARTBEAT_TICK:
        return "heartbeat";
    case HU_OBSERVER_EVENT_ERR:
        return "error";
    case HU_OBSERVER_EVENT_COGNITION_MODE:
        return "cognition.mode";
    case HU_OBSERVER_EVENT_METACOG_ACTION:
        return "metacog.action";
    default:
        return "unknown";
    }
}

static const char *metric_tag_str(hu_observer_metric_tag_t tag) {
    switch (tag) {
    case HU_OBSERVER_METRIC_REQUEST_LATENCY_MS:
        return "request.latency_ms";
    case HU_OBSERVER_METRIC_TOKENS_USED:
        return "tokens.used";
    case HU_OBSERVER_METRIC_ACTIVE_SESSIONS:
        return "sessions.active";
    case HU_OBSERVER_METRIC_QUEUE_DEPTH:
        return "queue.depth";
    default:
        return "unknown";
    }
}
#endif

static void otel_record_event(void *ctx, const hu_observer_event_t *event) {
    hu_otel_ctx_t *c = (hu_otel_ctx_t *)ctx;
    if (!c || !event || !c->enable_logs || !c->endpoint)
        return;
#if !HU_IS_TEST && defined(HU_HTTP_CURL)
    char body[2048];
    const char *svc = c->service_name ? c->service_name : "human";
    int n = snprintf(
        body, sizeof(body),
        "{\"resourceLogs\":[{\"resource\":{\"attributes\":[{\"key\":\"service.name\","
        "\"value\":{\"stringValue\":\"%s\"}}]},\"scopeLogs\":[{\"logRecords\":[{"
        "\"timeUnixNano\":\"%lld000000000\",\"body\":{\"stringValue\":\"%s\"},"
        "\"attributes\":[{\"key\":\"event.tag\",\"value\":{\"stringValue\":\"%s\"}}]}]}]}]}",
        svc, (long long)time(NULL), event_tag_str(event->tag), event_tag_str(event->tag));
    if (n <= 0 || (size_t)n >= sizeof(body))
        return;
    char url[512];
    snprintf(url, sizeof(url), "%s/v1/logs", c->endpoint);
    hu_http_response_t resp = {0};
    hu_error_t herr = hu_http_post_json(c->alloc, url, NULL, body, (size_t)n, &resp);
    (void)herr;
    if (resp.owned && resp.body)
        hu_http_response_free(c->alloc, &resp);
#else
    (void)c;
    (void)event;
#endif
}

static void otel_record_metric(void *ctx, const hu_observer_metric_t *metric) {
    hu_otel_ctx_t *c = (hu_otel_ctx_t *)ctx;
    if (!c || !metric || !c->enable_metrics || !c->endpoint)
        return;
#if !HU_IS_TEST && defined(HU_HTTP_CURL)
    char body[2048];
    const char *svc = c->service_name ? c->service_name : "human";
    int n = snprintf(
        body, sizeof(body),
        "{\"resourceMetrics\":[{\"resource\":{\"attributes\":[{\"key\":\"service.name\","
        "\"value\":{\"stringValue\":\"%s\"}}]},\"scopeMetrics\":[{\"metrics\":[{"
        "\"name\":\"%s\",\"gauge\":{\"dataPoints\":[{\"timeUnixNano\":\"%lld000000000\","
        "\"asInt\":\"%llu\"}]}}]}]}]}",
        svc, metric_tag_str(metric->tag), (long long)time(NULL), (unsigned long long)metric->value);
    if (n <= 0 || (size_t)n >= sizeof(body))
        return;
    char url[512];
    snprintf(url, sizeof(url), "%s/v1/metrics", c->endpoint);
    hu_http_response_t resp = {0};
    hu_error_t merr = hu_http_post_json(c->alloc, url, NULL, body, (size_t)n, &resp);
    (void)merr;
    if (resp.owned && resp.body)
        hu_http_response_free(c->alloc, &resp);
#else
    (void)c;
    (void)metric;
#endif
}

static void otel_flush(void *ctx) {
    (void)ctx;
}

static const char *otel_observer_name(void *ctx) {
    (void)ctx;
    return "otel";
}

static void otel_deinit(void *ctx) {
    hu_otel_ctx_t *c = (hu_otel_ctx_t *)ctx;
    if (!c || !c->alloc)
        return;
    hu_allocator_t *alloc = c->alloc;
    if (c->endpoint)
        alloc->free(alloc->ctx, c->endpoint, strlen(c->endpoint) + 1);
    if (c->service_name)
        alloc->free(alloc->ctx, c->service_name, strlen(c->service_name) + 1);
    alloc->free(alloc->ctx, c, sizeof(*c));
}

static const hu_observer_vtable_t otel_vtable = {
    .record_event = otel_record_event,
    .record_metric = otel_record_metric,
    .flush = otel_flush,
    .name = otel_observer_name,
    .deinit = otel_deinit,
};

#if !HU_IS_TEST && defined(HU_HTTP_CURL)
static void otel_export_span(hu_allocator_t *alloc, const hu_span_t *span,
                             const char *endpoint, const char *service_name) {
    if (!alloc || !span || !endpoint || !span->trace_id[0] || !span->span_id[0])
        return;
    hu_json_buf_t buf;
    if (hu_json_buf_init(&buf, alloc) != HU_OK)
        return;
    const char *svc = service_name ? service_name : "human";
    if (hu_json_buf_append_raw(&buf, "{\"resourceSpans\":[{\"resource\":{\"attributes\":[{\"key\":\"service.name\",\"value\":{\"stringValue\":\"", 95) != HU_OK)
        goto cleanup;
    if (hu_json_append_string(&buf, svc, strlen(svc)) != HU_OK)
        goto cleanup;
    if (hu_json_buf_append_raw(&buf, "\"}}]},\"scopeSpans\":[{\"spans\":[{\"traceId\":\"", 47) != HU_OK)
        goto cleanup;
    if (hu_json_buf_append_raw(&buf, span->trace_id, 32) != HU_OK)
        goto cleanup;
    if (hu_json_buf_append_raw(&buf, "\",\"spanId\":\"", 12) != HU_OK)
        goto cleanup;
    if (hu_json_buf_append_raw(&buf, span->span_id, 16) != HU_OK)
        goto cleanup;
    if (span->parent_span_id[0]) {
        if (hu_json_buf_append_raw(&buf, "\",\"parentSpanId\":\"", 17) != HU_OK)
            goto cleanup;
        if (hu_json_buf_append_raw(&buf, span->parent_span_id, 16) != HU_OK)
            goto cleanup;
    }
    if (hu_json_buf_append_raw(&buf, "\",\"name\":\"", 9) != HU_OK)
        goto cleanup;
    if (span->name && hu_json_append_string(&buf, span->name, strlen(span->name)) != HU_OK)
        goto cleanup;
    char ts_buf[128];
    int n = snprintf(ts_buf, sizeof(ts_buf), "\",\"startTimeUnixNano\":\"%llu\",\"endTimeUnixNano\":\"%llu\"",
                    (unsigned long long)span->start_time_ns, (unsigned long long)span->end_time_ns);
    if (n <= 0 || (size_t)n >= sizeof(ts_buf) || hu_json_buf_append_raw(&buf, ts_buf, (size_t)n) != HU_OK)
        goto cleanup;
    if (span->attr_count > 0) {
        if (hu_json_buf_append_raw(&buf, ",\"attributes\":[", 15) != HU_OK)
            goto cleanup;
        for (size_t i = 0; i < span->attr_count; i++) {
            const hu_attr_t *a = &span->attrs[i];
            if (i > 0 && hu_json_buf_append_raw(&buf, ",", 1) != HU_OK)
                goto cleanup;
            if (hu_json_buf_append_raw(&buf, "{\"key\":\"", 8) != HU_OK)
                goto cleanup;
            if (a->key && hu_json_append_string(&buf, a->key, strlen(a->key)) != HU_OK)
                goto cleanup;
            if (a->kind == ATTR_STR) {
                if (hu_json_buf_append_raw(&buf, "\",\"value\":{\"stringValue\":\"", 26) != HU_OK)
                    goto cleanup;
                if (a->str_val && hu_json_append_string(&buf, a->str_val, strlen(a->str_val)) != HU_OK)
                    goto cleanup;
                if (hu_json_buf_append_raw(&buf, "\"}}", 3) != HU_OK)
                    goto cleanup;
            } else if (a->kind == ATTR_INT) {
                n = snprintf(ts_buf, sizeof(ts_buf), "\",\"value\":{\"intValue\":\"%lld\"}}", (long long)a->int_val);
                if (n <= 0 || (size_t)n >= sizeof(ts_buf) || hu_json_buf_append_raw(&buf, ts_buf, (size_t)n) != HU_OK)
                    goto cleanup;
            } else {
                n = snprintf(ts_buf, sizeof(ts_buf), "\",\"value\":{\"doubleValue\":%.17g}}", a->dbl_val);
                if (n <= 0 || (size_t)n >= sizeof(ts_buf) || hu_json_buf_append_raw(&buf, ts_buf, (size_t)n) != HU_OK)
                    goto cleanup;
            }
        }
        if (hu_json_buf_append_raw(&buf, "]", 1) != HU_OK)
            goto cleanup;
    }
    if (hu_json_buf_append_raw(&buf, "}]}]}]}", 8) != HU_OK)
        goto cleanup;
    char url[512];
    snprintf(url, sizeof(url), "%s/v1/traces", endpoint);
    hu_http_response_t resp = {0};
    hu_http_post_json(alloc, url, NULL, buf.ptr, buf.len, &resp);
    if (resp.owned && resp.body)
        hu_http_response_free(alloc, &resp);
cleanup:
    hu_json_buf_free(&buf);
}
#endif

void hu_otel_set_global_config(const hu_otel_config_t *cfg) {
    hu_allocator_t alloc = hu_system_allocator();
    if (g_otel_config) {
        if (g_otel_endpoint)
            alloc.free(alloc.ctx, g_otel_endpoint, strlen(g_otel_endpoint) + 1);
        if (g_otel_service_name)
            alloc.free(alloc.ctx, g_otel_service_name, strlen(g_otel_service_name) + 1);
        alloc.free(alloc.ctx, g_otel_config, sizeof(*g_otel_config));
        g_otel_config = NULL;
        g_otel_endpoint = NULL;
        g_otel_service_name = NULL;
    }
    if (!cfg)
        return;
    g_otel_config = (hu_otel_config_t *)alloc.alloc(alloc.ctx, sizeof(*g_otel_config));
    if (!g_otel_config)
        return;
    memcpy(g_otel_config, cfg, sizeof(*g_otel_config));
    g_otel_config->endpoint = NULL;
    g_otel_config->service_name = NULL;
    if (cfg->endpoint && cfg->endpoint_len > 0) {
        g_otel_endpoint = hu_strndup(&alloc, cfg->endpoint, cfg->endpoint_len);
        g_otel_config->endpoint = g_otel_endpoint;
        g_otel_config->endpoint_len = cfg->endpoint_len;
    }
    if (cfg->service_name && cfg->service_name_len > 0) {
        g_otel_service_name = hu_strndup(&alloc, cfg->service_name, cfg->service_name_len);
        g_otel_config->service_name = g_otel_service_name;
        g_otel_config->service_name_len = cfg->service_name_len;
    }
}

hu_error_t hu_otel_observer_create(hu_allocator_t *alloc, const hu_otel_config_t *cfg,
                                   hu_observer_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    hu_otel_ctx_t *c = (hu_otel_ctx_t *)alloc->alloc(alloc->ctx, sizeof(*c));
    if (!c)
        return HU_ERR_OUT_OF_MEMORY;
    memset(c, 0, sizeof(*c));
    c->alloc = alloc;
    if (cfg) {
        c->endpoint = cfg->endpoint ? hu_strndup(alloc, cfg->endpoint, cfg->endpoint_len) : NULL;
        c->service_name =
            cfg->service_name ? hu_strndup(alloc, cfg->service_name, cfg->service_name_len) : NULL;
        c->enable_traces = cfg->enable_traces;
        c->enable_metrics = cfg->enable_metrics;
        c->enable_logs = cfg->enable_logs;
    }
    out->ctx = c;
    out->vtable = &otel_vtable;
    return HU_OK;
}

hu_span_t *hu_span_start(hu_allocator_t *alloc, const char *name, size_t name_len) {
    if (!alloc || !name)
        return NULL;
    hu_span_t *s = (hu_span_t *)alloc->alloc(alloc->ctx, sizeof(*s));
    if (!s)
        return NULL;
    memset(s, 0, sizeof(*s));
    s->alloc = alloc;
    s->name = hu_strndup(alloc, name, name_len);
    generate_hex_id(s->trace_id, sizeof(s->trace_id), 16);
    generate_hex_id(s->span_id, sizeof(s->span_id), 8);
    s->start_time_ns = now_ns();
    s->status = HU_SPAN_STATUS_UNSET;
    return s;
}

hu_span_t *hu_span_start_child(hu_allocator_t *alloc, hu_span_t *parent,
                               const char *name, size_t name_len) {
    if (!alloc || !parent || !name)
        return NULL;
    hu_span_t *s = (hu_span_t *)alloc->alloc(alloc->ctx, sizeof(*s));
    if (!s)
        return NULL;
    memset(s, 0, sizeof(*s));
    s->alloc = alloc;
    s->name = hu_strndup(alloc, name, name_len);
    memcpy(s->trace_id, parent->trace_id, sizeof(s->trace_id));
    generate_hex_id(s->span_id, sizeof(s->span_id), 8);
    memcpy(s->parent_span_id, parent->span_id, sizeof(s->parent_span_id));
    s->start_time_ns = now_ns();
    s->status = HU_SPAN_STATUS_UNSET;
    return s;
}

void hu_span_set_attr_str(hu_span_t *span, const char *key, const char *value) {
    if (!span || !key || span->attr_count >= MAX_ATTRS)
        return;
    hu_attr_t *a = &span->attrs[span->attr_count++];
    a->kind = ATTR_STR;
    a->key = hu_strndup(span->alloc, key, strlen(key));
    a->str_val = value ? hu_strndup(span->alloc, value, strlen(value)) : NULL;
}

void hu_span_set_attr_int(hu_span_t *span, const char *key, int64_t value) {
    if (!span || !key || span->attr_count >= MAX_ATTRS)
        return;
    hu_attr_t *a = &span->attrs[span->attr_count++];
    a->kind = ATTR_INT;
    a->key = hu_strndup(span->alloc, key, strlen(key));
    a->int_val = value;
}

void hu_span_set_attr_double(hu_span_t *span, const char *key, double value) {
    if (!span || !key || span->attr_count >= MAX_ATTRS)
        return;
    hu_attr_t *a = &span->attrs[span->attr_count++];
    a->kind = ATTR_DBL;
    a->key = hu_strndup(span->alloc, key, strlen(key));
    a->dbl_val = value;
}

void hu_span_set_status(hu_span_t *span, hu_span_status_t status) {
    if (!span)
        return;
    span->status = (int)status;
}

const char *hu_span_get_trace_id(const hu_span_t *span) {
    if (!span || !span->trace_id[0])
        return NULL;
    return span->trace_id;
}

const char *hu_span_get_span_id(const hu_span_t *span) {
    if (!span || !span->span_id[0])
        return NULL;
    return span->span_id;
}

void hu_span_set_genai_system(hu_span_t *span, const char *system) {
    if (span && system)
        hu_span_set_attr_str(span, "gen_ai.system", system);
}

void hu_span_set_genai_model(hu_span_t *span, const char *model) {
    if (span && model)
        hu_span_set_attr_str(span, "gen_ai.request.model", model);
}

void hu_span_set_genai_tokens(hu_span_t *span, int64_t input, int64_t output) {
    if (span) {
        hu_span_set_attr_int(span, "gen_ai.usage.input_tokens", input);
        hu_span_set_attr_int(span, "gen_ai.usage.output_tokens", output);
    }
}

void hu_span_set_genai_operation(hu_span_t *span, const char *op) {
    if (span && op)
        hu_span_set_attr_str(span, "gen_ai.operation.name", op);
}

hu_error_t hu_span_end(hu_span_t *span, hu_allocator_t *alloc) {
    if (!span || !alloc)
        return HU_ERR_INVALID_ARGUMENT;
    span->end_time_ns = now_ns();
#if !HU_IS_TEST && defined(HU_HTTP_CURL)
    if (g_otel_config && g_otel_config->enable_traces && g_otel_config->endpoint) {
        otel_export_span(alloc, span, g_otel_config->endpoint, g_otel_config->service_name);
    }
#endif
    for (size_t i = 0; i < span->attr_count; i++) {
        hu_attr_t *a = &span->attrs[i];
        if (a->key)
            alloc->free(alloc->ctx, a->key, strlen(a->key) + 1);
        if (a->kind == ATTR_STR && a->str_val)
            alloc->free(alloc->ctx, a->str_val, strlen(a->str_val) + 1);
    }
    if (span->name)
        alloc->free(alloc->ctx, span->name, strlen(span->name) + 1);
    alloc->free(alloc->ctx, span, sizeof(*span));
    return HU_OK;
}
