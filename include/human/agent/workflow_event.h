#ifndef HU_WORKFLOW_EVENT_H
#define HU_WORKFLOW_EVENT_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Workflow Event Log — Temporal-style durable execution foundation.
 *
 * An append-only JSON Lines event log that records every workflow step,
 * enabling crash-proof replay and recovery. Complements the checkpoint
 * system (per-task snapshots) with a complete audit trail.
 *
 * Events are immutable once written. Sequence numbers are monotonic
 * within a workflow. Idempotency keys prevent duplicate replay.
 */

typedef enum hu_workflow_event_type {
    HU_WF_EVENT_WORKFLOW_STARTED = 0,
    HU_WF_EVENT_WORKFLOW_COMPLETED,
    HU_WF_EVENT_WORKFLOW_FAILED,
    HU_WF_EVENT_STEP_STARTED,
    HU_WF_EVENT_STEP_COMPLETED,
    HU_WF_EVENT_STEP_FAILED,
    HU_WF_EVENT_TOOL_CALLED,
    HU_WF_EVENT_TOOL_RESULT,
    HU_WF_EVENT_HUMAN_GATE_WAITING,
    HU_WF_EVENT_HUMAN_GATE_RESOLVED,
    HU_WF_EVENT_CHECKPOINT_SAVED,
    HU_WF_EVENT_RETRY_ATTEMPTED,
} hu_workflow_event_type_t;

typedef struct hu_workflow_event {
    hu_workflow_event_type_t type;
    uint64_t sequence_num;        /* monotonic within workflow */
    int64_t timestamp;            /* unix timestamp in ms */
    char *workflow_id;
    size_t workflow_id_len;
    char *step_id;                /* HuLa node ID or operation identifier */
    size_t step_id_len;
    char *data_json;              /* event-specific payload (owned) */
    size_t data_json_len;
    char *idempotency_key;        /* for replay dedup (owned) */
    size_t idempotency_key_len;
} hu_workflow_event_t;

typedef struct hu_workflow_event_log hu_workflow_event_log_t;

/* Create event log backed by file at path. Path is created if it doesn't exist.
 * Under HU_IS_TEST, uses temp directory instead. */
hu_error_t hu_workflow_event_log_create(hu_allocator_t *alloc, const char *path,
                                        hu_workflow_event_log_t **out);

/* Append an event to the log. Sequence number is auto-assigned.
 * Event data_json and idempotency_key are copied (not owned by caller). */
hu_error_t hu_workflow_event_log_append(hu_workflow_event_log_t *log, hu_allocator_t *alloc,
                                        const hu_workflow_event_t *event);

/* Replay all events from the log, returned as sorted array by sequence number.
 * Caller owns output array; use hu_workflow_event_free to clean each event. */
hu_error_t hu_workflow_event_log_replay(hu_workflow_event_log_t *log, hu_allocator_t *alloc,
                                        hu_workflow_event_t **out_events, size_t *out_count);

/* Find event by idempotency key. Returns true if found, false otherwise. */
hu_error_t hu_workflow_event_log_find_by_key(hu_workflow_event_log_t *log,
                                              const char *idempotency_key, hu_workflow_event_t *out,
                                              bool *found);

/* Get total event count in log. */
size_t hu_workflow_event_log_count(const hu_workflow_event_log_t *log);

/* Free the event log structure. */
void hu_workflow_event_log_destroy(hu_workflow_event_log_t *log, hu_allocator_t *alloc);

/* Free a single event's owned strings. */
void hu_workflow_event_free(hu_allocator_t *alloc, hu_workflow_event_t *event);

/* Get human-readable name for event type. */
const char *hu_workflow_event_type_name(hu_workflow_event_type_t type);

/* Get current timestamp in milliseconds since epoch. */
int64_t hu_workflow_event_current_timestamp_ms(void);

#endif /* HU_WORKFLOW_EVENT_H */
