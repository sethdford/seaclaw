#ifndef HU_MCP_CONTEXT_H
#define HU_MCP_CONTEXT_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * MCP production hardening: identity propagation + adaptive timeout budgets.
 *
 * Addresses three critical gaps identified in arXiv:2603.13417:
 *   1. CABP — Context-Aware Broker Protocol: identity-scoped routing
 *   2. ATBA — Adaptive Timeout Budget Allocation: heterogeneous latency
 *   3. SERF — Structured Error Recovery Framework: machine-readable failures
 */

/* ── Identity context (CABP) ─────────────────────────────────────────── */

typedef struct hu_mcp_identity {
    const char *session_id;
    size_t session_id_len;
    const char *user_id;
    size_t user_id_len;
    const char *agent_id;
    size_t agent_id_len;
    const char *scope; /* e.g., "read", "read-write", "admin" */
    size_t scope_len;
} hu_mcp_identity_t;

/* ── Adaptive timeout budget (ATBA) ──────────────────────────────────── */

typedef struct hu_mcp_timeout_budget {
    uint32_t total_budget_ms; /* total wall time for entire tool sequence */
    uint32_t remaining_ms;    /* updated after each call */
    uint32_t per_call_default_ms;
    uint32_t per_call_min_ms;

    /* Latency tracking for adaptive allocation */
    double avg_latency_ms;
    uint32_t call_count;
    uint32_t timeout_count;
} hu_mcp_timeout_budget_t;

/** Initialize a timeout budget for a tool calling sequence. */
void hu_mcp_timeout_budget_init(hu_mcp_timeout_budget_t *budget, uint32_t total_ms,
                                uint32_t per_call_default_ms);

/** Allocate timeout for the next call, adaptively based on history. */
uint32_t hu_mcp_timeout_budget_allocate(hu_mcp_timeout_budget_t *budget);

/** Record the outcome of a call (latency or timeout). */
void hu_mcp_timeout_budget_record(hu_mcp_timeout_budget_t *budget, uint32_t elapsed_ms,
                                  bool timed_out);

/** Check if there's enough budget remaining for another call. */
bool hu_mcp_timeout_budget_has_remaining(const hu_mcp_timeout_budget_t *budget);

/* ── Structured error recovery (SERF) ────────────────────────────────── */

typedef enum hu_mcp_error_category {
    HU_MCP_ERR_NONE = 0,
    HU_MCP_ERR_TIMEOUT,
    HU_MCP_ERR_AUTH,
    HU_MCP_ERR_NOT_FOUND,
    HU_MCP_ERR_RATE_LIMITED,
    HU_MCP_ERR_SERVER,
    HU_MCP_ERR_INVALID_INPUT,
    HU_MCP_ERR_TRANSIENT,
} hu_mcp_error_category_t;

typedef struct hu_mcp_structured_error {
    hu_mcp_error_category_t category;
    bool retryable;
    uint32_t retry_after_ms; /* 0 = no suggestion */
    char message[256];
} hu_mcp_structured_error_t;

/** Classify a raw error string into a structured error. */
void hu_mcp_error_classify(const char *raw_error, size_t error_len, hu_mcp_structured_error_t *out);

#endif /* HU_MCP_CONTEXT_H */
