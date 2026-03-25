#ifndef HU_AGENT_ACP_BRIDGE_H
#define HU_AGENT_ACP_BRIDGE_H

#include "human/agent/agent_comm.h"
#include "human/agent/mailbox.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>
#include <stdint.h>

/*
 * ACP-to-Mailbox bridge: translates between the structured ACP protocol
 * (agent_comm.h) and the low-level mailbox (mailbox.h).
 *
 * Wraps ACP messages into mailbox payloads with a thin JSON envelope so
 * existing mailbox infrastructure handles routing, while preserving ACP
 * semantics (correlation, priority, deadlines).
 */

/* Send an ACP message through the mailbox system. */
hu_error_t hu_acp_bridge_send(hu_allocator_t *alloc, hu_mailbox_t *mbox,
                              const hu_acp_message_t *msg, uint64_t from_agent, uint64_t to_agent);

/* Receive and decode an ACP message from a mailbox message.
 * Caller owns the returned ACP message fields. */
hu_error_t hu_acp_bridge_recv(hu_allocator_t *alloc, const hu_message_t *mailbox_msg,
                              hu_acp_message_t *out);

/* Map ACP priority to mailbox priority. */
hu_msg_priority_t hu_acp_bridge_map_priority(hu_acp_priority_t acp_prio);

/* Map ACP message type to mailbox message type. */
hu_msg_type_t hu_acp_bridge_map_type(hu_acp_msg_type_t acp_type);

#endif /* HU_AGENT_ACP_BRIDGE_H */
