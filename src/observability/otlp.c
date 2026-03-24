#include "human/observability/otlp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void hu_otlp_trace_init(hu_otlp_trace_t *trace) {
    if (!trace)
        return;
    memset(trace, 0, sizeof(*trace));
}

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void gen_hex_id(char *buf, size_t bytes) {
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < bytes; i++) {
        unsigned int r = (unsigned int)rand();
        buf[i * 2] = hex[(r >> 4) & 0xF];
        buf[i * 2 + 1] = hex[r & 0xF];
    }
    buf[bytes * 2] = '\0';
}

hu_error_t hu_otlp_span_begin(hu_otlp_trace_t *trace, const char *name, size_t name_len,
                              const char *parent_span_id, hu_otlp_span_t **out) {
    if (!trace || !name || !out)
        return HU_ERR_INVALID_ARGUMENT;
    if (trace->span_count >= HU_OTLP_MAX_SPANS)
        return HU_ERR_OUT_OF_MEMORY;

    hu_otlp_span_t *span = &trace->spans[trace->span_count];
    memset(span, 0, sizeof(*span));

    if (trace->span_count == 0)
        gen_hex_id(span->trace_id, 16);
    else
        memcpy(span->trace_id, trace->spans[0].trace_id, sizeof(span->trace_id));

    gen_hex_id(span->span_id, 8);

    if (parent_span_id) {
        size_t plen = strlen(parent_span_id);
        if (plen < sizeof(span->parent_span_id))
            memcpy(span->parent_span_id, parent_span_id, plen + 1);
    }

    span->name = name;
    span->name_len = name_len;
    span->start_ns = now_ns();

    trace->span_count++;
    *out = span;
    return HU_OK;
}

hu_error_t hu_otlp_span_end(hu_otlp_span_t *span, int status) {
    if (!span)
        return HU_ERR_INVALID_ARGUMENT;
    span->end_ns = now_ns();
    span->status = status;
    return HU_OK;
}

hu_error_t hu_otlp_trace_to_json(hu_allocator_t *alloc, const hu_otlp_trace_t *trace,
                                 char **out_json, size_t *out_len) {
    if (!alloc || !trace || !out_json || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

    size_t buf_size = 256 + trace->span_count * 512;
    char *buf = (char *)alloc->alloc(alloc->ctx, buf_size);
    if (!buf)
        return HU_ERR_OUT_OF_MEMORY;

    size_t pos = 0;
    pos += (size_t)snprintf(buf + pos, buf_size - pos,
                            "{\"resourceSpans\":[{\"resource\":{},\"scopeSpans\":[{\"spans\":[");

    for (size_t i = 0; i < trace->span_count; i++) {
        const hu_otlp_span_t *s = &trace->spans[i];
        if (i > 0)
            buf[pos++] = ',';
        int n = snprintf(buf + pos, buf_size - pos,
                         "{\"traceId\":\"%s\",\"spanId\":\"%s\","
                         "\"parentSpanId\":\"%s\","
                         "\"name\":\"%.*s\","
                         "\"startTimeUnixNano\":\"%llu\","
                         "\"endTimeUnixNano\":\"%llu\","
                         "\"status\":{\"code\":%d}}",
                         s->trace_id, s->span_id, s->parent_span_id,
                         (int)(s->name_len < 200 ? s->name_len : 200), s->name ? s->name : "",
                         (unsigned long long)s->start_ns, (unsigned long long)s->end_ns, s->status);
        if (n > 0)
            pos += (size_t)n;
    }

    pos += (size_t)snprintf(buf + pos, buf_size - pos, "]}]}]}");

    *out_json = buf;
    *out_len = pos;
    return HU_OK;
}

/* Cost anomaly detection */

void hu_cost_monitor_init(hu_cost_monitor_t *m, double anomaly_threshold) {
    if (!m)
        return;
    memset(m, 0, sizeof(*m));
    m->anomaly_threshold = anomaly_threshold > 0.0 ? anomaly_threshold : 2.0;
}

void hu_cost_monitor_record(hu_cost_monitor_t *m, double cost) {
    if (!m)
        return;
    m->window[m->window_pos] = cost;
    m->window_pos = (m->window_pos + 1) % HU_COST_WINDOW_SIZE;
    if (m->window_filled < HU_COST_WINDOW_SIZE)
        m->window_filled++;
    m->total_cost += cost;

    double sum = 0.0;
    for (size_t i = 0; i < m->window_filled; i++)
        sum += m->window[i];
    m->baseline_avg = m->window_filled > 0 ? sum / (double)m->window_filled : 0.0;
}

bool hu_cost_monitor_is_anomaly(const hu_cost_monitor_t *m, double cost) {
    if (!m || m->window_filled < 3)
        return false;
    return cost > m->baseline_avg * m->anomaly_threshold;
}

double hu_cost_monitor_baseline(const hu_cost_monitor_t *m) {
    if (!m)
        return 0.0;
    return m->baseline_avg;
}

/* Factuality scoring heuristic */

static bool contains_substr(const char *text, size_t len, const char *sub) {
    size_t slen = strlen(sub);
    if (slen > len)
        return false;
    for (size_t i = 0; i <= len - slen; i++) {
        bool match = true;
        for (size_t j = 0; j < slen && match; j++) {
            char c = text[i + j];
            char s = sub[j];
            if (c >= 'A' && c <= 'Z')
                c += 32;
            if (s >= 'A' && s <= 'Z')
                s += 32;
            if (c != s)
                match = false;
        }
        if (match)
            return true;
    }
    return false;
}

hu_error_t hu_factuality_score_text(const char *text, size_t text_len, hu_factuality_score_t *out) {
    if (!out)
        return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));

    if (!text || text_len == 0) {
        out->confidence = 0.5;
        return HU_OK;
    }

    double score = 0.7;

    out->has_citations = contains_substr(text, text_len, "according to") ||
                         contains_substr(text, text_len, "source:") ||
                         contains_substr(text, text_len, "[1]") ||
                         contains_substr(text, text_len, "http");
    if (out->has_citations)
        score += 0.15;

    out->has_hedging = contains_substr(text, text_len, "i think") ||
                       contains_substr(text, text_len, "possibly") ||
                       contains_substr(text, text_len, "might be") ||
                       contains_substr(text, text_len, "not sure") ||
                       contains_substr(text, text_len, "approximately");
    if (out->has_hedging)
        score -= 0.1;

    out->has_contradictions = contains_substr(text, text_len, "however") &&
                              contains_substr(text, text_len, "but actually");
    if (out->has_contradictions)
        score -= 0.2;

    size_t sentences = 0;
    for (size_t i = 0; i < text_len; i++) {
        if (text[i] == '.' || text[i] == '!' || text[i] == '?')
            sentences++;
    }
    out->claim_count = sentences;

    if (score < 0.0)
        score = 0.0;
    if (score > 1.0)
        score = 1.0;
    out->confidence = score;

    return HU_OK;
}
