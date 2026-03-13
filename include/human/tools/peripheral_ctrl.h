#ifndef HU_TOOLS_PERIPHERAL_CTRL_H
#define HU_TOOLS_PERIPHERAL_CTRL_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/peripheral.h"
#include "human/tool.h"
#include <stddef.h>

hu_error_t hu_peripheral_ctrl_tool_create(hu_allocator_t *alloc, hu_peripheral_t *peripheral,
                                         hu_tool_t *out);

#endif
