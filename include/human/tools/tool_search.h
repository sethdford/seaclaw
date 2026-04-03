#ifndef HU_TOOLS_TOOL_SEARCH_H
#define HU_TOOLS_TOOL_SEARCH_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/tool.h"
#include <stddef.h>

/* Create a tool_search tool that searches available tools by name or keyword.
 * The tool iterates over the tools array and matches query against names and descriptions
 * (case-insensitive substring match). Results are returned as JSON array with name,
 * description, and permission level. Limited to 20 results.
 *
 * Pass the full tools array from the agent to tool_search_create; it will reference it
 * (not copy) for searching. */
hu_error_t hu_tool_search_create(hu_allocator_t *alloc, hu_tool_t *tools, size_t tools_count,
                                 hu_tool_t *out);

#endif /* HU_TOOLS_TOOL_SEARCH_H */
