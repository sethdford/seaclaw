#ifndef SC_CHANNELS_IMAP_H
#define SC_CHANNELS_IMAP_H

#include "seaclaw/channel.h"
#include "seaclaw/channel_loop.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct sc_imap_config {
    const char *imap_host;
    size_t imap_host_len;
    uint16_t imap_port;
    const char *imap_username;
    size_t imap_username_len;
    const char *imap_password;
    size_t imap_password_len;
    const char *imap_folder;
    size_t imap_folder_len;
    bool imap_use_tls;
} sc_imap_config_t;

sc_error_t sc_imap_create(sc_allocator_t *alloc, const sc_imap_config_t *config, sc_channel_t *out);
void sc_imap_destroy(sc_channel_t *ch);
bool sc_imap_is_configured(sc_channel_t *ch);

/** Poll IMAP inbox for new messages. Under SC_IS_TEST returns mock emails from queue. */
sc_error_t sc_imap_poll(void *channel_ctx, sc_allocator_t *alloc, sc_channel_loop_msg_t *msgs,
                        size_t max_msgs, size_t *out_count);

#if SC_IS_TEST
/** Push a mock email to the queue for poll to return. */
sc_error_t sc_imap_test_push_mock(sc_channel_t *ch, const char *session_key, size_t session_key_len,
                                  const char *content, size_t content_len);
#endif

#endif /* SC_CHANNELS_IMAP_H */
