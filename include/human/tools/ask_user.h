#ifndef HU_TOOL_ASK_USER_H
#define HU_TOOL_ASK_USER_H

#include "human/core/allocator.h"
#include "human/tool.h"

/* Create ask_user tool. approval_cb may be NULL (non-interactive mode).
 * Returns owned tool with vtable. Caller must call tool.vtable->deinit() when done. */
hu_tool_t hu_tool_ask_user_create(hu_allocator_t *alloc,
                                  bool (*approval_cb)(void *ctx, const char *question,
                                                      size_t question_len, char **out_response));

#endif /* HU_TOOL_ASK_USER_H */
