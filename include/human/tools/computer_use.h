#ifndef HU_COMPUTER_USE_H
#define HU_COMPUTER_USE_H
#include "human/tool.h"
typedef enum { HU_CU_SCREENSHOT=0, HU_CU_CLICK, HU_CU_TYPE, HU_CU_SCROLL, HU_CU_NAVIGATE, HU_CU_KEY } hu_computer_use_action_t;
hu_tool_t hu_computer_use_create(hu_allocator_t *alloc);
#endif
