#ifndef HU_INTELLIGENCE_ONLINE_LEARNING_H
#define HU_INTELLIGENCE_ONLINE_LEARNING_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Online Learning Engine — extracts learning signals from every interaction
 * and updates tool preferences, response strategy weights, and behavioral
 * patterns in real-time rather than only at scheduled reflection intervals.
 */

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>

typedef enum hu_signal_type {
    HU_SIGNAL_TOOL_SUCCESS = 0,
    HU_SIGNAL_TOOL_FAILURE,
    HU_SIGNAL_USER_CORRECTION,
    HU_SIGNAL_USER_APPROVAL,
    HU_SIGNAL_LONG_RESPONSE,
    HU_SIGNAL_SHORT_RESPONSE,
} hu_signal_type_t;

typedef struct hu_learning_signal {
    hu_signal_type_t type;
    char context[512];
    size_t context_len;
    char tool_name[128];
    size_t tool_name_len;
    double magnitude; /* 0.0–1.0, strength of signal */
    int64_t timestamp;
} hu_learning_signal_t;

typedef struct hu_strategy_weight {
    char strategy[128];
    size_t strategy_len;
    double weight;
    int64_t updated_at;
} hu_strategy_weight_t;

typedef struct hu_online_learning {
    hu_allocator_t *alloc;
    sqlite3 *db;
    double learning_rate; /* EMA decay factor, default 0.1 */
} hu_online_learning_t;

hu_error_t hu_online_learning_create(hu_allocator_t *alloc, sqlite3 *db,
                                     double learning_rate,
                                     hu_online_learning_t *out);
void hu_online_learning_deinit(hu_online_learning_t *engine);

hu_error_t hu_online_learning_init_tables(hu_online_learning_t *engine);

/* Extract and record a learning signal from an interaction. */
hu_error_t hu_online_learning_record(hu_online_learning_t *engine,
                                     const hu_learning_signal_t *signal);

/* Update strategy weight from accumulated signals. Uses EMA. */
hu_error_t hu_online_learning_update_weight(hu_online_learning_t *engine,
                                            const char *strategy, size_t len,
                                            double new_evidence, int64_t now_ts);

/* Get current strategy weight. Returns 1.0 (neutral) if not found. */
double hu_online_learning_get_weight(hu_online_learning_t *engine,
                                     const char *strategy, size_t len);

/* Get count of signals recorded. */
hu_error_t hu_online_learning_signal_count(hu_online_learning_t *engine,
                                           size_t *out);

/* Build learning context for prompt injection. Caller must free *out. */
hu_error_t hu_online_learning_build_context(hu_online_learning_t *engine,
                                            char **out, size_t *out_len);

/* Compute response quality score from signals. */
double hu_online_learning_response_quality(const hu_learning_signal_t *signals,
                                           size_t count);

const char *hu_signal_type_str(hu_signal_type_t type);

#endif /* HU_ENABLE_SQLITE */
#endif /* HU_INTELLIGENCE_ONLINE_LEARNING_H */
