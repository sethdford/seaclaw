#ifndef SC_CHANNELS_DINGTALK_H
#define SC_CHANNELS_DINGTALK_H

#include "seaclaw/channel.h"
#include "seaclaw/channel_loop.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include <stddef.h>

sc_error_t sc_dingtalk_create(sc_allocator_t *alloc, const char *app_key, size_t app_key_len,
                              const char *app_secret, size_t app_secret_len, sc_channel_t *out);

sc_error_t sc_dingtalk_on_webhook(void *channel_ctx, sc_allocator_t *alloc, const char *body,
                                  size_t body_len);

sc_error_t sc_dingtalk_poll(void *channel_ctx, sc_allocator_t *alloc, sc_channel_loop_msg_t *msgs,
                            size_t max_msgs, size_t *out_count);

void sc_dingtalk_destroy(sc_channel_t *ch);

#endif /* SC_CHANNELS_DINGTALK_H */
