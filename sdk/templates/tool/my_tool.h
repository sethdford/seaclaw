#ifndef SC_MY_TOOL_H
#define SC_MY_TOOL_H

#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/tool.h"
#include <stddef.h>

/**
 * Create a custom tool implementing sc_tool_t.
 *
 * Add to your tool array and pass to sc_agent_from_config:
 *   sc_tool_t tools[N];
 *   sc_my_tool_create(&alloc, &tools[i]);
 *   sc_agent_from_config(&agent, &alloc, provider, tools, N, ...);
 *
 * Or add to src/tools/factory.c in sc_tools_create_default.
 */
sc_error_t sc_my_tool_create(sc_allocator_t *alloc, sc_tool_t *out);

#endif /* SC_MY_TOOL_H */
