#ifndef HU_TOOLS_TASK_TOOLS_H
#define HU_TOOLS_TASK_TOOLS_H

#include "human/core/allocator.h"
#include "human/task_manager.h"
#include "human/tool.h"

/* Create task_create tool: creates a new task */
hu_tool_t hu_tool_task_create(hu_allocator_t *alloc, hu_task_manager_t *task_manager);

/* Create task_update tool: updates task status */
hu_tool_t hu_tool_task_update(hu_allocator_t *alloc, hu_task_manager_t *task_manager);

/* Create task_list tool: lists all tasks */
hu_tool_t hu_tool_task_list(hu_allocator_t *alloc, hu_task_manager_t *task_manager);

/* Create task_get tool: gets a specific task by ID */
hu_tool_t hu_tool_task_get(hu_allocator_t *alloc, hu_task_manager_t *task_manager);

#endif /* HU_TOOLS_TASK_TOOLS_H */
