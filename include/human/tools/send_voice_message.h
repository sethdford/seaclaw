#ifndef HU_TOOLS_SEND_VOICE_MESSAGE_H
#define HU_TOOLS_SEND_VOICE_MESSAGE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/tool.h"

hu_error_t hu_send_voice_message_create(hu_allocator_t *alloc, hu_tool_t *out);

#endif /* HU_TOOLS_SEND_VOICE_MESSAGE_H */
