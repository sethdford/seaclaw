#ifndef SC_TOOLS_SEND_MESSAGE_H
#define SC_TOOLS_SEND_MESSAGE_H

#include "seaclaw/agent/mailbox.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/tool.h"

sc_error_t sc_send_message_create(sc_allocator_t *alloc, sc_mailbox_t *mailbox, sc_tool_t *out);

#endif
