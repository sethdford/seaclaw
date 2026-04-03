#ifndef HU_AGENT_APPROVAL_GATE_H
#define HU_AGENT_APPROVAL_GATE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Human-in-the-Loop Approval Gates
 *
 * A durable approval gate that pauses workflow execution until a human
 * approves or rejects a decision. Gate state persists to disk, surviving
 * crash/restart. Integrates with workflow CLI commands.
 */

typedef enum hu_gate_status {
    HU_GATE_PENDING = 0,    /* waiting for human response */
    HU_GATE_APPROVED,       /* human approved */
    HU_GATE_REJECTED,       /* human rejected */
    HU_GATE_TIMED_OUT,      /* timeout expired */
} hu_gate_status_t;

/* A single approval gate */
typedef struct hu_approval_gate {
    char gate_id[64];             /* unique ID */
    char *description;            /* what the gate is asking; owned */
    size_t description_len;
    char *context_json;           /* additional context; owned */
    size_t context_json_len;
    hu_gate_status_t status;
    int64_t created_at;           /* unix timestamp */
    int64_t timeout_at;           /* 0 = no timeout */
    int64_t resolved_at;          /* when resolved; 0 = not yet */
    char *response;               /* human's response text; owned */
    size_t response_len;
} hu_approval_gate_t;

typedef struct hu_gate_manager hu_gate_manager_t;

/* Create a gate manager that persists gates to gates_dir.
 * If HU_IS_TEST, uses a temp directory instead. */
hu_error_t hu_gate_manager_create(hu_allocator_t *alloc, const char *gates_dir,
                                  hu_gate_manager_t **out);

/* Create and persist a new gate. Returns gate_id_out (64-char buffer).
 * timeout_sec: seconds until timeout (0 = no timeout). */
hu_error_t hu_gate_create(hu_gate_manager_t *mgr, hu_allocator_t *alloc, const char *description,
                          size_t description_len, const char *context_json,
                          size_t context_json_len, int64_t timeout_sec, char *gate_id_out);

/* Check gate status (reads from disk — crash-safe). */
hu_error_t hu_gate_check(hu_gate_manager_t *mgr, const char *gate_id, hu_gate_status_t *out_status);

/* Load gate details. Returns a copy; caller must free via hu_gate_free. */
hu_error_t hu_gate_load(hu_gate_manager_t *mgr, hu_allocator_t *alloc, const char *gate_id,
                        hu_approval_gate_t *out);

/* Resolve a gate (approve or reject with optional response). Persists to disk. */
hu_error_t hu_gate_resolve(hu_gate_manager_t *mgr, hu_allocator_t *alloc, const char *gate_id,
                           hu_gate_status_t decision, const char *response, size_t response_len);

/* List pending gates. Returns array; caller must free via alloc->free. */
hu_error_t hu_gate_list_pending(hu_gate_manager_t *mgr, hu_allocator_t *alloc,
                                hu_approval_gate_t **out, size_t *out_count);

/* Free a single gate struct (its owned fields). */
void hu_gate_free(hu_allocator_t *alloc, hu_approval_gate_t *gate);

/* Free an array of gates. */
void hu_gate_free_array(hu_allocator_t *alloc, hu_approval_gate_t *gates, size_t count);

/* Destroy gate manager. */
void hu_gate_manager_destroy(hu_gate_manager_t *mgr, hu_allocator_t *alloc);

/* Get the gates directory (for listing/inspection). */
const char *hu_gate_manager_dir(hu_gate_manager_t *mgr);

/* Status name helper. */
const char *hu_gate_status_name(hu_gate_status_t status);

#endif /* HU_AGENT_APPROVAL_GATE_H */
