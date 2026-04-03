#ifndef HU_TOOLS_MCP_RESOURCE_TOOLS_H
#define HU_TOOLS_MCP_RESOURCE_TOOLS_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/tool.h"
#include <stddef.h>

/**
 * list_mcp_resources tool: List available resources from MCP servers.
 *
 * @param alloc Allocator for tool context.
 * @param out   Receives the tool handle.
 * @return HU_OK on success.
 */
hu_error_t hu_mcp_resource_list_tool_create(hu_allocator_t *alloc, hu_tool_t *out);

/**
 * read_mcp_resource tool: Read a specific resource from an MCP server.
 *
 * @param alloc Allocator for tool context.
 * @param out   Receives the tool handle.
 * @return HU_OK on success.
 */
hu_error_t hu_mcp_resource_read_tool_create(hu_allocator_t *alloc, hu_tool_t *out);

#endif /* HU_TOOLS_MCP_RESOURCE_TOOLS_H */
