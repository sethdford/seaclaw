#ifndef HU_WEBHOOK_TOOLS_H
#define HU_WEBHOOK_TOOLS_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/tool.h"
#include "human/webhook.h"

/**
 * Webhook tools for managing incoming webhook events.
 *
 * webhook_register: Create a new webhook endpoint
 *   Parameters: {"path": "endpoint_path"}
 *   Returns: {"id": "webhook_id"}
 *
 * webhook_poll: Check for events on a webhook
 *   Parameters: {"id": "webhook_id"}
 *   Returns: {"events": [{"data": "...", "received_at": timestamp}]}
 *
 * webhook_list: List all registered webhooks
 *   Parameters: {}
 *   Returns: {"webhooks": [{"id": "...", "path": "...", "registered_at": timestamp}]}
 */

hu_error_t hu_webhook_register_tool_create(hu_allocator_t *alloc, hu_webhook_manager_t *mgr,
                                           hu_tool_t *out);
hu_error_t hu_webhook_poll_tool_create(hu_allocator_t *alloc, hu_webhook_manager_t *mgr,
                                       hu_tool_t *out);
hu_error_t hu_webhook_list_tool_create(hu_allocator_t *alloc, hu_webhook_manager_t *mgr,
                                       hu_tool_t *out);

#endif /* HU_WEBHOOK_TOOLS_H */
