#ifndef HU_TOOLS_SKILL_RUN_H
#define HU_TOOLS_SKILL_RUN_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/skillforge.h"
#include "human/tool.h"

hu_error_t hu_skill_run_create(hu_allocator_t *alloc, hu_tool_t *out,
                               hu_skillforge_t *skillforge);

#endif /* HU_TOOLS_SKILL_RUN_H */
