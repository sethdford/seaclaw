#ifndef HU_TOOLS_BFF_MEMORY_H
#define HU_TOOLS_BFF_MEMORY_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/tool.h"

hu_error_t hu_bff_memory_create(hu_allocator_t *alloc, hu_tool_t *out);

#endif
