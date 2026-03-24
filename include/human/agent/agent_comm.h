#ifndef HU_AGENT_COMM_H
#define HU_AGENT_COMM_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Agent Communication Protocol (ACP) — structured inter-agent messaging.
 *
 * Provides typed message envelopes for agent-to-agent communication with:
 *   - Sender/receiver identity
 *   - Message type classification
 *   - Structured payloads (request, response, delegation, broadcast)
 *   - Correlation IDs for request-response pairing
 *   - Priority levels for message ordering
 *
 * Inspired by multi-agent coordination research (arXiv:2603.xxxxx).
 */

typedef enum hu_acp_msg_type {
    HU_ACP_REQUEST = 0,   /* ask another agent to do something */
    HU_ACP_RESPONSE,      /* reply to a request */
    HU_ACP_DELEGATE,      /* hand off a sub-goal */
    HU_ACP_BROADCAST,     /* inform all agents */
    HU_ACP_STATUS_UPDATE, /* progress/status notification */
    HU_ACP_CANCEL,        /* cancel a prior request */
} hu_acp_msg_type_t;

typedef enum hu_acp_priority {
    HU_ACP_PRIORITY_LOW = 0,
    HU_ACP_PRIORITY_NORMAL,
    HU_ACP_PRIORITY_HIGH,
    HU_ACP_PRIORITY_URGENT,
} hu_acp_priority_t;

typedef struct hu_acp_message {
    char *id;
    size_t id_len;
    char *correlation_id; /* links request-response pairs */
    size_t correlation_id_len;
    char *sender_id;
    size_t sender_id_len;
    char *receiver_id; /* NULL for broadcast */
    size_t receiver_id_len;
    hu_acp_msg_type_t type;
    hu_acp_priority_t priority;
    char *payload; /* JSON-encoded payload */
    size_t payload_len;
    int64_t timestamp;
    int64_t deadline; /* 0 = no deadline */
} hu_acp_message_t;

typedef struct hu_acp_inbox {
    hu_allocator_t *alloc;
    hu_acp_message_t *messages;
    size_t count;
    size_t capacity;
} hu_acp_inbox_t;

hu_error_t hu_acp_inbox_init(hu_acp_inbox_t *inbox, hu_allocator_t *alloc, size_t initial_cap);
void hu_acp_inbox_deinit(hu_acp_inbox_t *inbox);

/** Create a new ACP message. Caller owns the returned message. */
hu_error_t hu_acp_message_create(hu_allocator_t *alloc, hu_acp_msg_type_t type,
                                 const char *sender_id, size_t sender_len, const char *receiver_id,
                                 size_t receiver_len, const char *payload, size_t payload_len,
                                 hu_acp_message_t *out);

/** Create a response to an existing request (auto-sets correlation_id). */
hu_error_t hu_acp_message_reply(hu_allocator_t *alloc, const hu_acp_message_t *request,
                                const char *payload, size_t payload_len, hu_acp_message_t *out);

/** Push a message into an inbox. Takes ownership of message fields. */
hu_error_t hu_acp_inbox_push(hu_acp_inbox_t *inbox, const hu_acp_message_t *msg);

/** Pop the highest-priority message from inbox. Caller owns the returned message. */
hu_error_t hu_acp_inbox_pop(hu_acp_inbox_t *inbox, hu_acp_message_t *out);

/** Count pending messages (optionally filtered by type). */
size_t hu_acp_inbox_count(const hu_acp_inbox_t *inbox, int type_filter);

/** Free all fields of a message. */
void hu_acp_message_free(hu_allocator_t *alloc, hu_acp_message_t *msg);

/** Get human-readable name for message type. */
const char *hu_acp_msg_type_name(hu_acp_msg_type_t type);

#endif /* HU_AGENT_COMM_H */
