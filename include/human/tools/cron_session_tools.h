#ifndef HU_TOOLS_CRON_SESSION_TOOLS_H
#define HU_TOOLS_CRON_SESSION_TOOLS_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/cron.h"
#include "human/tool.h"
#include <stddef.h>

/**
 * cron_create tool: Create a new scheduled cron job.
 *
 * Parameters:
 *   - cron_expr (string): Cron expression
 *   - prompt (string): Prompt to execute
 *   - recurring (boolean): If true, repeat; if false, one-shot
 *
 * Returns: JSON with id and message fields
 *
 * @param alloc        Allocator for tool context.
 * @param scheduler    Cron scheduler instance.
 * @param out          Receives the tool handle.
 * @return HU_OK on success.
 */
hu_error_t hu_cron_create_session_tool_create(hu_allocator_t *alloc,
                                              hu_cron_scheduler_t *scheduler, hu_tool_t *out);

/**
 * cron_delete tool: Remove a scheduled job by ID.
 *
 * Parameters:
 *   - id (integer): Job ID to delete
 *
 * Returns: { "message": "Job deleted" } or error
 *
 * @param alloc        Allocator for tool context.
 * @param scheduler    Cron scheduler instance.
 * @param out          Receives the tool handle.
 * @return HU_OK on success.
 */
hu_error_t hu_cron_delete_session_tool_create(hu_allocator_t *alloc,
                                              hu_cron_scheduler_t *scheduler, hu_tool_t *out);

/**
 * cron_list tool: List all active scheduled jobs.
 *
 * Parameters: (none)
 *
 * Returns: { "jobs": [ { "id": ..., "cron_expr": ..., "prompt": ..., ... } ] }
 *
 * @param alloc        Allocator for tool context.
 * @param scheduler    Cron scheduler instance.
 * @param out          Receives the tool handle.
 * @return HU_OK on success.
 */
hu_error_t hu_cron_list_session_tool_create(hu_allocator_t *alloc, hu_cron_scheduler_t *scheduler,
                                            hu_tool_t *out);

#endif /* HU_TOOLS_CRON_SESSION_TOOLS_H */
