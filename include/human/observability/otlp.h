#ifndef HU_OBSERVABILITY_OTLP_H
#define HU_OBSERVABILITY_OTLP_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * OpenTelemetry-compatible trace/span export + cost anomaly detection.
 * Provides OTLP JSON serialization for traces and a rolling-window cost monitor.
 */

typedef struct hu_otlp_span {
    char trace_id[33];       /* 16-byte hex */
    char span_id[17];        /* 8-byte hex */
    char parent_span_id[17]; /* empty = root */
    const char *name;
    size_t name_len;
    const char *service_name;
    size_t service_name_len;
    uint64_t start_ns;
    uint64_t end_ns;
    int status; /* 0=unset, 1=ok, 2=error */
    const char *attributes_json;
    size_t attributes_json_len;
} hu_otlp_span_t;

#define HU_OTLP_MAX_SPANS 256

typedef struct hu_otlp_trace {
    hu_otlp_span_t spans[HU_OTLP_MAX_SPANS];
    size_t span_count;
} hu_otlp_trace_t;

void hu_otlp_trace_init(hu_otlp_trace_t *trace);

hu_error_t hu_otlp_span_begin(hu_otlp_trace_t *trace, const char *name, size_t name_len,
                              const char *parent_span_id, hu_otlp_span_t **out);

hu_error_t hu_otlp_span_end(hu_otlp_span_t *span, int status);

hu_error_t hu_otlp_trace_to_json(hu_allocator_t *alloc, const hu_otlp_trace_t *trace,
                                 char **out_json, size_t *out_len);

/* Cost anomaly detection: rolling window monitor */

#define HU_COST_WINDOW_SIZE 64

typedef struct hu_cost_monitor {
    double window[HU_COST_WINDOW_SIZE];
    size_t window_pos;
    size_t window_filled;
    double total_cost;
    double baseline_avg;      /* rolling average */
    double anomaly_threshold; /* multiplier; e.g. 2.0 = alert if 2x baseline */
    size_t anomaly_count;
} hu_cost_monitor_t;

void hu_cost_monitor_init(hu_cost_monitor_t *m, double anomaly_threshold);

void hu_cost_monitor_record(hu_cost_monitor_t *m, double cost);

bool hu_cost_monitor_is_anomaly(const hu_cost_monitor_t *m, double cost);

double hu_cost_monitor_baseline(const hu_cost_monitor_t *m);

/* Factuality scoring (lightweight heuristic) */

typedef struct hu_factuality_score {
    double confidence; /* 0.0-1.0 */
    bool has_citations;
    bool has_hedging; /* "I think", "possibly", etc. */
    bool has_contradictions;
    size_t claim_count;
} hu_factuality_score_t;

hu_error_t hu_factuality_score_text(const char *text, size_t text_len, hu_factuality_score_t *out);

#endif /* HU_OBSERVABILITY_OTLP_H */
