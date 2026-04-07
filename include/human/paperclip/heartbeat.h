#ifndef HU_PAPERCLIP_HEARTBEAT_H
#define HU_PAPERCLIP_HEARTBEAT_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/paperclip/client.h"

hu_error_t hu_paperclip_heartbeat(hu_allocator_t *alloc, int argc, char **argv);

size_t hu_paperclip_build_task_context(char *buf, size_t cap,
                                       const hu_paperclip_task_t *task,
                                       const hu_paperclip_comment_list_t *comments,
                                       const hu_paperclip_client_t *client);

#endif /* HU_PAPERCLIP_HEARTBEAT_H */
